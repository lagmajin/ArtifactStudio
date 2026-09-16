# Timeline Diligent GPU Surface

**最終更新:** 2026-09-16

**ステータス:** Phase 1 実装済み・Phase 2 部分実装、runtime / backend 実機検証待ち

## Update 2026-09-16 — static/dynamic snapshot lanes

- タイムラインGPU面のsnapshotを、row／grid／clip／keyframe等の静的laneと、playhead／現在フレームの強調だけを含む動的laneに分離した。静的な可視範囲・編集内容が変わらない限り、GUI threadは大きなprimitive配列をコピー・再構築しない。
- Diligent windowは両laneを同じD3D12／Vulkan共通のcommand bufferへ順に記録する。既存の単一snapshot APIはCurve Editorおよび互換用途のため保持する。
- 静的lane更新時はUI側の一時snapshotをDiligent windowへムーブして所有権を一度だけ渡す。UIキャッシュ側に同じ大きなprimitive配列を保持しないため、行・グリッド・クリップ変更時の一時メモリと深いコピーを抑える。
- 静的lane構築時の選択行判定を事前集計し、トラック数×クリップ数の二重走査を避ける。大量レイヤーでの編集・スクラブ時も、描画結果とQtフォールバックの責務は変えない。
- 動的laneの矩形・線・三角形・テキスト配列にboundedな初期容量を設定し、プレイヘッド更新時の段階的な再確保を抑える。
- 動的laneの入力状態（現在フレーム、ズーム、スクロール、viewport、visual revision、ドラッグ中クリップ、キーフレームプレビュー）が前回と同一なら、snapshot生成とGPU通知をスキップする。
- トラック上端の可視座標配列（`trackTops`）を静的lane更新時に構築して保持し、動的lane更新では再利用する。
- `ArtifactTimelineTrackPainterView`の正本`trackTopsView()`をsnapshot builderから参照し、Diligent側で同じトラックgeometryを複製保持しない。
- Diligent描画時の`QColor`→linear `FloatColor`変換をフレーム内の固定長キャッシュで共有し、primitiveごとの重複した色変換を抑える。
- waveformをsnapshot内のimmutable payload（描画矩形・正規化ピーク列・色）として保持し、現行は線分fallbackで描画する。将来のtexture uploadは同じpayloadを直接消費する。
- GPU command再利用や入力hit testの完全移管は未実装。CPU snapshot構築／GPU submit時間の実計測とD3D12／Vulkan runtime検証は許可後に実施する。

## Update 2026-09-16 — native wheel navigation seam

- GPU windowはホイール操作をQt widgetへ`sendEvent`転送せず、位置・delta・modifierだけを明示のnavigation APIへ渡す。縦横スクロール、Ctrlズーム、Ctrl+Altの行高変更は既存の状態更新・EventBus経路を保つ。
- クリップ／キーフレームのhit testとUndo編集は引き続き互換入力モデルであり、次段階の移管対象とする。

## Update 2026-09-16 — direct-manipulation present lane

- Diligent surfaceはアイドル時の33ms Present上限を維持しつつ、互換入力モデルがdrag／scrub／pan／marquee等の操作中である間は16ms上限（60Hz）へ切り替える。
- snapshot更新の16ms間引きと同じ上限に揃え、操作中に33ms Present待ちが追加される状態を除去した。GPU面は依然Diligentのbackend-neutralなD3D12／Vulkan経路であり、Qt版は初期化失敗時のフォールバックとして残す。
- 入力はまだQtの既存interaction modelへ転送している。次段階ではmodelをwidgetから分離し、Diligent側へヒットテストとdrag stateを移す。CPU snapshot構築／GPU submit時間の実計測とD3D12／Vulkan runtime検証は未実施。

## Update 2026-09-13 — snapshot更新のcoalesceとムーブ引き渡し

- `ArtifactTimelineWidget`のGPU snapshot要求をqueued turn単位でcoalesceし、同一UIイベント内の連続refreshから重複した可視範囲配列構築とrender event投稿を抑制した。
- `ArtifactDiligentTimelineRenderWindow`にrvalue snapshot APIを追加し、UI側で構築済みのrect／line／triangle配列を共有snapshotへムーブする。既存のconst参照API、latest-wins世代判定、Diligentのwindow thread限定submit／presentは維持する。
- Phase 2のCPU snapshot構築時間・GPU時間の分離計測、D3D12／Vulkan実機、device loss受入は未検証のまま。ビルド／runtime確認は許可後に実施する。

## Update 2026-08-30 — current implementation reconciliation

- `ArtifactDiligentTimelineRenderWindow` が既存の共有 Diligent device / immediate context を取得し、D3D12 / Vulkan の swap chain を backend-neutral な `PrimitiveRenderer2D`、`RenderCommandBuffer`、`DiligentImmediateSubmitter` 経由で描画する。
- `ArtifactTimelineWidget` は明示 API と `ARTIFACT_GPU_TIMELINE_PREVIEW=1` でのみGPU pageへ切り替える。初期化失敗時は既存のQWidget/QPainter pageへ復帰し、編集・入力の正規経路は移管しない。
- snapshot はmutex保護された latest-wins 交換、render eventはatomic flagでcoalesceされる。スナップショット生成は現状UI threadで行い、可視範囲のrow / clip / keyframeだけを記録する。
- 背景、track row、grid、clip、playhead、keyframeは実装済み。glyph atlas、waveform / thumbnail texture、CPU/GPU時間の分離計測、D3D12・Vulkan各実機での表示・device loss受入は未実施。

## 目的

現在の `ArtifactTimelineTrackPainterView` を削除・置換せず、Diligent Engine の共有デバイス上に表示専用のタイムライン面を並行実装する。D3D12 と Vulkan は Diligent の backend-neutral な描画経路で共通化する。

