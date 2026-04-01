# Inline Interaction Surfaces Milestone (2026-03-31)

プロパティパネル、ビューポート、タイムライン、レイヤーパネルに散らばっている「その場で触る」操作を、共通の inline interaction として整理するマイルストーン。

狙いは、ダイアログに逃がす前に、まず行内・キャンバス内・行上で完結できる操作を増やすこと。

## Goal

- プロパティ編集をその場で完結しやすくする
- ビューポート内数値編集を最小モード切り替えで成立させる
- タイムラインとレイヤーパネルの inline 展開を統一する
- 画面遷移やポップアップ頼みの編集を減らす
- 小さな編集を「即時反映 + 即時復帰」できるようにする

## Scope

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/src/Layer/ArtifactCameraLayer.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`

## Non-Goals

- 専用モーダルダイアログの完全廃止
- 全 property を inline にすること自体を目的化する
- ビューポート編集を高機能ペイントツール化する
- 既存の editor / inspector の置き換えを一度で行う

## Current Targets

- プロパティパネル内の inline 展開
  - color picker
  - gradient editor
  - blend mode picker
  - expression input
- ビューポート内の inline 数値編集
  - size / position / anchor / transform
  - Figma 風の scrub input
- タイムライン内の inline input
  - expression / text / waveform preview
  - レイヤー行上での即時編集
- レイヤーパネル内の inline choice
  - mask preview
  - blend mode grid
  - 列幅や表示モードの即時調整

## Phases

### Phase 1: Inline Container Primitives

- inline 展開用の共通コンテナ方針を決める
- popup / embedded / overlay の3パターンを整理する
- 展開時のフォーカス・閉じる・commit/cancel を統一する
- CompositionEditor の selection / tool / fit などの同期は Qt signal 直結ではなく internal event に集約する
- widget 間の更新は 1 tick 遅延で coalesce し、連鎖再描画や再入を減らす

### Phase 2: Property Panel Inline Editors

- color / gradient / blend / expression を property row 内で開けるようにする
- ダイアログを開かずに値を反映できるようにする
- 既存の inspector / property widget と値が矛盾しないようにする

### Phase 3: Viewport Inline Scrub

- `W/H/X/Y/Anchor` などの数値を viewport 上で直接触れるようにする
- scrub と direct input を切り替えられるようにする
- selection gizmo と干渉しない操作モードを作る

### Phase 4: Timeline Inline Inputs

- expression input を行内に展開する
- audio layer に waveform を行内で表示する
- keyframe / clip / label の inline affordance を揃える

### Phase 5: Layer Panel Inline Choices

- mask preview
- blend mode grid
- column width / display mode / density の即時変更
- 行の中で終わる軽い選択操作を増やす

## Validation Checklist

- inline 展開後に値を確定/取消できる
- 行内編集がテーマ変更で壊れない
- viewport 操作と selection / gizmo が衝突しない
- ダイアログを開かずに済む編集が増える
- 既存のプロパティパネルと値が同期する
