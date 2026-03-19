# drawRectLocal 調査・修正記録 (2026-03-19)

## 対象

- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `ArtifactCore/src/Graphics/Shader/BasicVertexShader.cppm`
- `ArtifactCore/src/Transform/ViewportTransformer.cppm`

---

## 既存の問題

### 問題 1: `w/h` が実質反映されない

- `drawRectLocal()` は `CBSolidTransform2D.scale` に `w/h` を入れていたが、SolidRect の VS は `scale` を使っていなかった。
- VS 側は `pos = input.pos + offset` のみで計算しており、`scale` は未使用だった。
- そのため、矩形サイズの制御が壊れやすい状態だった。

関連:

- `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`drawRectLocal`)
- `ArtifactCore/src/Graphics/Shader/BasicVertexShader.cppm` (`drawSolidRectVSSource`)

### 問題 2: ズーム反映の参照先ミスマッチ

- `drawRectLocal()` は `viewportCB.scale` をズームとして使う前提だった。
- しかし `ViewportTransformer::GetViewportCB()` は `scale = {1,1}` を返し、実ズームは `zoom` フィールドに格納されている。
- 結果として `drawRectLocal()` はズームを正しく反映できない状態だった。

関連:

- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `ArtifactCore/src/Transform/ViewportTransformer.cppm`

---

## 今回の修正

### 修正 1: SolidRect VS を座標仕様書の式に合わせた

- `drawSolidRectVSSource` を `pos = input.pos * scale + offset` に変更。
- `COORDINATE_SYSTEMS.md` の `View = Composition * Zoom + Pan` と一致する式へ統一。

修正ファイル:

- `ArtifactCore/src/Graphics/Shader/BasicVertexShader.cppm`

### 修正 2: `drawRectLocal()` の TransformCB 設定を仕様準拠に変更

- 頂点は `0..1` のローカル矩形を使用。
- `cbTransform.offset = x * zoom + pan, y * zoom + pan`
- `cbTransform.scale = w * zoom, h * zoom`

修正ファイル:

- `Artifact/src/Render/PrimitiveRenderer2D.cppm`

### 修正 3: 他の Primitive 描画 API も同じ変換規約へ統一

- `drawSpriteLocal`, `drawCheckerboard`, `drawGrid` を同じ offset/scale 規約に変更。
- `drawLineLocal`, `drawThickLineLocal`, `drawDotLineLocal`, `drawSolidTriangleLocal` は
  頂点を Composition 座標として扱い、`scale = {zoom, zoom}`, `offset = pan` に統一。
- `viewportCB.scale` 依存を排除し、`viewportCB.zoom` を使用。

修正ファイル:

- `Artifact/src/Render/PrimitiveRenderer2D.cppm`

---

## 変更後の挙動（要点）

- `drawRectLocal(x, y, w, h, ...)` で `w/h` が正しく描画サイズに反映される。
- Rect/Sprite/Grid/Checker/Line 系が同一の座標変換規約で動作する。
- パン (`offset`) とズーム (`zoom`) が全 API で一貫して反映される。

---

## 未対応の残課題

### 残課題 1: `ViewportCB.scale` フィールドの意味整理

- 現在 `ViewportTransformer::GetViewportCB()` は `scale={1,1}` 固定で返す。
- 実運用では `zoom` を使う設計に揃えたため、`scale` の名称/用途が将来的に誤解を生みやすい。

---

## 確認観点（手動）

- ズーム 100% / 200% / 50% で Rect/Sprite/Grid/Checker/Line が一貫して拡大縮小する。
- パン後でも各プリミティブが同じ方向・同じ量で追従する。
- `opacity` が既存どおり `color.a * opacity` で効く。
