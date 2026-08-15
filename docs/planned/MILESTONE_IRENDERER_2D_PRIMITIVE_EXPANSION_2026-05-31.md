# M-IR-10 ArtifactIRenderer 2D Primitive Expansion

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

Arc と styled polyline は `ArtifactIRenderer`／`PrimitiveRenderer2D` 側に追加済みで、round cap／join、closed、dash pattern、miter／bevel の境界、および Shape layer の stroke／bounds invalidation まで接続されています。`TrimPaths`／`Repeater` の一部条件も native packet 経路へ接続済みです。

一方、gradient／taper、special operator、複数輪郭の fill rule、互換キャッシュ経路との parity、実 GPU／software runtime の描画一致は未完了または未検証です。なお、Rounded Rect は専用 public façade として全面置換されたとは確認できず、既存 rounded panel／point-list 経路が残っています。

## Update 2026-08-15

実装行を再照合した。`PrimitiveRenderer2D::drawArcLocal()` と styled polyline 相当の stroke／dash／cap／join 処理、`ArtifactIRenderer` の rounded panel façade、Shape layer の stroke／bounds 接続は確認できる。一方、TransformGizmo の arc は点列を組み立てて `drawPolyline()` を呼ぶ経路も残る。

- Arc と polyline の基盤は部分的に実装済みだが、提案 API 群が全て統一された public façade として提供されているわけではない。
- Rounded Rect の全面移行、gradient／taper、複数輪郭 fill rule、GPU／software parity の runtime 検証は未完了。
- 判定は **Arc／styled stroke の実装基盤と rounded panel は存在するが、2D primitive expansion の統一完了は未達** を維持する。

`ArtifactIRenderer` / `PrimitiveRenderer2D` に、上位の shape workflow を増やしやすくする低レベル 2D primitive を足すための段階メモ。

## Goal

- `ArtifactIRenderer` に上位 shape layer が再利用しやすい 2D primitive を増やす
- `Line / Arrow / Arc / Rounded Rect / Ring / Callout` のような図形追加を、renderer の薄い追加で進めやすくする
- 新しい図形を renderer 側で直接量産するのではなく、`再利用しやすい primitive` を先に整える

## Why This Exists

- 現状の `ArtifactIRenderer` には `line / bezier / circle / triangle / polygon` があり、土台はかなりある
- 一方で、shape workflow から見ると `Arc`、`Rounded Rect`、`styled polyline` のような中間 primitive が不足している
- その結果、上位 layer 側で point list を毎回組み立てるか、shape ごとに別実装を増やしやすい
- `ImmediateContext` 境界整理を進める上でも、上位から low-level draw を増やすのではなく、`ArtifactIRenderer` façade を少しずつ厚くした方が安全

## Current Ground Truth

- 公開済み primitive
  - `drawSolidLine`
  - `drawPolyline`
  - `drawDashedLineLocal`
  - `drawBezierLocal`
  - `drawSolidTriangleLocal`
  - `drawSolidPolygonLocal`
  - `drawCircle`
- shape layer 側では `Rect / Ellipse / Star / Polygon / Line / Triangle / Square` を point / path に落として描いている
- したがって今ほしいのは「shape list を増やす前に、再利用軸になる primitive を足す」こと

## Recommended Additions

### 1. Arc Primitive

- 最優先
- `Arc / Ring / Donut / Gauge / Sweep` の基礎になる
- `Arrow` の curved variant や motion-graphics 系 HUD にもつながる

### 2. Rounded Rect Primitive

- UI 図形と広告図形の両方で使用頻度が高い
- 現在は shape layer 側で rounded rect を点列近似しているため、renderer 側 primitive があると責務が整理しやすい

### 3. Styled Polyline Primitive

- `Line / Polygon / Open Path / Callout / Arrow` の共通基盤
- cap / join / closed / dash をまとめて扱えるようにすると、shape layer 側の分岐を減らせる

## Explicit Non-Goals

- `Speech Bubble` や `Badge` を renderer primitive として直接増やすこと
- `DiligentEngine` サブモジュール本体を触ること
- 既存の D3D12 path を推測で広く書き換えること

## Candidate API Signatures

最初の提案は「薄い façade」を優先し、実装詳細は `PrimitiveRenderer2D` に押し込む。

### Arc

```cpp
void drawArcLocal(Detail::float2 center,
                  float radiusX,
                  float radiusY,
                  float startAngleDeg,
                  float endAngleDeg,
                  float thickness,
                  const FloatColor& color);

void drawArcLocal(Detail::float2 center,
                  float radius,
                  float startAngleDeg,
                  float endAngleDeg,
                  float thickness,
                  const FloatColor& color);

void drawSolidArcLocal(Detail::float2 center,
                       float radiusX,
                       float radiusY,
                       float startAngleDeg,
                       float endAngleDeg,
                       const FloatColor& color);
```

### Rounded Rect

```cpp
void drawRoundedRectLocal(float x,
                          float y,
                          float w,
                          float h,
                          float radius,
                          const FloatColor& color,
                          float opacity = 1.0f);

void drawRoundedRectOutlineLocal(float x,
                                 float y,
                                 float w,
                                 float h,
                                 float radius,
                                 float thickness,
                                 const FloatColor& color);
```

