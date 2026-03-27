# レンダーキューマネージャー ％スライダーバグレポート (2026-03-27)

## 症状

レンダーキューマネージャーの進捗率（％スライダー）が、特定の経路でリセットされない、または不正な値を表示する。

---

## 調査結果 (2026-03-27)

### 未修正の問題一覧

| # | 問題 | 深刻度 | 状態 | 場所 |
|---|------|--------|------|------|
| 1 | `resetJobForRerun` で progress=0 リセット後に UI 更新が非同期 | ★★ | **未修正** | `ArtifactRenderQueueService.cppm:923-926` |
| 2 | `setJobCompleted` で progress=100 設定前に status 変更 | ★★ | **未修正** | `ArtifactRenderQueueService.cppm:703-706` |
| 3 | ワーカースレッドの進捗更新が `QueuedConnection` で遅延 | ★★ | **未修正** | `ArtifactRenderQueueService.cppm:1844,1930` |
| 4 | `setJobFailed` で progress がリセットされない | ★ | **未修正** | `ArtifactRenderQueueService.cppm:710-715` |
| 5 | `duplicateRenderQueueAt` で progress=0 リセット | ★ | **未修正** | `ArtifactRenderQueueService.cppm:1632` |
| 6 | UI スレッドとワーカースレッドの競合 (mutex 保護なし) | ★★★ | **未修正** | `ArtifactRenderQueueService.cppm` 全体 |
| 7 | `handleJobProgressChanged` で 2 重コールバック | ★ | **未修正** | `ArtifactRenderQueueService.cppm:1435-1437` |

---

## 詳細分析

### ★★★ 問題 1: `resetJobForRerun` の非同期 UI 更新

**場所:** `ArtifactRenderQueueService.cppm:889-928`

```cpp
bool resetJobForRerun(int index) {
    // ... 出力パスのリネーム処理 ...
    
    job.status = ArtifactRenderJob::Status::Pending;
    job.progress = 0;  // ← ここで 0 にリセット
    job.errorMessage.clear();
    
    if (jobStatusChanged) jobStatusChanged(index, job.status);
    if (jobProgressChanged) jobProgressChanged(index, job.progress);  // ← コールバック発火
    if (jobUpdated) jobUpdated(index);
    
    return wasRenderable;
}
```

**問題:**
- `jobProgressChanged` コールバックは発火するが、**UI 更新が非同期**
- `ArtifactRenderQueueManagerWidget` 側で `jobProgressChanged` シグナルを購読している場合、**QueuedConnection** で遅延
- リセット直後に UI を見ると、進捗率が 0 に戻っていないように見える

**影響:**
- ユーザーが「リセットしたのに％が戻っていない」と誤解
- 実際にはデータは 0 だが、UI 表示が追従しない

---

### ★★ 問題 2: `setJobCompleted` の順序問題

**場所:** `ArtifactRenderQueueService.cppm:701-707`

```cpp
void setJobCompleted(int index) {
    if (index < 0 || index >= jobs.size()) return;
    
    jobs[index].status = ArtifactRenderJob::Status::Completed;  // ← 先に status 変更
    jobs[index].progress = 100;  // ← ここで progress 設定
    
    if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
    if (jobProgressChanged) jobProgressChanged(index, jobs[index].progress);
    if (jobUpdated) jobUpdated(index);
}
```

**問題:**
- `status` 変更 → `progress` 設定の順序
- UI 側で `jobStatusChanged` を先に受け取ると、**「Completed だが progress < 100」** という状態が一瞬表示される可能性

**影響:**
- 稀に「完了なのに 99% で止まっている」ように見える
- 実際には即座に 100% に更新されるが、UI の描画タイミングでちらつき

---

### ★★ 問題 3: ワーカースレッドの進捗更新が `QueuedConnection`

**場所:** `ArtifactRenderQueueService.cppm:1844,1928-1930`

```cpp
// ジョブ開始時
QMetaObject::invokeMethod(this, [this, i]() {
    impl_->queueManager.setJobProgress(i, 0);
}, Qt::QueuedConnection);  // ← QueuedConnection で遅延

// フレームレンダリング中
int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);
QMetaObject::invokeMethod(this, [this, i, progress]() {
    impl_->queueManager.setJobProgress(i, progress);
}, Qt::QueuedConnection);  // ← QueuedConnection で遅延
```

**問題:**
- `Qt::QueuedConnection` は**イベントキューに積まれる**ため、UI スレッドの負荷が高いと遅延
- 60 フレームのレンダリング中、進捗更新が追いつかない可能性

**影響:**
- レンダリング中は 0% で止まり、完了時に一気に 100% にジャンプ
- ユーザーに「進捗が見えない」という不満

---

### ★ 問題 4: `setJobFailed` で progress がリセットされない

**場所:** `ArtifactRenderQueueService.cppm:710-715`

