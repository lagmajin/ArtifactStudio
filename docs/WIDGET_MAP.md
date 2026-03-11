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
- `ArtifactInspectorWidget`
  右側の Inspector パネル。現在のコンポジション / レイヤー / エフェクト情報を編集する。
- `ArtifactCompositionAudioMixerWidget`
  現在コンポジションの音声レイヤー用ミキサー。

## Composition Viewer

- `ArtifactCompositionEditor`
  コンポジションビューア本体。再生、停止、ズーム、フィットなどを持つ。

## Timeline

- `ArtifactTimelineWidget`
  タイムライン全体を束ねる親ウィジェット。左ペイン、右ペイン、同期、playhead をまとめる。
- `ArtifactLayerPanelWidget`
  タイムライン左ペイン。レイヤー名、親、ブレンド列などを持つ独自ツリーパネル。
- `ArtifactTimelineNavigatorWidget`
  タイムライン右上の細いバー。表示範囲を調整するタイムナビゲーター。
- `ArtifactTimelineScrubBar`
  タイムナビゲーターの下にある `F0 / 00:00:00:00` を表示するスクラブバー。クリック/ドラッグで現在フレームを動かす。
- `ArtifactWorkAreaControlWidget`
  ワークエリアの IN/OUT 範囲を編集するバー。
- `TimelineTrackView`
  タイムライン右下の本体。クリップアイテム、赤いシークバー、行グリッドを表示する。
- `TimelinePlayheadOverlay`
  右ペイン上に重なる赤い縦棒のオーバーレイ。

## Render / Queue

- `RenderQueueManagerWidget`
  レンダーキュー管理パネル。ジョブ一覧、出力先、フレーム範囲、詳細設定ダイアログ導線を持つ。
- `ArtifactRenderOutputSettingDialog`
  コーデック、解像度、fps、ビットレートなどの詳細出力設定ダイアログ。

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
- `F0 / 00:00:00:00` の段
  UI 用語では `スクラブバー`。
- 太い範囲バー
  UI 用語では `ワークエリアバー`。実体は `ArtifactWorkAreaControlWidget`。
- 右下の編集面
  UI 用語では `タイムライン本体` または `レイヤー編集領域`。コード上では `TimelineTrackView`。

## Primary Entry Points

- `Artifact/src/AppMain.cppm`
  主要 dock / widget の生成と接続。
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
  タイムライン構造と相互同期の中枢。
- `Artifact/src/Widgets/Menu/ArtifactTestMenu.cppm`
  テスト系ウィジェットの起動メニュー。
