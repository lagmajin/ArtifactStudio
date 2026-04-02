# MILESTONE: Camera Overlay Experiment

> 2026-04-02 作成

## Goal

Composition Editor 上で、3D カメラの視錐台やフレーム境界を重ねて見せる実験モードを入れる。
3D 編集そのものはまだやらず、現状の 2D コンポジット作業を壊さずに camera aware な確認手段だけを足す。

## Why

今の Artifact には `ArtifactCameraLayer` と `cameraFrustumVisual()` が既にある。
そのため、完全な 3D 編集機能を始める前に、camera の見え方だけを overlay として確認できるモードがあると、レイアウト調整や将来の 3D 導入の検証に使いやすい。

## Scope

- `CompositionEditor` に camera overlay toggle を追加する
- active camera の frustum を viewport 上に重ねる
- camera frame / near / far / clip guide を分かりやすく表示する
- 既存の 2D composition view は維持する

## Non-Goals

- 3D layer editing の本格導入
- viewport を完全に camera render に切り替えること
- camera gizmo の操作系刷新
- render pipeline の大改造

## Phases

### Phase 1: Overlay Wiring

- `CompositionRenderController::cameraFrustumVisual()` を UI から参照する
- `ArtifactCompositionEditor` に overlay toggle を追加する
- overlay 有効時に camera guide を描画する

### Phase 2: Visual Polish

- frustum 線を見やすい色と太さにする
- active camera / inactive camera の見分けを付ける
- composition frame と camera frame の区別を明確にする

### Phase 3: Experimental UX

- overlay の表示切り替えをしやすくする
- 3D 編集へ進まない実験モードとして文言を整える
- 必要なら shortcut / menu から触れるようにする

## Entry Points

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`

## Recommended Order

1. Overlay Wiring
2. Visual Polish
3. Experimental UX