```cpp
void setJobFailed(int index, const QString& message) {
    if (index < 0 || index >= jobs.size()) return;
    
    jobs[index].status = ArtifactRenderJob::Status::Failed;
    jobs[index].errorMessage = message;
    // ← progress のリセットなし！
    
    if (jobStatusChanged) jobStatusChanged(index, jobs[index].status);
    if (jobUpdated) jobUpdated(index);
    // ← jobProgressChanged も発火しない
}
```

**問題:**
- 失敗時に `progress` が現在の値のまま
- 99% で失敗すると、**「Failed だが 99%」** という状態になる

**影響:**
- ユーザーが「どこまで進んで失敗したのか」分からない
- リトライ時に 99% から始まるように見える（実際には 0 から再レンダリング）

---

### ★ 問題 5: `duplicateRenderQueueAt` で progress=0 リセット

**場所:** `ArtifactRenderQueueService.cppm:1623-1635`

```cpp
void duplicateRenderQueueAt(int index)
{
    if (index < 0 || index >= impl_->queueManager.jobCount()) return;
    
    const ArtifactRenderJob source = impl_->queueManager.getJob(index);
    ArtifactRenderJob copy = source;
    copy.id = ArtifactRenderJob::Id::generate();
    copy.status = ArtifactRenderJob::Status::Pending;
    copy.progress = 0;  // ← 0 にリセット
    copy.errorMessage.clear();
    
    impl_->queueManager.addJob(copy);
    // ...
}
```

**問題:**
- 複製時に progress が 0 にリセットされるのは正しい
- しかし、**UI 更新が `jobUpdated` のみ**で `jobProgressChanged` が発火しない

**影響:**
- 複製されたジョブが 0% で表示されない（空欄または不明な値）
- UI 再読み込みで初めて 0% が表示される

---

### ★★★ 問題 6: UI スレッドとワーカースレッドの競合

**場所:** `ArtifactRenderQueueService.cppm` 全体

**問題:**
```cpp
// ワーカースレッド側
impl_->queueManager.setJobProgress(i, progress);  // mutex なし

// UI スレッド側
int progress = impl_->queueManager.jobProgressAt(index);  // mutex なし
```

- `jobs` 配列へのアクセスに **mutex 保護がない**
- ワーカースレッドが `progress` を更新中に、UI スレッドが読み込むと**データ競合**

**影響:**
- 稀に進捗率が不正な値（負の値、100 を超える値）を表示
- クラッシュの可能性（データ破損）

---

### ★ 問題 7: `handleJobProgressChanged` で 2 重コールバック

**場所:** `ArtifactRenderQueueService.cppm:1435-1437`

```cpp
void handleJobProgressChanged(int index, int progress) {
    Q_EMIT owner_->jobProgressChanged(index, progress);  // ← 外部シグナル
    if (jobProgressChanged) jobProgressChanged(index, progress);  // ← 内部コールバック
}
```

**問題:**
- 外部シグナルと内部コールバックの**2 重発火**
- UI 側で `jobProgressChanged` を購読している場合、**同じ進捗が 2 回通知**される

**影響:**
- UI が 2 回更新され、ちらつきの原因
- パフォーマンス低下（不要な更新）

---

## 経路分析

### 経路 A: ジョブリセット → UI 更新遅延

```
resetJobForRerun(index)
  ├─ job.progress = 0
  ├─ jobProgressChanged(index, 0) 発火
  │    └─ handleJobProgressChanged(index, 0)
  │         ├─ Q_EMIT owner_->jobProgressChanged(index, 0)  ← QueuedConnection
  │         └─ jobProgressChanged callback  ← 同期
  └─ UI 更新 (遅延)
```

**問題点:**
- `Q_EMIT` が `Qt::QueuedConnection` の場合、イベントキューに積まれる
- UI スレッドがビジー状態だと、更新が数フレーム遅れる

---

### 経路 B: レンダリング中の進捗更新

```
ワーカースレッド:
  for (int f = startF; f <= endF; ++f) {
    // フレームレンダリング
    int progress = (rendered / totalFrames) * 100;
    
    QMetaObject::invokeMethod(this, [this, i, progress]() {
        impl_->queueManager.setJobProgress(i, progress);  ← UI スレッドで実行
    }, Qt::QueuedConnection);  ← ここが問題
  }
```

**問題点:**
- 60 フレームの場合、60 回の `invokeMethod` がキューに積まれる
- UI スレッドが追いつかないと、進捗がスキップ表示される

---

### 経路 C: ジョブ完了 → 100% 表示

```
setJobCompleted(index)
  ├─ job.status = Completed
  ├─ job.progress = 100
  ├─ jobStatusChanged(index, Completed) 発火
  ├─ jobProgressChanged(index, 100) 発火
  └─ jobUpdated(index) 発火
```

