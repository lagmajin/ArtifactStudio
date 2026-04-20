# Widget Map

このファイルは、Artifact の主要ウィジェットの表示名、コード上の名前、役割を AI と人間の両方がすぐ確認できるようにするための一覧です。

## Main Window

- `ArtifactMainWindow`
  アプリ全体のメインウィンドウ。dock 配置、タブ化、ステータス表示をまとめる。
- `DockStyleManager`
  QADS のアクティブタブ装飾や glow を管理する。

## Project / Inspector

- `ArtifactProjectManagerWidget`
  左側の Project パネル。コンポジションやアセットの一覧管理。
- `ArtifactAssetBrowser`
  左側の Asset Browser。ファイル探索、サムネイル、favorites、recent sources、Project View への選択同期を担当する。
- `ArtifactInspectorWidget`
  右側の Inspector パネル。現在のコンポジション / レイヤー / エフェクト情報を編集する。
- `ArtifactCompositionAudioMixerWidget`
  現在コンポジションの音声レイヤー用ミキサー。

## Composition Viewer

- `ArtifactContentsViewer`
  画像 / 動画 / 音声 / 3D model / source-final-compare を横断する閲覧 surface。比較・履歴・inspection はここで扱うが、composition 編集本体ではない。
- `ArtifactCompositionEditor`
  コンポジションビューア本体。再生、停止、ズーム、フィットなどを持つ。

## AI / Assistant

- `ArtifactAICloudWidget`
  Cloud AI の会話 UI。右側で送信や会話履歴を扱い、左側には接続設定・ツール・MCP の詳細をまとめる。既定では左ペインを隠し、必要時だけ表示する。

## Responsibility Boundaries

- `ArtifactContentsViewer`
  内容閲覧と比較の担当。composition editor の代替ではない。
- `ArtifactAssetBrowser`
  ファイル探索とプロジェクト素材の参照窓口。Project View との selection sync までは担当するが、composition 編集責務は持たない。
- `ArtifactCompositionEditor`
  composition 編集と viewport 操作の担当。Contents Viewer の比較導線とは別責務。
- `ArtifactTimelineWidget`
  タイムライン全体の orchestration 担当。左ペインの `ArtifactLayerPanelWidget` と右ペインの track 表示を束ねる。
- `ArtifactLayerPanelWidget`
  タイムライン左ペインの担当。レイヤー列と行操作に限定し、composition/preview の責務は持たない。
- `ArtifactPropertyWidget` / `PropertyEditor`
  property row と編集 UI の担当。Inspector は summary / selection / effect stack の窓口で、row chrome はここへ寄せる。
- `ArtifactRenderLayerWidgetv2`
  layer editor view wrapper。内部の実描画と widget shell を分けて扱う。

## Timeline

- `ArtifactTimelineWidget`
  タイムライン全体を束ねる親ウィジェット。左ペイン、右ペイン、同期、playhead をまとめる。
- `ArtifactLayerPanelWidget`
  タイムライン左ペイン。レイヤー名、親、ブレンド列などを持つ独自ツリーパネル。
- `ArtifactTimelineNavigatorWidget`
  タイムライン右上の細いバー。表示範囲を調整するタイムナビゲーター。
- `ArtifactTimelineScrubBar`
  タイムナビゲーターの下にある RAM preview cache の可視化バー。緑色でキャッシュ済み範囲を示す。旧スクラブ用途は現在は使わない。
- `ArtifactWorkAreaControlWidget`
  ワークエリアの IN/OUT 範囲を編集するバー。
- `TimelineTrackView`
  タイムライン右下の本体。クリップアイテム、赤いシークバー、行グリッドを表示する。
- `TimelinePlayheadOverlay`
  右ペイン上に重なる赤い縦棒のオーバーレイ。

## Render / Queue

- `RenderQueueManagerWidget`
  レンダーキュー管理パネル。ジョブ一覧、出力先、フレーム範囲、詳細設定ダイアログ導線を持つ。
