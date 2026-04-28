# コンポジットエディタ UI 更新メカニズム問題レポート (2026-04-25)

**ステータス:** 調査完了・未修正
**症状:** ビューポート操作（パン・ズーム・ギズモドラッグ）時にカーソルが追従せず、フレーム間隔が不規則で描画品質が段階的に変化する

---

## 概要

パフォーマンス不足ではなく、**レンダースケジューリングの設計**に根本的な問題がある。操作中に最低32msのレイテンシが発生し、3つの独立したスロットルが干渉し合ってフレームが不規則に間引かれる。

---

## 問題1: `renderOneFrame()` の強制16ms遅延

### 場所

`ArtifactCompositionRenderController.cppm:3878`

```cpp
const int scheduleDelayMs = impl_->viewportInteracting_ ? 16 : 0;
QTimer::singleShot(scheduleDelayMs, this, [this]() {
    // ... render ...
});
```

### 問題

操作中に `viewportInteracting_` が true になると、すべての `renderOneFrame()` 呼び出しに **強制16msの QTimer::singleShot遅延** が入る。

マウスイベント発生 (T=0) → singleShot(16ms) 待機 → レンダ開始 (T=16ms) → GPUレンダ → present (T≈28-32ms)

**最低レイテンシ: 32ms**（60Hzマウスイベントでも30fps相当に制限される）。

### 修正

```cpp
// Before
const int scheduleDelayMs = impl_->viewportInteracting_ ? 16 : 0;

// After — 操作中は即座にレンダを開始
const int scheduleDelayMs = 0;
```

---

## 問題2: Gizmoドラッグの三重スロットル

### 場所

| 層 | ファイル | 行 | 遅延 |
|---|---------|-----|------|
| A | `ArtifactCompositionEditor.cppm` | 940, 1122 | Editor側 `singleShot(16)` |
| B | `ArtifactCompositionRenderController.cppm` | 3684 | Controller側 `gizmoDragRenderTimer_` (33ms) |
| C | `ArtifactCompositionRenderController.cppm` | 3878 | `renderOneFrame()` 内の `singleShot(16)` |

### 問題

ギズモドラッグ中、3つの独立タイマーが同時に動作する：

1. **Editor** がマウスイベントを受信 → `singleShot(16ms)` で `controller_->renderOneFrame()` を予約
2. **Controller** の `handleMouseMove` → 33ms経過チェック → `renderOneFrame()` を呼ぶ
3. **renderOneFrame()** 内部 → `viewportInteracting_` → さらに `singleShot(16ms)` で遅延

3つのタイマーが異なるクロックで動くため、フレーム間隔が不規則になる（例: 16ms, 48ms, 32ms, 16ms...）。

### 修正

Editor側の `singleShot(16)` を削除。Controller側の `renderOneFrame()` が即時化されれば、Editor側のスロットルは不要。

```cpp
// Before (ArtifactCompositionEditor.cppm:940)
if (!pendingGizmoDragRender_) {
    pendingGizmoDragRender_ = true;
    QTimer::singleShot(16, this, [this]() {
        pendingGizmoDragRender_ = false;
        if (controller_) controller_->renderOneFrame();
    });
}

// After — 直接呼ぶ（renderOneFrame 内のスロットルに任せる）
if (controller_) controller_->renderOneFrame();
```

同様に `ArtifactCompositionRenderController.cppm:3684` の33msスロットルも削除を検討。`renderOneFrame()` 内の `renderScheduled_` が既に重複防止をしているため、外部スロットルは不要。

---

## 問題3: `renderInProgress_` ガードによるフレームドロップ

### 場所

`ArtifactCompositionRenderController.cppm:3868-3898`

```cpp
if (impl_->renderInProgress_) {              // レンダ中の場合
    impl_->renderRescheduleRequested_ = true; // フラグを立てる
    return;                                    // ドロップ
}

// ... renderOneFrameImpl() 完了後 ...
if (impl_->renderRescheduleRequested_) {
    impl_->renderRescheduleRequested_ = false;
    renderOneFrame();  // ← ここで再スケジュール。さらに16ms遅延が入る
}
```

### 問題

レンダ中に新しい要求が来ると **フレームをドロップ** し、レンダ完了後に `renderOneFrame()` を再呼び出し。問題1の遅延が再適用されるため、レンダ時間 > イベント間隔だと **カスケード的に遅延が増える**。