### Styled Polyline

```cpp
enum class PolylineCap {
  Flat = 0,
  Round = 1,
  Square = 2,
};

enum class PolylineJoin {
  Miter = 0,
  Round = 1,
  Bevel = 2,
};

struct PolylineStyle {
  float thickness = 1.0f;
  PolylineCap cap = PolylineCap::Flat;
  PolylineJoin join = PolylineJoin::Miter;
  bool closed = false;
  std::vector<float> dashPattern;
};

void drawStyledPolylineLocal(const std::vector<Detail::float2>& points,
                             const PolylineStyle& style,
                             const FloatColor& color);
```

## Design Notes

- `Arc` はまず近似 polyline / triangle fan ベースでよい
- `Rounded Rect` は内部的に 4 corner bezier 近似でも、polygon fan でもよいが、API は 1 個に固定したい
- `Styled Polyline` は将来的に shape layer の `StrokeCap / StrokeJoin / dashPattern` と自然に接続できる命名に寄せる
- 既存 `ShapeType::Line` を強くする用途にも、この primitive 群がそのまま効く

## Suggested Phase Order

### Phase 1: Arc / Rounded Rect Façade

- `ArtifactIRenderer` 宣言面を増やす
- `PrimitiveRenderer2D` に最小実装を入れる
- まずは `shape layer` 未接続でもよい

### 2026-07-29 Implementation Loop

- `PrimitiveRenderer2D::drawArcLocal()` を追加し、既存の thick-line packet へ分割委譲する円弧 stroke の最小実装を追加。
- `ArtifactIRenderer::drawArcLocal()` から上記 primitive を公開し、上位 layer が renderer façade 経由で再利用できる境界を確立。
- 既存 `drawRoundedPanel()` の outline corner を `drawArcLocal()` へ移し、rounded rectangle の重複した円弧実装を整理。
- `PolylineStyle` と `ArtifactIRenderer::drawStyledPolyline()` を追加し、closed／round join／round cap／square cap／任意長 dash pattern の連続消費を実装。
- miter／bevel の外側 join geometry を追加。`ArtifactShapeLayer` の custom path と単純な parametric shape native stroke を `drawStyledPolyline()` へ接続し、layer の cap／join／dash 設定を renderer へ渡す経路を追加。
- special operator、gradient／taper の互換キャッシュ経路、runtime parity は未完了。

Implementation update (2026-07-29): `TrimPaths`／`Repeater` の operator 出力を、solid fill・center stroke・taper／gradient なしの限定条件で `flattenSubpaths()`／`triangulate()` から native packet へ接続した。特殊条件は従来の互換 QImage 経路に残るため、runtime parity と全条件の native 化は未完了。

同経路は operator の加工結果が空の場合に native early-return せず、既存の互換キャッシュ経路へ戻る境界条件も追加済み。

operator の追加／削除／並べ替え／プロパティ編集について、local bounds と shape content cache の invalidation も同じ更新契約に統一済み。

gradient／taper など互換キャッシュへ進む operator は、native packet 用の加工を先に実行しないよう候補判定を分離した。空の加工結果も従来の互換経路へ戻る。

Shape layer の local bounds padding は stroke が無効な場合を除き、Center／Outside／Inside の align 契約に合わせて算出するよう更新した。
`setStrokeAlign()` も local bounds cache を無効化し、編集直後の bounds 再計算に追従する。
Miter join は renderer の effective thickness（最低 1.0、上限 `4 × thickness`）に合わせた保守的な bounds padding を使用し、`setStrokeJoin()` でも再計算を要求する。
Dash pattern は `setDashPattern()` の入口でも有限かつ正の値へ正規化し、GPU／software cache 経路の入力契約を統一する。
dash 有効時は solid 専用の join／round-cap overlay を抑止し、gap を頂点 geometry が埋めないようにした。dash-run の開始／終了を segment 間で追跡し、各 dash に square／round cap を付与する経路も追加済み。runtime parity は未検証。

次の安全な実装単位は、operator 複数輪郭の順序・fill rule・bounds invalidation と、gradient／taper の native contract を固定すること。

### Phase 2: Styled Polyline

- cap / join / dashPattern を low-level primitive に持ち込む
- `Line` と custom path の stroke 側で再利用しやすい共通面を作る

### Phase 3: Shape Workflow Adoption

- `ShapeType::Line` の強化
- `Rounded Rect` preset または shape entry の追加
- `Arc` / `Arrow` の app-layer 露出

## Success Criteria

- `ArtifactIRenderer` に、上位図形の土台として再利用できる 2D primitive が増える
- `Line` や `Rounded Rect` の app-layer 実装が renderer の寄せ集めではなく、正式 API を通る
- low-level 追加が `ImmediateContext` の露出拡大ではなく、renderer façade の強化として成立する

現時点の判定: **Phase 1 Arc／Rounded Rect façade と Phase 2 Styled Polyline の custom／simple parametric shape 接続は実装済み（special operator・runtime 検証 pending）**。

## Target Files

- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/PrimitiveRenderer2D.ixx`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`

## Related

- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`
- `docs/MILESTONE_SHAPE_LAYER_ENHANCEMENT_2026-04-28.md`
