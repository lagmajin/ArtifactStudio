# MILESTONE: Element 3D 相当機能 — Shape/Text 押し出しジオメトリと World Position AOV

**最終更新:** 2026-09-21

**ステータス:** In Progress（押し出しコア・TextExtrude レイヤー配線・Position/UV AOV 実装済み。Shape 3Dモード・runtime検証残）

## 目的

Element 3D 相当の機能のうち、(1) Shape/Text 輪郭からの Extrude + Bevel 3D ジオメトリ生成、
(2) world position / depth のコンポジット連携出力(AOV)を実現する。

## 実装済み（2026-08-25）

1. `Geometry.ShapeExtrude` モジュール（ArtifactCore）
   - `ArtifactCore/include/Geometry/ShapeExtrude.ixx`
   - `ArtifactCore/src/Geometry/ShapeExtrude.cppm`（非 export 実装ユニット、force list 不要）
   - `extrudeContourMesh(contours, params, outMesh)`: 閉輪郭群（even-odd で穴分類）から
     `ArtifactCore::Mesh` を生成。外周=CCW / 穴=CW に回転方向を正規化し、
     ear clipping キャップ（穴内部三角形は重心判定で除去）+ 四分円ベベルリング +
     側壁クワッドで構成。position / normal / uv 属性付きで
     Model3D の `generateRenderData()` 経由 GPU 経路にそのまま乗る形式。
   - ベベル無効（bevelWidth <= 0）時は直角押し出しにフォールバック。
2. `ArtifactCoreShapeExtrudeTest`（2026-08-30、ビルド未実行）
   - 閉じた矩形の position / normal / uv 属性、depth 範囲、render data 生成を静的な
     回帰対象へ追加。
   - bevel による幾何増加と、不正 depth 入力時に既存 `Mesh` を置換しない契約を追加。

## 未着手

- **レイヤー配線 (2026-09-21 TextExtrude まで実装)**: `ArtifactProcedural3DLayer` に
  `Procedural3DLayerKind::TextExtrude` を追加。`Procedural3DGenerators::generateTextExtrude`
  (QFont/QPainterPath → toSubpathPolygons → `extrudeContourMesh` → `generateRenderData`)
  で Mesh 化し、既存 drawMesh/material/AOV 経路へ接続。text/fontFamily/fontSize/bold/italic/
  depth/bevelWidth/bevelSegments をプロパティ・JSON・プリセット(`beveledText3D`)・
  Layerメニュー(`Text 3D (Extrude)`)へ露出。残りは (a) Shape レイヤーの 3D モード切替。
- **UI**: extrude depth / bevel width / bevel segments のプロパティ露出（Inspector）。
  ※TextExtrude 分は実装済み。Shape 3Dモード分が残。
- **World Position / Depth AOV**: 2026-09-21 に Position(XYZ)/UV チャンネルとして実装済み
  (`ChannelType` 拡張 + mesh only-pass mode 9/10 + pipeline target + EXR/Queue/VP 配線)。
  残りは DOF / fog / 2D エフェクトへの depth 参照で Phase 3 の depth/DOF 連携計画と合流。

## 未検証事項

- `ArtifactCoreShapeExtrudeTest` を含む ArtifactCore build/test は、リポジトリ方針により未実行。
- Shape Layer の 3D 表示は現時点では Composition Render Controller の
  `DirectShape3DCard` 経路であり、`Mesh` ownership・再生成・material 契約を持たない。
  押し出しを接続する際はこのカード経路を置換せず、明示的な 3D mesh slice として導入する。

## 対象ファイル

- `ArtifactCore/include/Geometry/ShapeExtrude.ixx` / `ArtifactCore/src/Geometry/ShapeExtrude.cppm`