例: レンダ18ms + 再スケジュール16ms = 34ms。次も同様 → フレーム間隔が34msに固定。

### 修正

再スケジュール時に `QTimer::singleShot(0, ...)` を直接使わず、`renderOneFrame()` 経由しないフラグベースの再実行にする：

```cpp
// renderOneFrameImpl 完了直後
if (impl_->renderRescheduleRequested_) {
    impl_->renderRescheduleRequested_ = false;
    // renderOneFrame() 経由せず直接実行（遅延なし）
    impl_->renderScheduled_ = false;
    impl_->renderInProgress_ = true;
    impl_->renderOneFrameImpl(this);
    impl_->renderInProgress_ = false;
}
```

---

## 問題4: 操作中の4xダウンサンプル（ピクセル数 1/16）

### 場所

`ArtifactCompositionRenderController.cppm:1612-1613, 4124-4125`

```cpp
int interactivePreviewDownsampleFloor_ = 4;  // 4x ダウンサンプル

const int effectivePreviewDownsample =
    viewportInteracting_
        ? std::max(previewDownsample_, interactivePreviewDownsampleFloor_)  // 最低4x
        : ...;
```

### 問題

操作中のレンダ解像度が **各軸1/4（ピクセル数1/16）** にダウンサンプルされる。テキストや細い線が潰れ、Diligentのバイリニア拡大でブロックノイズが見える。

さらに `finishViewportInteraction()` が呼ばれた瞬間にフル解像度に戻るため、**不自然な品質ジャンプ** が発生する。

### 修正

```cpp
// Before
int interactivePreviewDownsampleFloor_ = 4;

// After — 2xに緩和（ピクセル数1/4。テキストはまだ読める）
int interactivePreviewDownsampleFloor_ = 2;
```

または、操作中もフル解像度を維持し、GPU負荷が高い場合のみ動的にダウンサンプルする。

---

## 問題5: パン時に毎回フル再合成

### 場所

`ArtifactCompositionRenderController.cppm:2282`

```cpp
void panBy(const QPointF &viewportDelta) {
    impl_->renderer_->panBy(...);
    impl_->invalidateBaseComposite();  // ← 毎回フル再合成
    renderOneFrame();
}
```

### 問題

パン操作ではレンダーターゲットをオフセット blit するだけで済むが、毎回 `invalidateBaseComposite()` が呼ばれ **全レイヤーの再レンダリング** が走る。

### 修正

パン操作では `invalidateBaseComposite()` を呼ばず、前回の accumRTV をオフセット blit するだけにする：

```cpp
void panBy(const QPointF &viewportDelta) {
    impl_->renderer_->panBy(...);
    // invalidateBaseComposite() を呼ばない
    // accumRTV の内容はパン前のまま。次回の renderOneFrameImpl で
    // ビューポートのパンオフセットを適用して blit するだけで再利用可能
    renderOneFrame();
}
```

---

## 修正優先度

| 優先度 | 問題 | 効果 | 工数 |
|--------|------|------|------|
| **最優先** | 1: renderOneFrame() の16ms遅延削除 | レイテンシ半減 | 1行変更 |
| **高** | 2: 三重スロットル統一 | フレーム間隔が規則的になる | 数行削除 |
| **高** | 3: 再スケジュール即時化 | カスケード遅延の解消 | 数行変更 |
| **中** | 4: ダウンサンプル緩和 | 品質ジャンプの軽減 | 1行変更 |
| **中** | 5: パン最適化 | パン時のCPU負荷削減 | 10行程度 |

問題1〜3を修正すれば、操作感は劇的に改善するはず。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---------|-----|------|
| `ArtifactCompositionRenderController.cppm` | 3878 | scheduleDelayMs の条件分岐 |
| `ArtifactCompositionRenderController.cppm` | 3868-3898 | renderInProgress_ ガード + 再スケジュール |
| `ArtifactCompositionRenderController.cppm` | 3684 | gizmoDragRenderTimer_ スロットル |
| `ArtifactCompositionRenderController.cppm` | 4124 | effectivePreviewDownsample 計算 |
| `ArtifactCompositionRenderController.cppm` | 2282 | panBy の invalidateBaseComposite |
| `ArtifactCompositionEditor.cppm` | 940, 1122 | Editor 側 gizmo singleShot(16) |

---

**文書終了**
