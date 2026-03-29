# Milestone: アプリケーション自動アップデート (2026-03-28)

**Status:** Not Started
**Goal:** GitHub Releases から最新バージョンを確認し、ダウンロード・インストールを案内する。

---

## 現状

| 機能 | 状態 | 場所 |
|------|------|------|
| バージョン表示 | ⚠️ ハードコード "1.0.0" | `ArtifactHelpMenu.cppm:86` |
| `applicationVersion()` | ⚠️ 実装済みだが未使用 | `ApplicationService.cppm:115` |
| アップデート確認 | ❌ 未実装 | — |
| 自動ダウンロード | ❌ 未実装 | — |
| バージョン番号定義 | ❌ ビルド定数なし | — |

---

## Architecture

```
ArtifactUpdateManager (新規サービス)
  ├── checkForUpdates()              ← GitHub API で最新リリース取得
  ├── compareVersions(local, remote) ← セマンティックバージョン比較
  ├── downloadUpdate(url)            ← インストーラー/zip をダウンロード
  ├── updateAvailable signal         ← UI に通知
  └── updateReady signal             ← ダウンロード完了通知

バージョン管理:
  - ビルド時にバージョン番号を埋め込み (CMakeLists.txt)
  - applicationVersion() が正しいバージョンを返す
  - プロジェクトファイルにバージョンを記録

GitHub Releases API:
  GET https://api.github.com/repos/{owner}/{repo}/releases/latest
  → tag_name, assets[].browser_download_url, body (changelog)
```

---

## Milestone 1: バージョン管理基盤

### Implementation

1. CMakeLists.txt にバージョン定数を追加:
   ```cmake
   set(ARTIFACT_VERSION_MAJOR 0)
   set(ARTIFACT_VERSION_MINOR 9)
   set(ARTIFACT_VERSION_PATCH 0)
   set(ARTIFACT_VERSION "${ARTIFACT_VERSION_MAJOR}.${ARTIFACT_VERSION_MINOR}.${ARTIFACT_VERSION_PATCH}")
   ```

2. ビルド時にバージョンを埋め込み:
   ```cmake
   target_compile_definitions(Artifact PRIVATE
       ARTIFACT_VERSION_STRING="${ARTIFACT_VERSION}"
       ARTIFACT_VERSION_MAJOR=${ARTIFACT_VERSION_MAJOR}
       ARTIFACT_VERSION_MINOR=${ARTIFACT_VERSION_MINOR}
       ARTIFACT_VERSION_PATCH=${ARTIFACT_VERSION_PATCH}
   )
   ```

3. `ApplicationService::applicationVersion()` を修正:
   ```cpp
   QString ApplicationService::applicationVersion() const {
       return QStringLiteral(ARTIFACT_VERSION_STRING);
   }
   ```

4. ヘルプメニューのバージョン表示を修正:
   ```cpp
   QMessageBox::information(this, "Version",
       QString("Artifact Version: %1").arg(ApplicationService::instance()->applicationVersion()));
   ```

### 見積: 1h

---

## Milestone 2: アップデート確認

### Implementation

1. `ArtifactUpdateManager` クラス作成:
   - `checkForUpdates()` — 非同期で GitHub API を呼び出し
   - `QNetworkAccessManager` で HTTP リクエスト
   - レスポンスから `tag_name` と `body` (changelog) を取得

2. バージョン比較:
   - `compareVersions("0.9.0", "0.9.1")` → -1 (remote is newer)
   - セマンティックバージョニング (major.minor.patch)

3. シグナル:
   - `updateAvailable(const QString& version, const QString& changelogUrl, const QString& downloadUrl)`
   - `updateCheckFailed(const QString& error)`
   - `noUpdateAvailable()`

4. 設定:
   - `autoCheckOnStartup` — 起動時に自動確認
   - `checkIntervalDays` — 確認間隔 (1/7/30 日)

### 見積: 4h

---

## Milestone 3: UI

### Implementation

1. ヘルプメニューに項目追加:
   - "Check for Updates..."
   - アップデート確認中のプログレス表示

2. アップデート通知ダイアログ:
   - バージョン番号
   - 変更履歴
   - "Download" / "Skip" / "Later" ボタン

3. 起動時チェック:
   - 設定で有効なら起動後に自動確認
   - 更新があれば通知バナー表示

### 見積: 3h

---

## Milestone 4: ダウンロード (オプション)

### Implementation

1. ダウンロード進行状況:
   - `QNetworkReply` でダウンロード進捗表示
   - 一時ディレクトリに保存

2. インストール案内:
   - Windows: ダウンロードしたインストーラーを起動
   - zip 版: 展開先を案内

3. アプリ再起動:
   - ダウンロード完了後に再起動を案内

### 見積: 3h

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `ArtifactCore/include/Update/UpdateManager.ixx` (新規) | アップデートマネージャ |
| `ArtifactCore/src/Update/UpdateManager.cppm` (新規) | GitHub API + バージョン比較 |
| `Artifact/src/Widgets/Menu/ArtifactHelpMenu.cppm` (拡張) | チェック項目追加 |
| `Artifact/src/Service/ApplicationService.cppm` (修正) | バージョン定数使用 |
| `Artifact/CMakeLists.txt` (修正) | バージョン定数 |

---

## Recommended Order

| 順序 | マイルストーン | 見積 |
|---|---|---|
| 1 | **M1 バージョン管理基盤** | 1h |
| 2 | **M2 アップデート確認** | 4h |
| 3 | **M3 UI** | 3h |
| 4 | **M4 ダウンロード** | 3h (オプション) |

**総見積: ~8h (M4 除く) / ~11h (M4 含む)**
