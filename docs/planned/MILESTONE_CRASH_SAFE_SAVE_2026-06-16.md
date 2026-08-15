# M-CRASH-1 Crash-safe Save Foundation Milestone

作成日: 2026-06-16
最終更新: 2026-08-15
ステータス: QSaveFile／backup rotation／autosave recovery prompt は実装済み、crash-safe loader／size limit／diagnostics は未完了
対象: `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `Artifact/src/Project/ArtifactProjectSerializer.cppm`,
      `Artifact/src/Project/ArtifactProjectImporter.cppm`,
      `Artifact/src/Service/ArtifactProjectService.cppm`,
      `Artifact/src/Service/ArtifactRevisionService.cppm`,
      `Artifact/src/AutoSave/ArtifactAutoSaveManager.cppm`,
      `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`,
      `ArtifactCore/src/Project/*`,
      `ArtifactCore/src/Utils/File*.ixx`
位置づけ: クラッシュ / 電源断 / 強制終了から **プロジェクトを救出** できる foundation。現状は `Auto-save 3 hit / Crash recovery 6 hit` の最低限のみ。

## Update 2026-08-15

- 現行コードでは通常保存・async 保存の `QSaveFile`、既存 project の backup rotation、revision ledger／snapshot、autosave recovery prompt、save/import validation を確認できる。
- 専用 crash-safe loader、project size limit／巨大データ制御、復元結果 diagnostics、実際の電源断・破損ファイル復旧と性能検証は未完了または未確認。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2
- `docs/planned/MILESTONE_PROJECT_AUTO_SAVE_2026-04-10.md`
- `docs/planned/MILESTONE_CRASH_DIAGNOSTICS_RECOVERY_2026-03-15.md`
- `docs/planned/MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md`
- `docs/planned/MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` (checkpoint との接続)
- `docs/planned/MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md`

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2:

> - Crash-safe load: 0 hit
> - Crash on save (atomic write): 0 hit
> - Auto-recovery on crash: 0 hit
> - Project size limit: 0 hit

Auto-save 機構 (`MILESTONE_PROJECT_AUTO_SAVE_2026-04-10.md`) と Crash recovery (`MILESTONE_CRASH_DIAGNOSTICS_RECOVERY_2026-03-15.md`) はあるが、**save 自体が atomic でない** ため、save 中の crash で プロジェクトファイルが破損する可能性がある。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存資産

- `MILESTONE_PROJECT_AUTO_SAVE_2026-04-10.md` — 定期 auto-save
- `MILESTONE_CRASH_DIAGNOSTICS_RECOVERY_2026-03-15.md` — crash detection
- `MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md` — session ledger
- `ArtifactRevisionService` — 履歴
- `AppMain.cppm:1633` — auto-save 機構

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Atomic write | 0 hit | save 中 crash で破損 |
| Crash-safe load | 0 hit | 破損 project 検出なし |
| Auto-recovery | 0 hit | 起動時の自動復元なし |
| Project size limit | 0 hit | 巨大 project で save 失敗 |
| Backup rotation | 部分的 | `.bak` 不在 |

---

## 3. 設計の柱

### 3.1 Atomic File Writer

`ArtifactCore/src/Utils/AtomicFileWriter.ixx` を新規追加:

```cpp
namespace ArtifactCore {

class AtomicFileWriter {
public:
    // write to tmp file then rename (POSIX / Windows atomic)
    static bool writeAtomic(const QString& targetPath,
                            const QByteArray& data,
                            QString* errorMessage = nullptr);

    // 書込中に backup 作成
    static bool writeAtomicWithBackup(const QString& targetPath,
                                      const QByteArray& data,
                                      int maxBackups = 3,
                                      QString* errorMessage = nullptr);

    // fsync 強制
    static bool flushToDisk(const QString& path);
};

} // namespace ArtifactCore
```

- **write to tmp**: `<target>.tmp` に書き出し
- **fsync**: tmp ファイルに fsync
- **rename**: `QSaveFile::commit()` 風 に atomic rename
- **backup**: 既存 `<target>` を `<target>.bak.1` 〜 `.bak.3` にローテーション

### 3.2 Crash-safe Project Loader

`ArtifactCore/src/Project/CrashSafeProjectLoader.ixx`:

```cpp
class CrashSafeProjectLoader {
public:
    enum class Status {
        OK,
        RecoveredFromBackup,
        RecoveredFromTmp,
        Corrupted,  // 起動不可
    };

    struct Result {
        Status status;
        QJsonObject json;
        QString backupPath;  // 復元元 (あれば)
    };

    static Result load(const QString& projectPath);

    // backup 候補の列挙
    static QStringList enumerateBackups(const QString& projectPath);
};
```

- 読み込み順序:
  1. `<project>.artifact` (主)
  2. `<project>.artifact.bak.1` (最新 backup)
  3. `<project>.artifact.tmp` (書込途中の可能性)
  4. JSON 検証 → OK なら採用
- JSON 破損時は `Corrupted` 状態

### 3.3 Auto-recovery 起動時

`ArtifactProjectManager::openProject` に:

```cpp
class ArtifactProjectManager {
    // 起動時に:
    // 1. projectPath を開く
    // 2. status == Recovered* なら user に通知
    // 3. tmp file があればクリーンアップ
    // 4. backup 候補を列挙して "Recovery..." メニューから選べる
};
```

- 起動時の自動復元は **dialog 表示** (silent に上書きしない)
- user 確認後に現状 / backup / tmp から選択

### 3.4 Project Size Limit

`ArtifactProjectManager` の project JSON に `project.sizeLimitMB` 追加:

- 既定 1024 MB
- 超過時に save を中止し `severity=error` 通知
- 超過直前の `archive` を作成

### 3.5 Backup Rotation

- 起動時に最新 1 件を `.bak.1` に rename
- save 時に `.bak.1` → `.bak.2` → `.bak.3` → 削除
- max 3 件保持
- `ApplicationSettingDialog` から max 件数変更

### 3.6 Auto-save + Crash Recovery 統合

`ArtifactAutoSaveManager` と `CrashSafeProjectLoader` を統合:

- auto-save 中は `AtomicFileWriter` 経由で書き出し
- crash 後の起動時、`tmp` ファイルが残っていれば候補として提示
- `CrashSafeProjectLoader::Status` を `AppDebuggerWidget` に表示

### 3.7 ApplicationSettingDialog

新ページ `Project > Backup` 追加:

- `Max backups` (1〜10)
- `Save interval` (1〜60 min)
- `Project size limit` (MB)
- `Auto-recovery` (on/off)

### 3.8 Project 保存

- `ArtifactProjectManager` の project JSON に `backup.rotation.maxBackups` 追加
- 旧プロジェクトは default (`maxBackups=3`) で開く

### 3.9 不変条件 (Guardrails)

- `ArtifactWidgets` 触らない
- 新規 signal-slot 接続は `projectSaved / projectRecovered` 2 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- backup は `<project>.bak.N` 形式で同 directory (project root 内)
- 起動時の自動復元は **dialog 必須** (silent 上書き禁止)
- 既存 `AutoSaveManager` の API は温存

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `crash.atomic-write-failed` (severity=error, atomic write 失敗)
- `crash.recovered-from-backup` (severity=info, backup から復元)
- `crash.recovered-from-tmp` (severity=info, tmp から復元)
- `crash.size-limit-exceeded` (severity=error, project size 上限超過)
- `crash.corrupted` (severity=error, project file 破損)

---

## 4. フェーズ計画

### Phase 1: AtomicFileWriter (P0, 1 セッション)

- `ArtifactCore/src/Utils/AtomicFileWriter.ixx` 新規
- POSIX / Windows atomic rename
- `QSaveFile` ベースの内部実装
- backup rotation

**Done criteria:**
- write 中の crash で 0 byte 破損
- backup rotation 動作
- 既存 save 動作を破壊しない

### Phase 2: CrashSafeProjectLoader (P0, 1〜2 セッション)

- `CrashSafeProjectLoader::load` 実装
- 4 段階の読み込み順序
- JSON 検証

**Done criteria:**
- 主ファイル破損時に backup から復元
- tmp ファイルが残っていれば候補
- JSON 検証失敗時に `Corrupted`

### Phase 3: Auto-recovery 起動時 (P0, 1 セッション)

- 起動時の `Recovery` dialog
- user 確認後に選択
- tmp クリーンアップ

**Done criteria:**
- 起動時に recovery 候補 dialog 表示
- 復元元を選択可能
- 破損 project が手動復元可能

### Phase 4: Project size limit (P0, 1 セッション)

- `project.sizeLimitMB` 追加
- 超過時に save 中止
- archive 自動作成

**Done criteria:**
- 1 GB 超過で save 中止
- archive 自動作成
- 旧 project が default で動く

### Phase 5: Auto-save 統合 (P0, 1 セッション)

- `AutoSaveManager` を `AtomicFileWriter` 経由に
- save 進捗の diagnostics 統合

**Done criteria:**
- auto-save 中の crash で破損 0
- save 進捗が Problem View に表示

### Phase 6: 設定 UI (P1, 1 セッション)

- `ApplicationSettingDialog` の `Project > Backup` ページ
- `FastSettingsStore` に保存

**Done criteria:**
- dialog から max backups / interval / size limit 変更
- 設定が永続化

### Phase 7: Project 保存 + Diagnostics (P1, 1 セッション)

- project JSON に backup rotation 設定
- Problem View への `crash.*` 健全性 contribution

**Done criteria:**
- project 保存 → 再読込で復元
- `crash.recovered-from-backup` 等が Problem View 表示

### Phase 8: 破損 project recovery wizard (P2, 別 milestone 推奨)

- 破損 project を手動で部分復元
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_PROJECT_RECOVERY_WIZARD_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_PROJECT_AUTO_SAVE_2026-04-10.md` | 並走。atomic write 経由に。 |
| `MILESTONE_CRASH_DIAGNOSTICS_RECOVERY_2026-03-15.md` | crash detection。本 milestone は save 側。 |
| `MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md` | session ledger。並走。 |
| `MILESTONE_RENDER_FARM_DESIGN_2026-06-16.md` | checkpoint。本 milestone の backup rotation と並走。 |
| `MILESTONE_LIGHTWEIGHT_VCS_AND_LAYER_VARIANTS_2026-04-17.md` | lightweight VCS。backup とは別概念。 |

---

## 6. リスクと未解決論点

### 6.1 実装リスク

1. **POSIX / Windows atomic rename 差**。Windows の `MoveFileEx` (MOVEFILE_REPLACE_EXISTING) と POSIX の `rename(2)` の挙動差
2. **disk full 時の挙動**。write 失敗時に rollback
3. **backup 容量**。`maxBackups=10` で 10 倍の容量消費
4. **OS crash 時の tmp 残存**。tmp クリーンアップの冪等性
5. **network drive (NFS / SMB) の atomic rename 保証なし**。warning 通知

### 6.2 設計未解決

- **project size limit** の警告 UI (save 中に通知)
- **archive 形式**。zip 単体か、project + asset 全部か
- **破損 project からの手動復元**。Phase 8 で別途
- **auto-save interval** の dynamic 化 (project size 連動)

### 6.3 サブモジュール境界

- `ArtifactCore/src/Utils/AtomicFileWriter.ixx` を新規追加
- `ArtifactCore/src/Project/CrashSafeProjectLoader.ixx` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` 触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- save 中の crash で 0 byte 破損
- 破損 project 起動時に recovery dialog 表示
- backup rotation が max 3 件
- project size limit 1 GB 超過で save 中止
- 旧 project が default で動作
- Problem View に `crash.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 を正式 milestone に起こした。

## 現行コード監査 (2026-08-15)

- `ArtifactProjectManager::saveToFile()` は保存前に `.bak~1`〜`.bak~3` をローテーションし、exporter 側の `QSaveFile` 経由で project JSON を commit する経路がある。sidecar／revision／workspace／autosave でも `QSaveFile` の利用が確認できる。
- `ArtifactAutoSaveManager` は時刻付き recovery snapshot の作成、保存件数の prune、`hasRecoveryPoint()`／最新 snapshot 読み込みを実装している。起動時には `showRecoveryPrompt()` が Recover／Ignore を提示し、復元 JSON を別ファイルへ出して再ロードする。
- ただし本 milestone が想定する共通 `AtomicFileWriter`／`CrashSafeProjectLoader` は現行コード上で確認できず、主 project の破損時に backup／tmp を JSON 検証して自動選択する loader 契約もない。backup の命名は `.bak~N` で、仕様記載の `.bak.N` と異なる。
- project size limit、save／recovery の `crash.*` Problem View 診断、disk-full／network-drive の明示的扱い、recovery 候補の選択 UI（最新 snapshot 以外）は未確認。save 中断時に元ファイルを保持する範囲は `QSaveFile` の挙動に依存しており、実機 crash 受入れは未実施。

判定: **atomic save の実装基盤、backup rotation、autosave recovery prompt は大きく進展。破損主ファイルの検証付き復元、共通 loader、size limit、Problem View 診断、crash runtime parity は pending。**
