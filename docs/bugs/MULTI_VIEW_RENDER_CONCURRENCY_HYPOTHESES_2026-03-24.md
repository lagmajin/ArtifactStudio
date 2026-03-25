# 調査メモ: Software / Diligent 複数ビュー同時常駐が重さを増やしている可能性

## 対象

以下 4 ビューが同時に存在する構成を対象にする。

- `Composition Viewer` = `ArtifactCompositionEditor`
- `Composition View (Software)` = `ArtifactSoftwareCompositionTestWidget`
- `Layer View (Diligent)` = `ArtifactRenderLayerEditor`
- `Layer View (Software)` = `ArtifactSoftwareLayerTestWidget`

---

## 結論

`複数レンダラを同時に使っているせいで重い` というより、

- 複数のビューがアプリ起動時から同時生成される
- Diligent 側は少なくとも 1 本が常時 60fps 相当で回る
- software 側はイベントのたびに同期で再合成する
- しかも全ビューが同じ `ArtifactProjectService` / `PlaybackService` を監視している

という構造なので、`同時常駐コストが重さに寄与している` 仮説はかなり強い。

少なくとも現状コードでは、

- `Composition Viewer` だけ
- `Layer View (Diligent)` だけ

を単独で使っているわけではない。

---

## コード上で確認できた事実

### 1. App 起動時に 4 ビューを全部生成している

`Artifact/src/AppMain.cppm`

- `ArtifactCompositionEditor`
- `ArtifactSoftwareCompositionTestWidget`
- `ArtifactRenderLayerEditor`
- `ArtifactSoftwareLayerTestWidget`

を起動時に `new` して dock へ追加している。

さらに以下を `true` にしている。

- `Composition View (Software)`
- `Layer View (Diligent)`
- `Layer View (Software)`

つまり、診断用ビューが opt-in ではなく初期レイアウトへ最初から載っている。

### 2. Composition Viewer は show 時に renderer を初期化し、playback frame で毎回描画する

`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

- `CompositionViewport::showEvent()` で `controller_->initialize()` と `controller_->start()`
- `hideEvent()` で `controller_->stop()`

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `ArtifactPlaybackService::frameChanged` に接続して `renderOneFrame()`
- `projectChanged`
- `currentCompositionChanged`
- `layerChanged`

でも再描画する。

連続 timer は外れているが、再生中や編集イベント中は十分に高頻度で動く。

### 3. Layer View (Diligent) は 16ms timer の render loop を持つ

`Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

- `renderTimer_->setInterval(16)`
- `showEvent()` で `startRenderLoop()`
- `startRenderLoop()` で timer 開始
- timeout ごとに `renderOneFrame()`

しかも `hideEvent()` が見当たらず、`stopRenderLoop()` は `destroy()` か明示 stop 時しか呼ばれない。

timeout 側には `if (!isVisible()) return;` があるので完全な無限描画ではないが、dock/tab の見え方次第で動き続ける余地がある。

### 4. software 2 ビューは timer loop こそないが、イベントたびに同期再合成する

`Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm`

`ArtifactSoftwareCompositionTestWidget` と `ArtifactSoftwareLayerTestWidget` はどちらも:

- `projectChanged`
- `currentCompositionChanged`
- `layerCreated`
- `layerRemoved`
- `layerSelected`
- `resizeEvent`
- `wheelEvent`
- `mouseMoveEvent`

などで `refreshPreview()` を直接呼ぶ。

`refreshPreview()` は最終的に `SoftwareRender::compose(request)` を同期実行して `QLabel` に `QPixmap` を張る。

つまり非 GPU でも、変更のたびに CPU 合成と画像更新を即時実行する。

### 5. software 側は visibility guard を持っていない

`ArtifactSoftwareRenderInspectors.cppm` の `refreshPreview()` には

- `isVisible()`
- `isHidden()`
- active tab 判定

のガードがない。

そのため、dock が見えている限り、前面タブでなくても `projectChanged` 系イベントで仕事をする可能性が高い。

### 6. Diligent 側だけでも render path が複数ある

既存調査の通り、`Composition Viewer` では:

- `CompositionRenderController`
- `ArtifactPreviewCompositionPipeline`
- `TransformGizmo`

など責務が分散している。

したがって「Diligent renderer が 1 個だから軽い」は成り立たず、1 ビュー内でも複数描画経路が残っている。

---

## 仮説の強さ

### 強い

- `Layer View (Diligent)` の 16ms timer は常時負荷源になりうる
- software 2 ビューは `projectChanged` ごとに同期再合成するため、編集のたびに横から CPU を食う
- 4 ビューとも同じ composition / layer 更新を購読しているので、1 回の操作で複数ビューが反応する

### まだ未確定

- tab 非アクティブ時に `isVisible()` が Qt/QADS 上どう評価されるか
- `Composition Viewer` と `Layer View (Diligent)` が GPU 上でどの程度同期 stall を起こしているか
- software ビューの再合成コストが体感の主因か、Diligent 側の present/flush が主因か

---

## 優先度の高い原因候補

### 原因候補 1

`Layer View (Diligent)` がタブ非前面でも render timer を回している。

これが当たりなら、最も分かりやすい常時負荷。

### 原因候補 2

software 2 ビューが `projectChanged` や `layerSelected` のたびに hidden/inactive でも `refreshPreview()` している。

これは「ドラッグ開始や選択直後に重い」とも整合しやすい。

### 原因候補 3

`Composition Viewer` と `Layer View (Diligent)` が別々に GPU context / present / flush 系の処理を走らせ、UI スレッドや GPU 同期を増やしている。

---

## 実測で確認したいこと

1. `Layer View (Diligent)` の timer timeout 回数を visible / hidden / inactive tab 別に出す
2. software 2 ビューの `refreshPreview()` 実行時間と呼び出し回数を出す
3. `projectChanged` 1 回で何ビューが再描画するか数える
4. `Composition Viewer` 単独時と 4 ビュー同時時で `frameMs` を比較する

---

## 暫定判断

この仮説は十分有力。

ただし本質は `複数レンダラ` そのものより、

- 診断用ビューが本番 UI と同時常駐している
- 各ビューが独立に再描画/再合成を行う
- inactive/hidden 時の負荷抑制が弱い

ことにある。

つまり対策の軸は:

- test/diagnostic view を起動時常駐させない
- inactive tab の render/compose を止める
- software preview の refresh を debounce する
- `Layer View (Diligent)` の timer を event-driven 化する

になる。