- `ArtifactRenderCenterWindow`
  Render Queue を独立ウィンドウとして扱うトップレベル画面。現状は `RenderQueueManagerWidget` を内包するシェル。
- `ArtifactRenderOutputSettingDialog`
  コーデック、解像度、fps、ビットレートなどの詳細出力設定ダイアログ。

## Debug / Diagnostics

- `ArtifactDebugConsoleWidget`
  低コストな診断テキストの受け皿。frame summary、queue summary、error report の fallback 表示を担当する。
- `ProfilerPanelWidget`
  パフォーマンス要約と補助トレースの表示面。frame debug の timing summary を載せる候補。
- `FrameDebugViewWidget`
  1 フレーム固定、pass / resource / attachment の検査、compare / step / export を扱う内蔵フレームデバッグ面。`App Internal Debugger` の frame タブに対応する。
- `FrameDebugDock`
  `FrameDebugViewWidget` を dock 化したもの。既存の app debugger surface から開ける前提。
- `FrameDebugController`
  capture / compare / bundle export の状態をまとめる制御層。表示責務は持たない。

## Software Render Test

- `ArtifactSoftwareRenderTestWidget`
  既存のソフトウェアレンダリング診断ウィジェット。3D キューブと画像オーバーレイの合成確認用。
- `ArtifactSoftwareCompositionTestWidget`
  現在または選択したコンポジションをソフトウェア合成で簡易プレビューする検証ウィジェット。
- `ArtifactSoftwareLayerTestWidget`
  現在または選択したレイヤーを単体で検証するソフトウェアレンダリング用ウィジェット。blend / opacity / offset / scale / rotation を sandbox で触る。

## Naming Notes

- 赤い縦棒
  UI 用語では `シークバー`。コード上では主に `playhead` として扱う。
- タイムライン右上の細いバー
  UI 用語では `タイムナビゲーター`。
- タイムナビゲーターの下の横棒
  UI 用語では `キャッシュバー`。旧名は `スクラブバー`。
- 太い範囲バー
  UI 用語では `ワークエリアバー`。実体は `ArtifactWorkAreaControlWidget`。
- 右下の編集面
  UI 用語では `タイムライン本体` または `レイヤー編集領域`。コード上では `TimelineTrackView`。

## Timeline Range Notes

- `time.inPoint` / `time.outPoint`
  タイムライン上でのレイヤーの有効範囲。右側のバー編集はこの 2 つを書き換える。
- `time.startTime`
  レイヤー内容のソース側オフセット。タイムライン上の表示可否そのものではなく、必要に応じて開始位置の調整に使う。
- 右側のクリップ移動は `inPoint/outPoint` の移動、端のリサイズは `inPoint/outPoint` のトリムとして扱う。
- `isActiveAt()`
  現在は `inPoint <= frame < outPoint` で判定する。描画経路はこの判定に従う前提。

## Primary Entry Points

- `Artifact/src/AppMain.cppm`
  主要 dock / widget の生成と接続。
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
  タイムライン構造と相互同期の中枢。
- `Artifact/src/Widgets/Menu/ArtifactTestMenu.cppm`
  テスト系ウィジェットの起動メニュー。

## Responsibility Boundaries Addendum

- `ArtifactDebugConsoleWidget`
  テキストベースの診断窓口。詳細な frame inspection そのものは持たない。
- `ProfilerPanelWidget`
  timing / performance の要約窓口。pass / resource の完全検査は `FrameDebugViewWidget` 側に寄せる。
- `FrameDebugViewWidget`
  frame 固定、pass / resource / attachment 検査、compare / scrub / step / export を担当する。
- `FrameDebugController`
  capture 状態、比較対象、bundle export を管理する。レンダリング自体は担当しない。
## Timing Event View

- `TimingEventView`
  Lightweight event and timing editor view for cue strips, playhead scrubbing, and compact selection. Use this when the task is event timing rather than full layer/timeline orchestration.