## 不変条件

- Diligent面を通常の表示経路とし、Qt版はGPU初期化失敗時に即時復帰できるフォールバックとして残す。Qt版の削除は行わない。
- Diligent入力移管の完了までは、既存のQt interaction modelを入力転送先として利用し、Undo/Redoの正規経路を維持する。
- UI状態から immutable な `DiligentTimelineVisualSnapshot` を作り、GPU面は最新の完成済みスナップショットだけを消費する。
- UIワーカースレッドから Diligent の immediate context や swap chain を直接操作しない。`setSnapshot()` はmutex保護された最新snapshotの交換とthread-safeなevent投稿だけを行い、初期化・resize・submit・presentはwindow所有threadに限定する。
- CPU readback、フレームごとの `WaitForIdle`、Qt合成への迂回をホットパスに入れない。
- GPU初期化失敗、software backend、device loss 時は既存タイムラインへ戻せる構造を維持する。

## 構成

1. `ArtifactTimelineWidget` が既存ビューと同じ表示状態からスナップショットを生成する。
2. 独立module `Artifact.Widgets.Timeline.DiligentRenderWindow` の `ArtifactDiligentTimelineRenderWindow` が共有Diligentデバイスと専用swap chainを所有する。
3. `PrimitiveRenderer2D` が矩形、線、三角形を `RenderCommandBuffer` に記録する。
4. `DiligentImmediateSubmitter` がD3D12またはVulkan backendへ送出する。

## 実装段階

### Phase 1: 並行表示面

- 背景、トラック行、時間グリッド、クリップ、再生ヘッド、キーフレームを描画する。
- 明示的なpreview切替時だけGPU面を表示する。
- 公開API `setGpuTimelinePreviewEnabled(true)` または環境変数 `ARTIFACT_GPU_TIMELINE_PREVIEW=1` を明示的なopt-in入口とする。
- 現在のタイムラインは常に生成・保持する。

### Phase 2: 表示品質と負荷制御

- 可視範囲だけをスナップショット化する。
- 矩形batchと更新世代番号を利用し、未変更フレームの再構築を避ける。
- worker生成されたsnapshotをlatest-winsで交換し、未処理のrender eventはatomic flagで1件にcoalesceする。
- GPUタイマーとCPU snapshot構築時間を別々に計測する。

### Phase 3: 機能パリティ

- GPU glyph atlasでラベルを追加する。
- 波形・サムネイルは非同期キャッシュからGPU textureへ供給する。
- 入力移管は表示パリティと安定性の確認後に別マイルストーンで判断する。

## 次期マイルストーン: Diligent Timeline Phase 3 — 表示パリティと入力移管

### スコープ

- `PrimitiveRenderer2D` の既存共通描画経路にglyph atlasを接続し、クリップ名・マーカーコメントをGPU面で表示する。
- 波形・サムネイルを非同期キャッシュからGPU textureへ供給する。UI threadでのreadbackやフレームごとの再生成は行わない。
- GPU面の座標変換と可視範囲を使ったhit-testを追加し、選択・scrub・dragの候補を既存interaction modelへ明示的に渡す。
- D3D12／Vulkanの両backendで表示、リサイズ、device loss後のQt復帰を確認する。

### 非スコープ

- Qt版タイムラインの削除・置換。
- Undo/Redo経路やReactiveEventsの再設計。
- CPU readback、Qt合成、フレーム中の`WaitForIdle`導入。

### 完了条件

- GPU面でラベル、波形、サムネイルが既存Qt面と同じ編集状態に同期する。
- 選択・scrub・dragがGPU面上で入力可能で、既存のUndo/Redo結果と一致する。
- D3D12／Vulkan双方で実機確認を行い、GPU初期化失敗・resize・device loss時にQt面へ復帰できる。
- CPU snapshot構築時間、GPU submit／present時間、入力から表示までの遅延を計測し、基準値を記録する。

### Phase 3 resource境界

- CPU側は既存の正規化済み`waveformPeaks`とasset identityだけをsnapshotへ渡し、音声解析や画像デコードをDiligent window threadで実行しない。
- GPU texture、staging resource、upload fenceは`ArtifactDiligentTimelineRenderWindow::Impl`が所有し、作成・更新・破棄を同一のwindow threadへ限定する。
- 非同期cacheの結果は世代番号付きのimmutable payloadとして交換し、古い波形／サムネイルが新しい静的laneを上書きしないlatest-wins契約を維持する。
- device lossまたはswap chain再生成時はtexture viewを破棄してcache payloadから再uploadし、CPU readbackやQt合成へフォールバックしない。GPU初期化自体が失敗した場合のみ既存Qt面へ復帰する。

### Phase 3 runtime受入マトリクス

- D3D12 × 通常表示／スクロール／scrub／clip drag：ラベル・波形・サムネイルとplayheadが同じフレームで更新され、入力遅延が基準値以内であること。
- Vulkan × 通常表示／resize／device loss復帰：texture view再生成後に古いpayloadが混入せず、失敗時はQt面へ復帰すること。
- GPU初期化失敗 × preview有効化：編集状態・selection・Undo履歴を失わず、Qt面へ即時復帰すること。
- 全条件 × preview無効化：GPU resourceを新規生成せず、既存QPainter面の描画・入力・保存経路が変わらないこと。

## 完了条件

- DX12/Vulkan双方でGPU previewを開閉できる。
- 既存タイムラインへ即時復帰でき、編集状態が失われない。
- スクロール、ズーム、再生ヘッド、選択表示が既存ビューと同期する。
- GPU面を無効化した通常動作に回帰がない。
