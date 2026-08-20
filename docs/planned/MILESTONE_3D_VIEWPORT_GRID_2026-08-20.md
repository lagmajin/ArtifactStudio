# M-GRID-3D-1 DCC風3D Viewport Grid

**最終更新:** 2026-08-20
ステータス: In Progress（既存の3D XZグリッド描画経路を確認。Viewport設定UIと受入確認が未完了）

## 1. 目的

ArtifactStudioの3D Viewportに、Blender／Maya系DCCで一般的な作業用グリッドを提供する。グリッドはエディタ補助表示であり、最終レンダーには含めない。

## 2. 既存基盤

- `Artifact::Grid::GridSettings`
- `Artifact::Grid::GroundGridSettings`
- `Artifact::Grid::GridSystem`
- `CompositionRenderController::setGridSettings()`
- `CompositionRenderController::setShowGrid()`
- `PrimitiveRenderer3D::drawGroundGrid()`
- `ArtifactAppSettings::compositionGridSettings()`
- 既存View MenuのMajor／Minor／Axis／Numbers設定

### 現行コード確認（2026-08-20）

- `CompositionRenderController`は`showGrid_`、3D Viewport判定、ギズモ用カメラ行列が有効な場合に`GroundGridSettings`を生成し、`GridSystem::computeGroundGridLines()`からXZ平面の線を描画している。
- 3DグリッドはPerspectiveの投影スケールからMajor間隔を自動計算し、カメラ距離に応じて範囲とフェードを調整している。
- 3D軸線は`GridSettings::axisColor`を参照し、汎用グリッド設定と色設定を共有するようにした。
- 残りは3D Viewportでの表示導線、平面切替、ラベル／軸色の統一、グリッドスナップの実機受入である。

新しいグリッド設定体系や別レンダーパスは作らず、既存の汎用設定を3D Viewportへ接続する。

## 3. スコープ

### P0: DCC風XZグリッド

- 3D ViewportでXZ平面グリッドを表示
- 表示／非表示
- X軸／Z軸／原点の強調
- Major／Minorライン
- グリッド間隔とMinor分割数
- Perspective／Orthographic両方で表示
- グリッドは非レンダー対象

### P1: 操作と設定

- グリッドスナップの有効／無効
- View MenuまたはViewport表示メニューからの切替
- `GridSettings`の保存／復元
- カメラズームに応じた自動密度
- グリッド色・透明度・数値ラベル

### P2: 3D作業平面

- XY／XZ／YZ平面の切替
- カスタム作業平面
- 作業平面の原点・法線編集
- 3Dギズモの軸拘束とグリッドスナップの統合
- 無限グリッド風の遠近表示

## 4. 非スコープ

- 最終レンダーへのグリッド出力
- グリッドを通常レイヤーとしてタイムラインへ追加
- 動画レイヤー・音声・アニメーションレイヤー連携
- Qt合成や`QImage`による描画

## 5. 完了条件

- 3D Viewportを開くとXZグリッドを表示できる
- 軸線と原点が通常グリッドより明確に見える
- Perspective操作でグリッドが破綻しない
- グリッド表示をView Menuから切り替えられる
- グリッドスナップが3Dギズモ操作で機能する
- 設定が再起動後も復元される
- レンダー結果にグリッドが混入しない

## 6. 実装順序

1. `GroundGridSettings`の既存描画結果と3D Viewport経路を確認
2. XZ平面の表示と軸線・原点表示を接続
3. View Menuの表示切替と既存`GridSettings`保存を接続
4. 3Dギズモのスナップへグリッド値を統合
5. XY／YZ平面とカスタム作業平面をP2として追加

## 7. リスク

- 2Dコンポジション用グリッドと3D作業グリッドを混同しない
- Viewport Overlayとレンダーパスを分離する
- 既存Smart Guidesのスナップと二重適用しない
- Diligent低レベルコードを広範囲に変更せず、既存のPrimitiveRenderer3D経路を優先する
