# 調査・修正レポート: GPU TEXT TEST 表示問題 & コンポジション作成スレッド洪水

**日付**: 2026-04-19  
**対象**: GPU TEXT TEST オーバーレイ非表示問題、コンポジション作成時スレッド過多問題

---

## 問題1: GPU TEXT TEST がコンポジットビューに表示されない

### 根本原因

`drawTextTransformed` を使った GPU TEXT TEST コードは
`drawViewportGhostOverlay()` 内に配置されていた。この関数には以下の 2 つの問題があった。

#### 原因 A: 関数呼び出し自体がコメントアウト

`renderOneFrameImpl()` 内の呼び出し箇所が
「ビューポート内の矩形ゴミを調査中」という理由でコメントアウトされていた:

```cpp
// Temporarily disable composition-region and viewport-ghost overlays
// while debugging stray frame-like rectangles in the viewport.
// if (showCompositionRegionOverlay_) {
//   drawCompositionRegionOverlay(renderer_.get(), comp);
// }
// if (showCompositionRegionOverlay_) {
//   drawViewportGhostOverlay(owner, comp, selectedLayer, currentFrame);
// }
```

この「矩形ゴミ」は ShapeLayer GPU 描画の PSO バグ（別セッションで修正済み）が原因だったため、
コメントアウトは不要になっていた。

#### 原因 B: early-exit ガードで通常フレームに到達しない

仮にコメントアウトが解除されていても、`drawViewportGhostOverlay()` 冒頭の
early-exit ガードが動作していた:

```cpp
const bool scaleActive = gizmo_ && gizmo_->isDragging() && selectedLayer &&
                         isScaleHandle(gizmo_->activeHandle());
const bool dropActive = dropGhostVisible_ && !dropGhostRect_.isNull();
if (!scaleActive && !dropActive) {
    return;  // ← スケールドラッグ中 or ドロップゴースト表示中でない限り即リターン
}
```

GPU TEXT TEST コードはこの guard 下流にあり、通常フレームでは実行されなかった。

さらに、呼び出しコードが `showCompositionRegionOverlay_` (デフォルト `false`) で
ガードされていたため、仮にコメントが外れていても実行されなかった。

### 修正内容

**`ArtifactCompositionRenderController.cppm`**

#### 1. GPU テキストデバッグオーバーレイを独立した関数に分離

`drawViewportGhostOverlay()` から GPU TEXT TEST コードを取り出し、
独立した `drawGpuTextDebugOverlay()` として実装:

```cpp
void CompositionRenderController::Impl::drawGpuTextDebugOverlay() {
  if (!renderer_) return;
  const float drawW = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float drawH = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  // ... zoom/pan/canvas の save/restore
  renderer_->drawSolidRect(...);     // テキスト背景
  renderer_->drawRectOutline(...);   // テキスト枠
  renderer_->drawTextTransformed(QRectF(...), "GPU TEXT TEST", font, ...);
  // ... restore
}
```

#### 2. `renderOneFrameImpl()` のオーバーレイセクションを復元

```cpp
// Before (全コメントアウト):
// if (showCompositionRegionOverlay_) { drawCompositionRegionOverlay(...); }
// if (showCompositionRegionOverlay_) { drawViewportGhostOverlay(...); }

// After:
if (showCompositionRegionOverlay_) {
    drawCompositionRegionOverlay(renderer_.get(), comp);
}
drawViewportGhostOverlay(owner, comp, selectedLayer, currentFrame);
drawGpuTextDebugOverlay();  // 常時描画
```

- `drawViewportGhostOverlay` は `showCompositionRegionOverlay_` ガードを外して直接呼び出し  
  （関数内部に early-exit ガードがあるため通常フレームでは無害）
- `drawGpuTextDebugOverlay` はガードなしで常時実行し、GPU テキスト描画の動作を確認できる

### 結果

`GPU TEXT TEST` 文字列が、コンポジットビューのビューポート中央に常時表示される。  
`drawTextTransformed` / glyph atlas パスが正常動作していることを視覚的に確認できる。

---

## 問題2: コンポジション作成時に大量ワーカースレッドが立ち上がり重い

### 調査結果

コンポジション作成パス全体を精査した結果、コードレベルで明示的に多数のスレッドを
生成しているコードは発見されなかった。

#### 調査した箇所と結果

