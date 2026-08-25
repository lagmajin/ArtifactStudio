# MILESTONE: Element 3D 相当機能 — Shape/Text 押し出しジオメトリと World Position AOV

**最終更新:** 2026-08-25

**ステータス:** In Progress（押し出しコアジオメトリ実装済み・レイヤー配線未着手。AOV は設計確定のみ）

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

## 未着手

- **レイヤー配線**: `ArtifactShapeLayer` の `cachedNativeGeometry_`（triangles + flattenSubpaths 済み輪郭）を
  `extrudeContourMesh()` に渡し、生成 Mesh を Model3D 描画経路へ接続する導線。
  配線候補は (a) Shape レイヤーの 3D モードで mesh draw へ切替、(b) `ArtifactProcedural3DLayer` に
  `PathExtrude` kind 追加。Text は glyph 輪郭→ShapePath 化して同じ関数へ渡す。
- **UI**: extrude depth / bevel width / bevel segments のプロパティ露出（Inspector）。
- **World Position / Depth AOV**: メッシュ描画パスでの world position 書き込みパス（専用 PSO または MRT）が本体。
  静的調査済みの統合点:
  - `ArtifactCompositionRenderController.cppm` の `PrecompGpuOutputEntry`（~10297 行付近、color/depth target + SRV を保持）
    に world position 用 float target（`createOffscreenComputeTexture` + SRV で Diligent バックエンド無変更でリソースは用意可能）
  - 書き込み自体は `DiligentImmediateSubmitter` の mesh draw パスへの PSO 追加が必要（シビアコードのため別スライスで実装する）
  - 消費側: DOF / fog / 2D エフェクトへの depth 参照は Phase 3 の depth/DOF 連携計画と合流

## 対象ファイル

- `ArtifactCore/include/Geometry/ShapeExtrude.ixx` / `ArtifactCore/src/Geometry/ShapeExtrude.cppm`