**問題点:**
- 3 つのシグナルが連続発火
- UI 側でそれぞれを別々に処理している場合、3 回の更新が発生

---

## 対策案

### 対策 1: `setJobFailed` で progress を 0 にリセット

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

**効果:** 失敗時に進捗率が 0% にリセットされ、再レンダリング時に分かりやすくなる

---

### 対策 2: `duplicateRenderQueueAt` で `jobProgressChanged` を発火

```cpp
void duplicateRenderQueueAt(int index)
{
    // ...
    copy.progress = 0;
    
    impl_->queueManager.addJob(copy);
    
    const int newIndex = impl_->queueManager.jobCount() - 1;
    if (jobAdded) jobAdded(newIndex);
    if (jobProgressChanged) jobProgressChanged(newIndex, 0);  // ← 追加
}
```

**効果:** 複製ジョブが即座に 0% で表示される

---

### 対策 3: mutex 保護の追加

```cpp
class ArtifactRenderQueueManager {
private:
    mutable std::mutex jobsMutex_;  // ← 追加

public:
    void setJobProgress(int index, int progress) {
        std::lock_guard<std::mutex> lock(jobsMutex_);  // ← 追加
        if (index < 0 || index >= jobs.size()) return;
        jobs[index].progress = std::clamp(progress, 0, 100);
        // ...
    }
    
    int jobProgressAt(int index) const {
        std::lock_guard<std::mutex> lock(jobsMutex_);  // ← 追加
        if (index < 0 || index >= jobs.size()) return 0;
        return jobs[index].progress;
    }
};
```

**効果:** データ競合を防止、クラッシュリスク低減

---

### 対策 4: `Qt::DirectConnection` の使用（要検討）

```cpp
// 現在: Qt::QueuedConnection
QMetaObject::invokeMethod(this, [this, i, progress]() {
    impl_->queueManager.setJobProgress(i, progress);
}, Qt::QueuedConnection);

// 修正: Qt::DirectConnection (ワーカースレッドから直接呼び出し)
QMetaObject::invokeMethod(this, [this, i, progress]() {
    impl_->queueManager.setJobProgress(i, progress);
}, Qt::DirectConnection);
```

**注意点:**
- `setJobProgress` 内で UI 更新を行う場合、`DirectConnection` は NG（別スレッドから UI 操作）
- UI 更新は `QMetaObject::invokeMethod` で再度マーシャリングする必要がある

---

### 対策 5: 進捗更新の間引き

```cpp
// 現在: 毎フレーム更新
int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);
QMetaObject::invokeMethod(this, [this, i, progress]() {
    impl_->queueManager.setJobProgress(i, progress);
}, Qt::QueuedConnection);

// 修正: 5% 刻みで更新
int progress = static_cast<int>((static_cast<float>(rendered) / totalFrames) * 100);
const int lastProgress = impl_->queueManager.jobProgressAt(i);
if (std::abs(progress - lastProgress) >= 5) {  // ← 5% 以上変化した場合のみ
    QMetaObject::invokeMethod(this, [this, i, progress]() {
        impl_->queueManager.setJobProgress(i, progress);
    }, Qt::QueuedConnection);
}
```

**効果:**
- UI 更新回数を 1/5 に削減
- イベントキューの輻輳を緩和

---

## 推奨対応順

| 順序 | 対策 | 効果 | 見積 |
|:---:|------|:----:|:----:|
| **1** | mutex 保護の追加 | ★★★ | 1h |
| **2** | `setJobFailed` で progress リセット | ★★ | 15min |
| **3** | `duplicateRenderQueueAt` で `jobProgressChanged` 発火 | ★★ | 15min |
| **4** | 進捗更新の間引き | ★★ | 30min |
| **5** | `setJobCompleted` の順序変更 (progress → status) | ★ | 15min |
| **6** | 2 重コールバックの整理 | ★ | 30min |

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 694-698 | `setJobProgress` |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 701-707 | `setJobCompleted` |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 710-715 | `setJobFailed` (progress リセットなし) |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 889-928 | `resetJobForRerun` |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 1623-1635 | `duplicateRenderQueueAt` |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 1435-1437 | `handleJobProgressChanged` (2 重発火) |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 1844 | ジョブ開始時の progress=0 更新 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 1928-1930 | フレームごとの進捗更新 |
| `ArtifactCore/src/Render/RenderJobModel.cppm` | 143-145 | `RenderJobModel::setJobProgress` |

---

## 補足: UI ウィジェットの実装不明箇所

`ArtifactRenderQueueManagerWidget` の実装ファイル (`*.cppm`) がプロジェクト内に存在しない。

**可能性:**
1. ビルドシステム外で管理されている
2. 実装がヘッダーファイルにインラインされている
3. 別のリポジトリ/サブモジュールにある

UI 側のシグナル接続 (`connect()`) の実装を確認できないため、正確な影響範囲は不明。