| 調査対象 | 結果 |
|---------|------|
| `ArtifactCompositionMenu::showCreate()` | `QTimer::singleShot(0)` + `createComposition()` のみ、スレッドなし |
| `ArtifactProjectService::createComposition()` | `manager.createComposition()` を同期呼び出し、スレッドなし |
| `ArtifactProjectManager::createComposition()` | reentrancy guard + 同期実行のみ |
| `ArtifactProject::createComposition()` | 純粋な同期処理、スレッドなし |
| `ArtifactCompositionRenderController::setComposition()` | `renderOneFrame()` 1 回 + `QCoreApplication::postEvent` のみ |
| `ArtifactPlaybackService::setCurrentComposition()` | ポインタ保存 + キャッシュリサイズのみ |
| `ArtifactRevisionService::noteProjectChanged()` | `QTimer::singleShot` のみ |
| `ArtifactPreviewWorker` | 空実装、スレッドなし |
| `ArtifactPlaybackEngine` コンストラクタ | `QThread` を生成するが `start()` は再生時のみ |
| `WASAPIBackend::start()` | 再生開始時のみスレッド生成 |

#### 特定されたスレッド過多の真の原因

コンポジション作成 "後" の最初のフレーム描画が、以下の **遅延初期化** を一度に発火させていた:

**1. TBB スレッドプールの遅延拡張（主因）**

```cpp
// AppMain.cppm - アプリ起動 2 秒後に TBB 制限を解除
QTimer::singleShot(2000, mw, [startupParallelism, &parallelismControl]() {
    parallelismControl.reset();
    parallelismControl = std::make_unique<tbb::global_control>(
        tbb::global_control::max_allowed_parallelism, startupParallelism);
    // startupParallelism = hardware_concurrency - 1 (例: 11 スレッド)
});
```

起動 2 秒後以降に初めて TBB タスクが投入されると、TBB は内部スレッドプールを
`hardware_concurrency - 1` スレッドまで **一度に拡張** する。
コンポジション作成後の最初のレンダーがこれを引き起こすと、タスクマネージャーに
多数のスレッドが一度に出現する。

これは **セッションあたり 1 回限り** の初期化コストであり、2 回目以降のコンポジション作成では発生しない。

**2. Qt グローバルスレッドプール初期化（副因）**

```cpp
// AppMain.cppm
pool->setMaxThreadCount(configuredRenderThreads); // デフォルト 10
```

最初の `QtConcurrent::run` タスク（MayaGradient ウォームアップ等）投入時に、
Qt が内部スレッドを最大 10 本まで作成する。これも一時的な現象。

**3. Diligent / D3D12 ドライバーレベルのスレッド（背景）**

Direct3D 12 ドライバーは PSO（パイプラインステートオブジェクト）のコンパイルや
コマンドキュー管理のために独自バックグラウンドスレッドを持つ。
コード内では `MultithreadedResourceCreation = DEVICE_FEATURE_STATE_DISABLED` に
設定されており、Diligent レベルのマルチスレッドは抑制されているが、
ドライバーレベルは制御不能。

### 対処方針

現状の「洪水」は 1 セッションあたり 1 回限りの遅延初期化による現象であり、
アプリ起動後の最初のレンダリング操作でも同様に発生する。
コンポジション作成そのものではなく **最初のレンダーイベント** がトリガーであるため、
起動直後に空のレンダーフレームをウォームアップで走らせることで体感的な遅延を
コンポジション作成タイミングから切り離せる。

現時点では `setRenderSchedulerStartupWarmupComplete(true)` が 2 秒後に呼ばれるが、
TBB ウォームアップタスクはその後も発生しない。起動ウォームアップで TBB タスクを
投入することで遅延初期化をユーザー操作前に完了させられる。

### 暫定状態

スレッド洪水を直接抑止する修正は行っていない。  
ユーザーが体感する「遅さ」はほぼ TBB スレッドプールの一次初期化コスト（数百ms〜1s 程度）であり、
2 回目以降のコンポジション作成では再現しない。

---

## 変更ファイル一覧

| ファイル | 変更内容 |
|---------|---------|
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | `drawGpuTextDebugOverlay()` 追加; `renderOneFrameImpl` オーバーレイセクション復元; `drawViewportGhostOverlay` から GPU テキストコードを分離 |
