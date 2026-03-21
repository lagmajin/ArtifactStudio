# drawRectLocal トランスフォーム対応 WIP メモ (2026-03-21)

## 状態

- `drawRectLocal()` は既存のまま残す。
- 回転・拡縮対応は新API `drawSolidRectTransformed()` に切り出した。
- 新APIは専用の CB `CBSolidRectTransform2D` と専用 VS `drawSolidRectTransformVSSource` を使う。
- 座標変換は CPU 側で 1 回だけ合成し、shader はその行列をそのまま読む。

## いまの構成

- `Artifact.Render.PrimitiveRenderer2D`
  - `drawSolidRectTransformed(float x, float y, float w, float h, const QTransform&, const FloatColor&, float opacity)` を追加
  - `DrawSolidRectTransformMatrixCB` に 4x4 相当の行列を詰める
- `Artifact.Render.ShaderManager`
  - `DrawSolidRectTransform PSO` を追加
  - 既存の solid rect PSO とは別に管理する
- `ArtifactSolid2DLayer` / `ArtifactSolidImageLayer`
  - いったん旧APIへ戻した
- `CompositionRenderController` / `PreviewCompositionPipeline`
  - 固体レイヤーは旧APIで描く
  - rasterizer 系エフェクトがある場合だけ surface 経路を使う

## 重要な注意

- `drawRectLocal()` と新APIを同じレイヤーに二重適用しない。
- `surface.transformed()` と `drawSolidRectTransformed()` を同じ描画フローで重ねない。
- transform の責務は `caller -> renderer -> shader` のどこか 1 か所に固定する。
- コンポジション表示は回転しないので、当面は旧APIで十分な可能性が高い。

## 次に確認すること

- 固体レイヤーの回転が viewport 上で期待どおりになるか
- rasterizer 系エフェクトありレイヤーで見た目が崩れないか
- 他の layer type に同じ新APIを広げるかどうか

## 仮説 (2026-03-21 シェーダー/CB 解析)

### 仮説1 (決定的): zoom と pan が CB に反映されていない

`drawSolidRect` (動作する) の CB:

```
offset = { x*zoom + pan.x, y*zoom + pan.y }
scale  = { w*zoom, h*zoom }
シェーダー: pos = unitVertex * scale + offset
結果: pos = (vertex * size + position) * zoom + pan  ✓
```

`drawSolidRectTransformed` の CB:

```
row0 = { 2*(a*w)/screenW, 2*(c*h)/screenW, 0, 2*(a*x + c*y + tx)/screenW - 1 }
zoom も pan も含まれない
結果: pos = (vertex * transform) / screen → NDC直接変換  ✗
```

`viewportCB.zoom` と `viewportCB.offset` を読み取っているが**全く使っていない**。
zoom≠1 または pan≠{0,0} の場合、描画位置がビューポート外に飛ぶ。

### 仮説2 (高): NDC 変換方式が旧APIと不整合

- `drawSolidRect`: ピクセル空間で計算 → シェーダーの別ステージで NDC
- `drawSolidRectTransformed`: CB 内で直接 NDC (`/screenSize * 2 - 1`)

同じ頂点バッファとインデックスバッファを使っているが、PSO が異なる。
PSO のビューポート/シザー設定の差異で描画領域がクリップされる可能性。

### 仮説3 (中): QTransform の dx/dy と x,y パラメータの重複

CB 内で `tx = transform.dx()` をオフセットとして使い、さらに `a*x + c*y` を加算している。
呼び出し側が `x=0, y=0` で渡していれば問題ないが、`x` がレイヤーの配置位置の場合、
`a*x` が二重適用される可能性 (transform に既に position 情報が含まれている場合)。

### 仮説4 (低): 行列のパッディング不整合

`CBSolidRectTransform2D` は `float4 row0,row1,row2,row3` で 64 バイト。
`#pragma pack(push,1)` でパックされているが、Diligent の cbuffer アライメント (16バイト) と
一致しているか確認が必要。不一致の場合、シェーダーが読み取る値がずれる。

---

### 修正方針

仮説1 の修正: CB に zoom と pan を反映する:

```cpp
const float zoom = std::max(viewportCB.zoom, 0.001f);
const float2 pan  = viewportCB.offset;

// composition空間→viewport空間→NDC の2段階変換をCBに埋め込む
// pos_viewport = transform(vertex) * zoom + pan
// pos_NDC      = pos_viewport / screenSize * 2 - 1

cbTransform.row0 = {
    2.0f * (a * w * zoom) / screenW,
    2.0f * (c * h * zoom) / screenW,
    0.0f,
    2.0f * ((a*x + c*y + tx) * zoom + pan.x) / screenW - 1.0f
};
cbTransform.row1 = {
    -2.0f * (b * w * zoom) / screenH,
    -2.0f * (d * h * zoom) / screenH,
    0.0f,
    1.0f - 2.0f * ((b*x + d*y + ty) * zoom + pan.y) / screenH
};
```
