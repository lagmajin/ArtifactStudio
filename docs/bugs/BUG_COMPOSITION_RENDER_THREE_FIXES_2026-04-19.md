# Composition Render: Three Bug Fixes (2026-04-19)

## 1. Grid pan/zoom drift (root cause + fix)

### 症状
コンポジットエディタでパン操作するとグリッド線がずれる。
ズームレベルを変えるたびにグリッドの位置も変わって見える。

### 根本原因
`g_gridPS` (ViewerHelperShaders.ixx) が `SV_POSITION.xy`（スクリーン絶対ピクセル座標）で `fmod` を計算していた。

```hlsl
// 修正前 (間違い)
float2 pos = input.pos.xy;   // SV_POSITION = 絶対スクリーン座標
float2 grid = fmod(pos, spacing);
```

`SV_POSITION` はパン移動で変化するため、コンポジションが画面上を動くたびにグリッド位相がずれる。

### 修正
VS `drawSolidRectVSSource` がすでに `TEXCOORD0` にユニット UV (0..1) を出力しているが、
グリッド PS は `COLOR0` しか宣言していなかったため UV が届いていなかった。

- グリッド PS の `PSInput` に `float2 uv : TEXCOORD0` を追加
- `cbuffer` の `float2 padding` を `float2 canvasSize` にリネーム（レイアウト不変）
- `canvas_pos = uv * canvasSize` でコンポジション座標系に変換してから `fmod`
- `drawGrid()` 側で `pkt.helper._pad[0] = w; pkt.helper._pad[1] = h;` でキャンバスサイズを渡す

```hlsl
// 修正後
float2 canvas_pos = input.uv * canvasSize;   // キャンバス座標 (0..w, 0..h)
float2 grid = fmod(canvas_pos, spacing);
```

### 変更ファイル
- `Artifact/include/Render/ViewerHelperShaders.ixx`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`drawGrid`)

---

## 2. 低ズームでライン・グリッド線が消える (root cause + fix)

### 症状
ズームアウト時にグリッド線・十字線・厚みのある線がすべて消える。

### 根本原因
`drawThickLineLocal()` の thickness および `drawGrid()` の thickness が
**キャンバス座標単位**のピクセル数として扱われていた。

ズーム rate が小さいとき: `screen_pixels = thickness * zoom < 1px` → ラスタライザが描画を破棄する。

例: thickness=1.0, zoom=0.1 → 画面上 0.1px → 不可視

### 修正
両関数で `effectiveThickness = max(thickness, 1.0 / zoom)` としてスクリーン上最低 1px を保証。

```cpp
// drawThickLineLocal
const float effectiveThickness = std::max(thickness, 1.0f / zoom);

// drawGrid
const float effectiveThickness = std::max(thickness, 1.0f / zoom);
```

### 変更ファイル
- `Artifact/src/Render/PrimitiveRenderer2D.cppm` (`drawThickLineLocal`, `drawGrid`)

---

## 3. GPU テキストデバッグオーバーレイが歪んで見える

### 症状
「GPU TEXT TEST」テキストが小さくて歪んで見える。

### 原因
最小フォントサイズを 9pt に設定していたため、アンチエイリアシングにより
小さなテキストがギザギザ・滲みとして見えていた。

### 修正
デバッグオーバーレイの最小フォントサイズを 9pt → 14pt に変更。

```cpp
font.setPointSizeF(std::max(14.0, static_cast<double>(font.pointSizeF())));
```

### 変更ファイル
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`drawGpuTextDebugOverlay`)

---

## 既知の残課題

- DPR (devicePixelRatio) != 1.0 の環境で、グリフ描画サイズと `QFontMetricsF` の
  論理ピクセル間にズレが生じる可能性がある。DPR=1 では問題なし。
  修正方針: `submitGlyphTextTransformed()` 内で `width/height/bearingX/bearingY` を DPR で除算する。
