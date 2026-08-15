# Milestone: Shape Layers (2026-03-29)

**最終更新:** 2026-08-15

**Status:** Phase 1+3 Complete、Phase 2 は部分実装（パラメータ／property／JSON は接続済み、アニメーションと runtime 検証は未完了）
**Goal:** パラメトリックシェイプをレイヤーとして追加。円、星、多角形をタイムラインから即座に生成。

---

## 現状

| 機能 | 状態 |
|------|------|
| Solid レイヤー | ✅ 実装済み |
| SVG インポート | ✅ 実装済み |
| パラメトリックシェイプ | ✅ 実装済み |
| シェイプアニメーション | ❌ 未実装 |
| インスペクタUI | ✅ 実装済み |

## 2026-08-15 現行コード照合

- ✅ `ArtifactShapeLayer` は Rect／Ellipse／Star／Polygon に加え、custom polygon／custom path、fill／stroke、gradient、corner radius、dash／cap／join、shape operators を保持する。
- ✅ Shape property group は `shape.*` の persistent property path として公開され、Property Editor から編集できる。JSON には形状パラメータ、色、stroke、custom path、operator が保存され、`fromJson()` で復元される。
- ✅ `nativeShapePaths()`、`ShapePath`／`ShapeLayer` 系の Core 基盤と、renderer draw／software compatibility fallback の描画経路が存在する。旧記載の「Rect／Ellipse／Star だけ」「Path は未実装」は現状と不一致。
- ⚠️ 現行コードからは、各 shape parameter のキーフレーム評価が一貫して描画へ反映されることまでは静的に保証できない。GPU pixel shader 専用経路としての完了も確認できず、renderer／fallback の実機差は未検証。
- ⏳ パスポイント単位の編集 UI、全パラメータのアニメーション、複雑な operator chain、保存再読込を含む runtime QA は未完了。

## Update 2026-08-15

- `ArtifactShapeLayer` の Rect／Ellipse／Star／Polygon、custom path、fill／stroke／gradient、dash／cap／join、operator と persistent property／JSON 経路を現行コードで再確認。
- Core の `ShapePath`／`ShapeLayer`、native path、renderer と software fallback の両経路が存在するため、旧来の「基本形状のみ」「Path 未実装」という記載は現状に合わせて扱う。
- shape parameter のキーフレーム評価が全描画経路で一貫して反映されること、パスポイント編集 UI、複雑な operator chain、保存再読込と renderer／fallback の pixel parity は未検証。

---

## Architecture

```
ArtifactShapeLayer (新規レイヤータイプ)
  ├── ShapeType: Rect / Ellipse / Star / Polygon / Line / Path
  ├── ShapeParams:
  │     ├── size (width, height)
  │     ├── cornerRadius (角丸)
  │     ├── fillColor / strokeColor
  │     ├── strokeWidth
  │     ├── starPoints / starInnerRadius
  │     └── polygonSides
  └── Rendering:
        ├── GPU: Parametric shape in pixel shader
        └── Software: QPainter path generation
```

---

## シェイプタイプ

| タイプ | パラメータ |
|--------|----------|
| **Rect** | width, height, cornerRadius (4隅個別) |
| **Ellipse** | width, height |
| **Star** | points (5), innerRadius (0.382), outerRadius |
| **Polygon** | sides (6), radius |
| **Line** | x1, y1, x2, y2, strokeWidth |
| **Path** | control points (手動編集) |

---

## Milestone 1: Basic Shapes

### Implementation
- `ArtifactShapeLayer` クラス
- Rect / Ellipse / Star の描画
- QPainter パス描画 + GPU シェーダー描画
- 基本プロパティ (fill, stroke, size)

### 見積: 6h

---

## Milestone 2: Shape Animation

### Implementation
- 各パラメータをアニメーション可能に
- cornerRadius のアニメーション
- starPoints / innerRadius のアニメーション
- パスポイントのアニメーション

### 見積: 4h

---

## Milestone 3: Shape Properties UI

### Implementation
- シェイプタイプ選択
- パラメータスライダー
- Fill/Stroke カラーピッカー

### 見積: 3h

---

## Recommended Order

| 順序 | マイルストーン | 見積 |
|---|---|---|
| 1 | **M1 Basic Shapes** | 6h |
| 2 | **M3 Properties UI** | 3h |
| 3 | **M2 Shape Animation** | 4h |

**総見積: ~13h**

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `Artifact/include/Layer/ArtifactShapeLayer.ixx` | Shape レイヤー |
| `Artifact/src/Layer/ArtifactShapeLayer.cppm` | 実装 |
| `Artifact/shaders/ShapeVS.hlsl` | シェイプ頂点シェーダー |
| `Artifact/shaders/ShapePS.hlsl` | シェイプピクセルシェーダー |

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Layer/ArtifactSolid2DLayer.cppm` | - | Solid レイヤー (参考) |
| `Artifact/src/Layer/ArtifactSvgLayer.cppm` | - | SVG レイヤー (パス描画参考) |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 867 | drawCircle (参考) |
| `ArtifactCore/include/Effetcs/Clone/BasicEffectors.ixx` | - | シェイプエフェクタ (参考) |
