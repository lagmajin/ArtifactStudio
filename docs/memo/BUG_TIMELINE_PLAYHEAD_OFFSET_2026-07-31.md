# タイムライン右パネル プレイヘッド描画ずれ 調査メモ

**日付**: 2026-07-31
**現象**: タイムライン右ウィンドウのプレイヘッド（赤い縦線）が ScrubBar より上のバー部分でずれて1本の直線にならない

---

## 1. 構造

```
TimelineRightPanelWidget (QWidget, parentWidget)
├── navigator_   (ArtifactTimelineNavigatorWidget)
├── scrubBar_    (ArtifactTimelineScrubBar)  ← 上部: ルーラー + フレームバー
├── workArea_    (WorkAreaControl)
├── painterTrackView_  (ArtifactTimelineTrackPainterView)
└── playheadOverlay_   (TimelinePlayheadOverlayWidget) ← 透明+赤線描画
```

`TimelinePlayheadOverlayWidget`:
- ScrubBar の下端から右パネル下端までを覆う**透明**な QWidget
- `syncGeometryToPanel()` で geometry を `[0, scrubBar.bottom, panel.width, panel.height - scrubBar.bottom]` に設定
- ScrubBar 自身も内部にプレイヘッドを描画する（L580-587）

## 2. プレイヘッド描画の2系統

### ScrubBar 内のプレイヘッド（ArtifactTimelineScrubBar.cppm L580-587）
```cpp
const int clampedX = std::clamp(currentX, railRect.left(), railRect.right());
TimelinePlayheadDraw::drawPlayhead(
    p, static_cast<qreal>(clampedX), 0.0, static_cast<qreal>(h) - 1.0, false);
```
- `currentX` = `resolveFrameToX(visualFrame, w)` で計算
- ルーラーの ppf（pixels per frame）+ xOffset を考慮した X 座標
- ScrubBar の座標系内で描画

### オーバーレイのプレイヘッド（ArtifactTimelineWidget.cppm L4349-4364）
```cpp
int x = currentPlayheadX();
TimelinePlayheadDraw::drawPlayhead(
    painter, static_cast<qreal>(x), 0.0, static_cast<qreal>(height()) - 1.0, true);
```
- `currentPlayheadX()` で計算

## 3. currentPlayheadX() の算出（L4389-4398）
```cpp
int currentPlayheadX() const {
    const double frame = trackView_->currentFrame();
    const QPoint panelPoint = scrubBar_->mapTo(
        parentWidget(), QPoint(scrubBar_->rulerFrameToX(frame), 0));
    return panelPoint.x() - x();
}
```
1. `scrubBar_->rulerFrameToX(frame)` → ScrubBar の **内部座標系** での X
2. `scrubBar_->mapTo(parentWidget(), ...)` → 右パネル座標系に変換
3. `- x()` → オーバーレイ自身の座標系に変換（オーバーレイの x() = マージン = left margin）

## 4. 疑わしい原因

### 原因A: `trackLeft()` マージンの不整合
- `rulerFrameToX()` 内部 (`resolveFrameToX()`) は `trackLeft(width)` のオフセットを含む
- ScrubBar の画面上での `trackLeft` が右パネルの `contentsMargins().left()` と競合
- 右パネルの `rightPanelLayout->setContentsMargins(0, 0, 0, 0)` だが、上位レイアウトがマージンを持つ可能性

### 原因B: ScrubBar と右パネル間のレイアウトオフセット
- ScrubBar が右パネル内で `x > 0` の位置に配置されている場合
- `currentPlayheadX()` の `mapTo(parentWidget()).x()` は正しいが、ScrubBar の `paintEvent` 内の描画 X がずれる
- 逆に ScrubBar の paintEvent が正しく、オーバーレイ側がずれるパターンも

### 原因C: 線が1本につながらないケース
ScrubBar 自身の `drawPlayhead` がプロパティ `timelineDrawPlayhead` で無効化されている場合：
```cpp
const auto drawPlayheadProperty = property("timelineDrawPlayhead");
if (railRect.width() > 0 &&
    (!drawPlayheadProperty.isValid() || drawPlayheadProperty.toBool())) {
    // draw
}
```
→ `timelineDrawPlayhead = false` の場合、ScrubBar 内には線が描画されず、`TimelinePlayheadOverlayWidget` の線だけが表示される。このときオーバーレイの線は `rulerFrameToX → mapTo → -x()` で計算されるが、ScrubBar の ruler 計算（`resolveFrameToX`）は `trackLeft()` のオフセットを含むため、オーバーレイ側の `currentPlayheadX()` との間に `trackLeft()` 分の差が生じる。

## 5. 検証コマンド
```cpp
// ScrubBar とオーバーレイ間の offset を確認
qDebug() << "scrubBar->rulerFrameToX:" << scrubBar_->rulerFrameToX(frame);
qDebug() << "scrubBar->trackLeft:" << scrubBarImpl->trackLeft(scrubBar_->width());
qDebug() << "panelPoint X:" << panelPoint.x();
qDebug() << "overlay x():" << x();
qDebug() << "final currentPlayheadX:" << panelPoint.x() - x();
```

## 6. 修正方向

### 案1: オーバーレイ側の計算修正
`currentPlayheadX()` で `rulerFrameToX` の代わりに `trackLeft` を考慮するか、オーバーレイも ScrubBar と同じ `resolveFrameToX` を使う。

### 案2: ScrubBar 側の描画を常に有効にする
オーバーレイを廃止し、ScrubBar 内の playhead 線を常に全高で描画するように変更（stickToBottom = true）。

### 案3: オーバーレイのマッピング統一
`rulerFrameToX` を使うのではなく、直接 `ppf * frame - xOffset` で座標を計算し、ScrubBar → パネル → オーバーレイ の3段マッピングを避ける。

---

**キーファイル**:
- `ArtifactTimelineWidget.cppm` L4349-4405: `TimelinePlayheadOverlayWidget`
- `ArtifactTimelineScrubBar.cppm` L414-587: ScrubBar `paintEvent`
- `ArtifactTimelineScrubBar.cppm` L91-121: `frameToX`, `resolveFrameToX`
- `ArtifactTimelineScrubBar.cppm` L406-411: `rulerFrameToX`
- `TimelinePlayheadDraw.hpp`: 共通描画関数
