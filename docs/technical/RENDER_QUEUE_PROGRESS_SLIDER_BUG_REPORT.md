# レンダーキューマネージャー 進捗率スライダーバグ調査レポート

**作成日:** 2026-03-27  
**最終更新:** 2026-03-27  
**ステータス:** 調査完了  
**関連コンポーネント:** RenderQueueService, RenderQueueManagerWidget

---

## 概要

レンダーキューマネージャーの進捗率表示（％スライダー）において、特定の操作経路で進捗率が正しくリセットされない、または表示が更新されない問題が発生している。

本レポートでは、問題の根本原因をコードレベルで特定し、影響範囲と修正方針を提示する。

---

## 目次

1. [問題の概要](#1-問題の概要)
2. [影響範囲](#2-影響範囲)
3. [技術的詳細](#3-技術的詳細)
4. [バグ経路分析](#4-バグ経路分析)
5. [修正方針](#5-修正方針)
6. [実装計画](#6-実装計画)
7. [関連ドキュメント](#7-関連ドキュメント)

---

## 1. 問題の概要

### 1.1 現象

レンダーキューマネージャーにおいて、以下の現象が報告されている：

- ジョブをリセットして再実行すると、進捗率が 0% に戻らないことがある
- レンダリング中に進捗率が更新されない、またはスキップ表示される
- ジョブが失敗した際に、進捗率が中途半端な値で停止する
- ジョブを複製すると、進捗率が正しく表示されない

### 1.2 影響

- ユーザーがレンダリングの進行状況を正確に把握できない
- 再レンダリング時に「進捗がリセットされていない」という誤解を招く
- 大規模なレンダリングジョブで、進捗表示の信頼性が低下する

---

## 2. 影響範囲

### 2.1 影響を受けるコンポーネント

| コンポーネント | ファイル | 影響度 |
|--------------|---------|--------|
| RenderQueueService | `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 大 |
| RenderQueueManager (Core) | `ArtifactCore/src/Render/RendererQueueManager.cppm` | 中 |
| RenderJobModel | `ArtifactCore/src/Render/RenderJobModel.cppm` | 中 |
| RenderQueueManagerWidget | `ArtifactWidgets/include/Render/RenderQueueManagerWidget.ixx` | 小 |

### 2.2 影響を受けるユーザー操作

| 操作 | 現象 | 頻度 |
|------|------|------|
| ジョブのリセット → 再実行 | 進捗率が 0% に戻らない | 高 |
| レンダリング中の進捗表示 | 更新が遅延・スキップ | 高 |
| ジョブの失敗 | 進捗率が中途半端な値 | 中 |
| ジョブの複製 | 進捗率が表示されない | 中 |
| 複数ジョブの並列実行 | 進捗表示が不安定 | 中 |

---

## 3. 技術的詳細

### 3.1 アーキテクチャ概要

```
┌─────────────────────────────────────────────────────────┐
│                    UI Thread                            │
│  ┌─────────────────────────────────────────────────┐   │
│  │  RenderQueueManagerWidget                       │   │
│  │    └─ QProgressBar (進捗表示)                   │   │
│  └─────────────────────────────────────────────────┘   │
│                         ▲                               │
│                         │ Qt::QueuedConnection         │
│                         │ jobProgressChanged シグナル  │
└─────────────────────────┼───────────────────────────────┘
                          │
┌─────────────────────────┼───────────────────────────────┐
│                  Worker Thread                          │
│  ┌─────────────────────┴───────────────────────────┐   │
│  │  ArtifactRenderQueueService::Impl               │   │
│  │    └─ queueManager.setJobProgress()             │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3.2 データフロー

```
レンダリング中:
  Worker Thread
    ├─ フレームレンダリング完了
    ├─ progress = (rendered / totalFrames) * 100
    ├─ QMetaObject::invokeMethod(Qt::QueuedConnection)
    │
    └─→ UI Thread (イベントキュー)
         └─ setJobProgress(index, progress)
              └─ jobProgressChanged シグナル発火
                   └─ UI 更新 (QProgressBar)
```

### 3.3 問題の根本原因

#### 3.3.1 スレッド間通信の遅延

`Qt::QueuedConnection` を使用しているため、UI スレッドの負荷が高い場合、進捗更新イベントがキューに滞留する。

```cpp
// 現状の実装
QMetaObject::invokeMethod(this, [this, i, progress]() {
    impl_->queueManager.setJobProgress(i, progress);
}, Qt::QueuedConnection);  // ← イベントキューに積まれる
```

#### 3.3.2 mutex 保護の欠如

`jobs` 配列へのアクセスに mutex 保護がないため、スレッド間でデータ競合が発生する可能性がある。

```cpp
// Worker Thread
jobs[index].progress = progress;  // mutex なし

// UI Thread
int p = jobs[index].progress;     // mutex なし → 競合リスク
```

#### 3.3.3 状態更新の順序問題

`setJobCompleted` などで、`status` と `progress` の更新順序が適切でない。

```cpp
// 現状
jobs[index].status = Completed;   // 先に status 変更
jobs[index].progress = 100;       // ここで progress 設定

// UI が status を先に受け取ると、「Completed だが progress < 100」
```

---

## 4. バグ経路分析

### 4.1 バグ一覧

| ID | 問題 | 深刻度 | 状態 |
|----|------|--------|------|
| BUG-001 | `resetJobForRerun` で UI 更新が遅延 | ★★ | 未修正 |
| BUG-002 | `setJobCompleted` の順序問題 | ★★ | 未修正 |
| BUG-003 | ワーカースレッドの進捗更新が遅延 | ★★ | 未修正 |
| BUG-004 | `setJobFailed` で progress がリセットされない | ★ | 未修正 |
| BUG-005 | `duplicateRenderQueueAt` で進捗表示が更新されない | ★ | 未修正 |
| BUG-006 | スレッド競合 (mutex 保護なし) | ★★★ | 未修正 |
| BUG-007 | `handleJobProgressChanged` で 2 重コールバック | ★ | 未修正 |

---

### 4.2 BUG-001: `resetJobForRerun` で UI 更新が遅延

**場所:** `ArtifactRenderQueueService.cppm:889-928`

**現象:**
ジョブをリセットして再実行する際、進捗率が 0% にリセットされるが、UI 表示が追従しない。

**原因:**
```cpp
bool resetJobForRerun(int index) {
    job.status = ArtifactRenderJob::Status::Pending;
    job.progress = 0;  // データは 0 にリセット
    
    if (jobProgressChanged) jobProgressChanged(index, job.progress);
    // ↑ コールバックは発火するが、UI 側が QueuedConnection の場合遅延
}
```

**影響:**
- リセット直後に UI を見ると、進捗率が 0 に戻っていないように見える
- 実際にはデータは正しく 0 になっている

**再現手順:**
1. レンダリングジョブを 50% まで進行させる
2. ジョブをキャンセルまたは完了させる
3. ジョブをリセット（再実行）
4. 即座に進捗率を確認 → 0% でなく 50% のまま表示される

---

### 4.3 BUG-002: `setJobCompleted` の順序問題

**場所:** `ArtifactRenderQueueService.cppm:701-707`

**現象:**
ジョブ完了時に、一瞬「Completed だが progress < 100」という状態が表示される。

**原因:**
```cpp
void setJobCompleted(int index) {
    jobs[index].status = Completed;   // 先に status 変更
    jobs[index].progress = 100;       // ここで progress 設定
    
    if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
    if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
}
```

**影響:**
- UI の描画タイミングで、完了時に 99% で一瞬止まって見える
- 実際には即座に 100% に更新される

---

### 4.4 BUG-003: ワーカースレッドの進捗更新が遅延

**場所:** `ArtifactRenderQueueService.cppm:1844,1928-1930`

**現象:**
レンダリング中に進捗率が更新されない、または完了時に一気に 100% にジャンプする。

**原因:**
```cpp
// 毎フレームこのコードが実行される（例：60 フレーム）
int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);
QMetaObject::invokeMethod(this, [this, i, progress]() {
    impl_->queueManager.setJobProgress(i, progress);
}, Qt::QueuedConnection);  // ← 60 回のイベントがキューに積まれる
```

**影響:**
- UI スレッドがビジー状態だと、進捗更新が追いつかない
- ユーザーに「進捗が見えない」という不満

---

### 4.5 BUG-004: `setJobFailed` で progress がリセットされない

**場所:** `ArtifactRenderQueueService.cppm:710-715`

**現象:**
ジョブが失敗した際に、進捗率が現在の値のままになる。

**原因:**
```cpp
void setJobFailed(int index, const QString& message) {
    jobs[index].status = Failed;
    jobs[index].errorMessage = message;
    // ← progress のリセットなし！
    // ← jobProgressChanged も発火しない
}
```

**影響:**
- 99% で失敗すると、「Failed だが 99%」という状態になる
- ユーザーが「どこまで進んで失敗したのか」分からない

---

### 4.6 BUG-005: `duplicateRenderQueueAt` で進捗表示が更新されない

**場所:** `ArtifactRenderQueueService.cppm:1623-1635`

**現象:**
ジョブを複製した際、複製されたジョブの進捗率が正しく表示されない。

**原因:**
```cpp
void duplicateRenderQueueAt(int index) {
    ArtifactRenderJob copy = source;
    copy.progress = 0;  // 0 にリセット
    
    impl_->queueManager.addJob(copy);
    if (jobAdded) jobAdded(newIndex);
    // ← jobProgressChanged 発火なし
}
```

**影響:**
- 複製ジョブが 0% で表示されない（空欄または不明な値）

---

### 4.7 BUG-006: スレッド競合 (mutex 保護なし)

**場所:** `ArtifactRenderQueueService.cppm` 全体

**現象:**
稀に進捗率が不正な値（負の値、100 を超える値）を表示する。

**原因:**
```cpp
// Worker Thread
impl_->queueManager.setJobProgress(i, progress);  // mutex なし

// UI Thread
int progress = impl_->queueManager.jobProgressAt(index);  // mutex なし
```

**影響:**
- データ競合により、進捗率が不正な値になる
- 最悪の場合、クラッシュの可能性

---

### 4.8 BUG-007: `handleJobProgressChanged` で 2 重コールバック

**場所:** `ArtifactRenderQueueService.cppm:1435-1437`

**現象:**
進捗更新時に UI が 2 回更新され、ちらつきが発生する。

**原因:**
```cpp
void handleJobProgressChanged(int index, int progress) {
    Q_EMIT owner_->jobProgressChanged(index, progress);  // 外部シグナル
    if (jobProgressChanged) jobProgressChanged(index, progress);  // 内部コールバック
    // ← 2 重発火
}
```

**影響:**
- UI が 2 回更新され、パフォーマンス低下
- ちらつきの原因

---

## 5. 修正方針

### 5.1 修正の優先度

| 優先度 | バグ ID | 修正工数 | リスク |
|-------|--------|---------|--------|
| **P0** | BUG-006 | 1h | 低 |
| **P1** | BUG-004 | 15min | 低 |
| **P1** | BUG-005 | 15min | 低 |
| **P2** | BUG-003 | 30min | 中 |
| **P2** | BUG-002 | 15min | 低 |
| **P3** | BUG-007 | 30min | 中 |
| **P3** | BUG-001 | 15min | 低 |

---

### 5.2 詳細な修正内容

#### 5.2.1 P0: BUG-006 (mutex 保護の追加)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
class ArtifactRenderQueueManager {
private:
    mutable std::mutex jobsMutex_;  // ← 追加

public:
    void setJobProgress(int index, int progress) {
        std::lock_guard<std::mutex> lock(jobsMutex_);  // ← 追加
        if (index < 0 || index >= jobs.size()) return;
        jobs[index].progress = std::clamp(progress, 0, 100);
        if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
        if (jobUpdated) jobUpdated(index);
    }
    
    int jobProgressAt(int index) const {
        std::lock_guard<std::mutex> lock(jobsMutex_);  // ← 追加
        if (index < 0 || index >= jobs.size()) return 0;
        return jobs[index].progress;
    }
    
    // 他の jobs アクセスメソッドも同様に mutex 保護
};
```

**テスト項目:**
- 複数ジョブを並列実行中に進捗率を更新してもクラッシュしない
- 進捗率が不正な値（負、100 超）にならない

---

#### 5.2.2 P1: BUG-004 (`setJobFailed` で progress リセット)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
void setJobFailed(int index, const QString& message) {
    if (index < 0 || index >= jobs.size()) return;
    
    jobs[index].status = ArtifactRenderJob::Status::Failed;
    jobs[index].progress = 0;  // ← 追加
    jobs[index].errorMessage = message;
    
    if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
    if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);  // ← 追加
    if (jobUpdated) jobUpdated(index);
}
```

**テスト項目:**
- ジョブ失敗時に進捗率が 0% にリセットされる
- 失敗したジョブを再実行时、0% から始まる

---

#### 5.2.3 P1: BUG-005 (`duplicateRenderQueueAt` で `jobProgressChanged` 発火)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
void duplicateRenderQueueAt(int index)
{
    if (index < 0 || index >= impl_->queueManager.jobCount()) return;
    
    const ArtifactRenderJob source = impl_->queueManager.getJob(index);
    ArtifactRenderJob copy = source;
    copy.id = ArtifactRenderJob::Id::generate();
    copy.status = ArtifactRenderJob::Status::Pending;
    copy.progress = 0;
    copy.errorMessage.clear();
    
    impl_->queueManager.addJob(copy);
    
    const int newIndex = impl_->queueManager.jobCount() - 1;
    if (jobAdded) jobAdded(newIndex);
    if (jobProgressChanged) jobProgressChanged(newIndex, 0);  // ← 追加
}
```

**テスト項目:**
- 複製ジョブが即座に 0% で表示される

---

#### 5.2.4 P2: BUG-003 (進捗更新の間引き)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
// フレームレンダリングループ内
int rendered = ++framesRendered;
int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);

// 前回の進捗を取得
const int lastProgress = impl_->queueManager.jobProgressAt(i);

// 5% 以上変化した場合のみ更新（間引き）
if (std::abs(progress - lastProgress) >= 5) {
    QMetaObject::invokeMethod(this, [this, i, progress]() {
        impl_->queueManager.setJobProgress(i, progress);
    }, Qt::QueuedConnection);
}
```

**テスト項目:**
- 進捗率が 5% 刻みで更新される（0%, 5%, 10%, ...）
- UI のちらつきが減少する
- レンダリング完了時には 100% が表示される

---

#### 5.2.5 P2: BUG-002 (`setJobCompleted` の順序変更)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
void setJobCompleted(int index) {
    if (index < 0 || index >= jobs.size()) return;
    
    jobs[index].progress = 100;  // ← progress を先に設定
    jobs[index].status = ArtifactRenderJob::Status::Completed;
    
    if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
    if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
    if (jobUpdated) jobUpdated(index);
}
```

**テスト項目:**
- ジョブ完了時に 100% が確実に表示される
- 「Completed だが progress < 100」という状態が表示されない

---

#### 5.2.6 P3: BUG-007 (2 重コールバックの整理)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
void handleJobProgressChanged(int index, int progress) {
    // 内部コールバックのみ発火（外部シグナルは不要）
    if (jobProgressChanged) jobProgressChanged(index, progress);
    // Q_EMIT owner_->jobProgressChanged(index, progress);  ← 削除
}
```

**注意:** UI 側が `owner_->jobProgressChanged` を購読している場合、代替手段が必要。

**テスト項目:**
- UI の進捗表示が正しく更新される
- UI のちらつきが減少する

---

#### 5.2.7 P3: BUG-001 (UI 更新の遅延対策)

**修正ファイル:** `ArtifactRenderQueueService.cppm`

**修正内容:**
```cpp
bool resetJobForRerun(int index) {
    // ... 既存の処理 ...
    
    job.status = ArtifactRenderJob::Status::Pending;
    job.progress = 0;
    job.errorMessage.clear();
    
    // UI 更新を確実にするため、DirectConnection で即時発火
    if (jobStatusChanged) jobStatusChanged(index, job.status);
    if (jobProgressChanged) {
        QMetaObject::invokeMethod(this, [this, index]() {
            jobProgressChanged(index, 0);
        }, Qt::DirectConnection);  // ← 即時発火
    }
    if (jobUpdated) jobUpdated(index);
    
    return wasRenderable;
}
```

**テスト項目:**
- リセット直後に進捗率が 0% で表示される

---

## 6. 実装計画

### 6.1 マイルストーン

| 段階 | 内容 | 期間 |
|-----|------|------|
| **Phase 1** | P0 修正 (mutex 保護) | 1h |
| **Phase 2** | P1 修正 (setJobFailed, duplicate) | 30min |
| **Phase 3** | P2 修正 (間引き，順序変更) | 45min |
| **Phase 4** | P3 修正 (2 重コールバック，UI 遅延) | 45min |
| **Phase 5** | テスト・検証 | 1h |
| **合計** | | **4h** |

---

### 6.2 テスト計画

#### 6.2.1 単体テスト

| テスト ID | 内容 | 対象バグ |
|----------|------|---------|
| UT-001 | 複数スレッドからの同時アクセス | BUG-006 |
| UT-002 | ジョブ失敗時の進捗率リセット | BUG-004 |
| UT-003 | ジョブ複製時の進捗率表示 | BUG-005 |
| UT-004 | 進捗更新の間引き | BUG-003 |
| UT-005 | ジョブ完了時の進捗率 100% 表示 | BUG-002 |

#### 6.2.2 結合テスト

| テスト ID | 内容 | 対象バグ |
|----------|------|---------|
| CT-001 | レンダリング中の進捗表示 | BUG-003 |
| CT-002 | ジョブリセット→再実行 | BUG-001 |
| CT-003 | 複数ジョブの並列実行 | BUG-006 |

#### 6.2.3 手動テスト

1. **リセット再実行テスト**
   - 50% までレンダリング → キャンセル → リセット → 0% 表示を確認

2. **失敗ジョブテスト**
   - 意図的に失敗させる → 0% リセットを確認

3. **複製テスト**
   - ジョブを複製 → 0% 表示を確認

4. **並列レンダリングテスト**
   - 複数ジョブを同時実行 → クラッシュしないことを確認

---

### 6.3 リスク管理

| リスク | 影響 | 対策 |
|-------|------|------|
| mutex 保護によるパフォーマンス低下 | 小 | ロック範囲を最小化 |
| 間引きによる進捗表示の不自然さ | 小 | 間引き閾値を調整可能に |
| UI シグナルの削除による副作用 | 中 | 依存箇所を事前に調査 |

---

## 7. 関連ドキュメント

### 7.1 内部ドキュメント

- `docs/bugs/RENDER_SYSTEM_AUDIT_2026-03-27.md` - レンダーシステム包括調査
- `docs/bugs/COMPOSITION_EDITOR_PERFORMANCE_2026-03-26.md` - コンポジットエディタパフォーマンス
- `docs/bugs/CODE_QUALITY_ISSUES_2026-03-25.md` - コード品質問題

### 7.2 外部ドキュメント

- [Qt Documentation: Meta-Object System](https://doc.qt.io/qt-6/metaobjects.html)
- [Qt Documentation: Thread Support](https://doc.qt.io/qt-6/threads.html)
- [C++ Reference: std::mutex](https://en.cppreference.com/w/cpp/thread/mutex)

---

## 付録 A: 用語集

| 用語 | 説明 |
|------|------|
| RenderQueueService | レンダーキューの管理とジョブ実行を担当するサービス |
| RenderQueueManager | レンダージョブのキューを管理するコアコンポーネント |
| RenderQueueManagerWidget | UI 側のレンダーキュー表示ウィジェット |
| jobProgressChanged | ジョブの進捗率変更を通知するシグナル |
| QueuedConnection | Qt のシグナル・スロット接続方式。イベントキューに積んで非同期実行 |
| DirectConnection | Qt のシグナル・スロット接続方式。即時同期実行 |

---

## 付録 B: 変更履歴

| 日付 | バージョン | 変更内容 | 著者 |
|------|----------|---------|------|
| 2026-03-27 | 1.0 | 初版作成 | AI Assistant |

---

## 付録 C: 関連コード抜粋

### C.1 `setJobProgress` (修正前)

```cpp
void setJobProgress(int index, int progress) {
    if (index < 0 || index >= jobs.size()) return;
    jobs[index].progress = std::clamp(progress, 0, 100);
    if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
    if (jobUpdated) jobUpdated(index);
}
```

### C.2 `setJobProgress` (修正後)

```cpp
void setJobProgress(int index, int progress) {
    std::lock_guard<std::mutex> lock(jobsMutex_);  // ← 追加
    if (index < 0 || index >= jobs.size()) return;
    jobs[index].progress = std::clamp(progress, 0, 100);
    if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
    if (jobUpdated) jobUpdated(index);
}
```

---

**文書終了**
