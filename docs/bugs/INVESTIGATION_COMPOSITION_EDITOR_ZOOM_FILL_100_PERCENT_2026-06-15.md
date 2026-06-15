# Investigation Report - Composition Editor Zoom / Fill / 100% Misposition

> **Date**: 2026-06-15
> **Source bug report**: [BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md](./BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md)
> **Scope**: 静的コード検証のみ（ビルド・実機確認なし）

本報告は 2026-05-30 の BUG レポートが挙げた 3 仮説を、実コードで検証した結果をまとめたもの。元 BUG レポートは編集せず、本ファイルを検証結果として参照関係に置く。

---

## 検証サマリ

| # | 仮説 | 結論 | 備考 |
|---|------|------|------|
| 1 | `Fill` は `fit` ではなく `cover` 動作 | **Confirmed** | ArtifactCore 子モジュール |
| 2 | `100%` は physical pixel 基準で DPR 環境でズレる | **Confirmed（報告より重大）** | 単位混在バグ。親リポジトリ内で完結 |
| 3 | 初期 `Fill` と resize debounce のタイミングずれ | **Confirmed（緩和あり）** | ポーリングで回避されているがエッジ残存 |

---

## 仮説 1: `Fill` は cover 動作（Confirmed）

**対象**: `ArtifactCore/src/Transform/ViewportTransformer.cppm`

### `FillCanvasToViewport()` — L140-163

```cpp
float availableW = impl_->viewportSize.x - margin * 2.0f;
float availableH = impl_->viewportSize.y - margin * 2.0f;
...
float zoomW = availableW / impl_->canvasSize.x;
float zoomH = availableH / impl_->canvasSize.y;
impl_->zoom = std::max(zoomW, zoomH);   // L152
```

インラインコメント（L154）にも明記:

> "Center the canvas, then allow the canvas to crop beyond viewport edges."

これは明らかに cover（縁が切れる）動作。全体を収める `fit` 相当は別メソッド `FitCanvasToViewport()`（L118-138, `std::min` @ L127）として存在する。

### 備考
- `ViewportTransformer.cppm` は **ArtifactCore 子モジュール** 配下。AGENTS.md のサブモジュール非改変ルールに注意。
- `ResetView()`（L102-105）は `zoom = 1.0f` をハードコードするだけであり、キャンバス/viewport 比から計算する真の「100%」計算ではない。
- このファイルには **DPR / devicePixelRatio への言及が一切ない**。`GetViewportCB()`（L174-176）はスケール欄を `{1.0f, 1.0f}` 固定で返しており、HiDPI 非対応。

---

## 仮説 2: `100%` の単位混在バグ（Confirmed / 報告より重大）

**対象**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### `zoom100()` — L4971-4981

```cpp
void CompositionRenderController::zoom100() {
  if (impl_->renderer_) {
    impl_->renderer_->setZoom(1.0f);
    // Center the canvas in the viewport at 100% zoom
    const float panX = (impl_->hostWidth_ - impl_->lastCanvasWidth_) * 0.5f;
    const float panY = (impl_->hostHeight_ - impl_->lastCanvasHeight_) * 0.5f;
    impl_->renderer_->setPan(panX, panY);
    ...
  }
}
```

**致命点**: 左辺と右辺の**単位が一致しない**。

| 変数 | 単位 | 根拠 |
|------|------|------|
| `hostWidth_` / `hostHeight_` | **physical px**（logical × DPR） | `initialize()` L3857-3862, `setViewportSize()` L4058-4079 が `* devicePixelRatio_` して格納。後者には "Callers pass logical pixels; convert to physical pixels for the renderer." のコメント明記 |
| `lastCanvasWidth_` / `lastCanvasHeight_` | **composition raw px**（1920×1080 等） | L3573-3574, L6927-6928 で `composition->settings().compositionSize()` をそのまま格納。DPR 適用なし |

DPR 2.0 では `hostWidth_` だけ倍化し、`lastCanvasWidth_` は倍化しないため、centering offset が physical px 系で計算されてしまう。報告の「ズレやすい」レベルではなく、**ほぼ確定バグ**。

### 範囲の好ましさ
親リポジトリ `Artifact/src/...` 内に閉じる。子モジュールに触れずに修正可能で、かつ最小範囲（`zoom100()` 1 箇所の単位統一）で完結する。

---

## 仮説 3: 初期 `Fill` × resize debounce のタイミング（Confirmed / 緩和あり）

**対象**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

### `scheduleInitialFit()` — L2823-2852

- `resizePending_` が立っている間は 50ms ポーリングで待つ
- 初期化未完・非表示・viewport ≤64px でも再スケジュール
- 最終的に `controller_->zoomFill()`（※`zoomFit()` でなく `zoomFill()`）を呼ぶ

### `resizeDebounceTimer_` — ctor L641-658 / `resizeEvent` L1923-1947

- `resizeEvent` は即座に `setViewportSize`（logical px）を呼んだ上で、160ms の `resizeDebounceTimer_` を再始動
- debounce timeout 本体は final size で `setViewportSize` / `recreateSwapChain` を適用後、`resizePending_ = false` し、`pendingInitialFit_` が残っていれば 50ms 後に `scheduleInitialFit()` を再スケジュール

### 評価
機構自体は正しく、連続 resize 中の古い viewport 参照は回避されている。ただし QADS `showEvent` の順序依存や、ウィンドウ未展開（≤64px）状態での長期ポーリングなど、エッジケースは残りうる。報告の「dock / QADS 再表示直後に違和感が出やすい」はこの残存リスクに対応する。

---

## 副次発見: 命名・ラベル・接続先の三者不一致

**対象**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

```cpp
// L4251-4252
impl_->zoomFitAction_ = impl_->topToolbar_->addAction("Fill");
impl_->zoom100Action_ = impl_->topToolbar_->addAction("100%");

// L4818-4821
QObject::connect(impl_->zoomFitAction_, &QAction::triggered, this,
                 &ArtifactCompositionEditor::zoomFill);
```

- 変数名: `zoomFitAction_`
- 表示ラベル: `"Fill"`
- 接続先: `zoomFill()`

三者が別方向を指しており、実装者にもユーザーにも Fit / Fill の区別を曖昧にする要因。`zoomFit()` 自体はコマンドパレット / コンテキストメニュー（L898, L1692, L2895）からのみ呼ばれ、ツールバーの "Fill" ボタンからは到達しない。

---

## 実装時の方向性（記録用、未確定）

以下はユーザー意向として控えておくメモ。本ファイル作成時点では実装しない。

1. **Fill の意味を Fit（全体を収める）に変更**
   - `FillCanvasToViewport()` を `FitCanvasToViewport()` 相当に切り替える、または接続先を差し替える
   - いずれも **ArtifactCore 子モジュール修正が必要**。AGENTS.md のサブモジュール運用（fork / パッチ提案）を経る必要がある

2. **100% の単位修正**
   - `zoom100()` で `hostWidth_/lastCanvasWidth_` を同じ単位に揃える
   - 親リポジトリ内で完結、最小範囲

3. **命名整理**
   - `zoomFitAction_` → `zoomFillAction_` へのリネーム、またはラベルを "Cover" 表記にする等
   - 親リポジトリ内で完結

---

## 関連ファイル

- [BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md](./BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md) — 元 BUG レポート（仮説段階）
- `ArtifactCore/src/Transform/ViewportTransformer.cppm` — 仮説 1
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — 仮説 2
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` — 仮説 3, 副次発見
