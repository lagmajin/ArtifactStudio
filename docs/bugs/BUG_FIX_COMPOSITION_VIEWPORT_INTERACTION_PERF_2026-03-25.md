# Composition Viewport 操作もたつき軽減 (2026-03-25)

## 対象

- `ArtifactCompositionEditor`
- `CompositionRenderController`
- Diligent backend の `Composition Viewer`

## 症状

`Composition Viewer` 上で以下の操作をすると、体感で引っかかりやもたつきが強い。

- ホイールで pan / zoom
- Space + Drag または Middle Drag で pan

特に、入力イベントが密に来る間に毎回フル描画が走るため、操作追従が悪く見える。

## 今回確認した原因

既存メモの通り、Composition View はまだ「軽い viewport transform 更新」ではなく、
操作中もほぼ通常描画と同じ重い経路へ入りやすい。

主に次の条件が重なっていた。

- pan / zoom のたびに `renderOneFrame()` を即時要求する
- スケジュールは `singleShot(0)` なので、操作中は高頻度の再描画になりやすい
- GPU blend path では offscreen RT を毎回作り直す前提で進む
- レイヤーによっては `QImage -> GPU texture` 再利用が弱く、CPU/GPU 両方の負荷が残る

つまり、根本は「カメラ操作だけなのに、描画全体が重いまま毎回走る」こと。

## 今回の変更

### 1. viewport 操作中フラグを controller 側に追加

`CompositionRenderController` に:

- `notifyViewportInteractionActivity()`
- `finishViewportInteraction()`

を追加した。

これにより、wheel / pan drag の間だけ controller が「今は操作中」と判断できる。

### 2. 操作中の再描画を 60fps 相当に間引き

`renderOneFrame()` の `singleShot` delay を:

- 通常時: `0ms`
- viewport 操作中: `16ms`

に切り替えた。

これで入力イベントごとに即時レンダを積むのではなく、操作中は 1 フレーム分まとめて扱う。

### 3. 操作中だけ preview downsample の下限を引き上げ

`previewDownsample_` とは別に、操作中は

- `effectivePreviewDownsample = max(previewDownsample_, 2)`

を使うよう変更した。

つまり user 設定が `Full` でも、viewport 操作中だけ最低 `Half` 相当で描画する。

操作終了後は full quality に戻して 1 フレーム再描画する。

### 4. CompositionEditor から操作開始/終了を明示通知

`CompositionViewport` で次を追加した。

- `wheelEvent()` 開始時に `notifyViewportInteractionActivity()`
- pan 開始時に `notifyViewportInteractionActivity()`
- pan 中 move ごとに `notifyViewportInteractionActivity()`
- pan 終了時に `finishViewportInteraction()`
- Space 離し時に `finishViewportInteraction()`

wheel は離散入力なので、controller 側で 120ms の single-shot timer を使って自動終了する。

## 期待する効果

- pan / zoom 中の render 要求が過密になりにくい
- GPU blend path の offscreen サイズが一時的に下がる
- 体感の drag / wheel 追従が改善する

## まだ残る根本課題

この修正は「操作中の負荷を下げる」もので、描画パスそのものの根治ではない。

引き続き重さの本命候補は残っている。

- CPU rasterizer effect / mask 適用
- `QImage -> GPU texture` の再作成 / 再転送
- 毎フレームの `flush/present`
- pan / zoom でも offscreen 合成全体をやり直す構造

本当に大きく改善するには、将来的に

- viewport transform だけで再利用できる composited result cache
- surface upload の持続化 / dynamic update
- interaction 時の overlay-only / camera-only fast path

のどれかが必要。

## 変更ファイル

- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## 検証メモ

今回はユーザー指示によりビルド未実施。
差分整合性の目視確認まで。
