# MILESTONE: 平面レイヤー Noise Fill

**最終更新:** 2026-08-18

**ステータス:** Planned

**識別子:** M-SOLID-NOISE

## 目的

`ArtifactSolidImageLayer`（平面レイヤー）の塗りを、単色とグラデーションに加えて「ノイズフィル」へ拡張する。既存の `ProceduralTextureGenerator`（ArtifactCore）が持つ Perlin / Simplex / FBM / Voronoi / White / Value / Gradient の生成能力と GPU コンピュートパイプラインを再利用し、AE の Solid / Gradient には無い「手続きノイズを直接塗れる平面」を実現する。

Domain Warp、Secondary Blend、オフセットのキーフレーム駆動により、雲・煙・水面・大理石・布などの有機的なモーショングラフィックを平面1枚で成立させることを到達点とする。

## 背景と責務

- 現在の平面レイヤーは `ArtifactSolid2DLayer`（旧）と `ArtifactSolidImageLayer`（新）の2系統が並存する。
- `ArtifactSolidImageLayer` は `AnimatableValueT<FloatColor>` によるキーフレーム対応と、`toQImage()` / `getThumbnail()` / `draw()` の描画経路を持つ。ノイズフィルの正規実装先とする。
- `ArtifactSolidFillType` は現在 `Solid / LinearGradient / RadialGradient / ConicalGradient / RepeatingGradient / MirroredGradient` の6値。ここに `Noise` を追加する。
- `ArtifactCore` には既に3系統のノイズ資産がある:
  - `ProceduralTextureGenerator`（`ImageProcessing.ProceduralTexture`）— 7種のノイズ + Domain Warp + Secondary Blend + ポスト処理 + 6プリセット + GPU コンピュートパイプライン（HLSL 実装済み）。**本マイルストーンの主要再利用対象。**
  - `NoiseImageGenerator`（`Generator:Noise`）— CPU のみ。turbulence / woodGrain / marble / cloudTexture の独自プリセットを持つ。
  - `NoiseGenerator` / `NoiseField`（`Math.Noise`）— 低レベル。`NoiseField` は値型で並行安全、`NoiseGenerator` は process-wide seed のレガシー。
- 本マイルストーンはノイズ生成アルゴリズムを新規実装せず、既存資産の「平面フィルとしての露出」に責務を限定する。

## 到達目標

1. 平面レイヤーの Fill Mode に `Noise` を追加し、既存の単色・グラデーションと同列に選択・編集・保存・復元できる。
2. Perlin / Simplex / FBM / Voronoi（Distance/Cell/Edge）/ White / Value / Gradient の7種をノイズタイプとして選択できる。
3. Marble / Clouds / Cellular / Fabric / Terrain / Metal の既存プリセットと、Turbulence / WoodGrain の移植プリセットをワンクリックで呼び出せる。
4. Seed / Scale / Offset / Rotation / Amplitude / Octaves / Lacunarity / Gain / Cell Jitter / Domain Warp / Secondary Blend / Gamma / Invert をキーフレーム可能なプロパティとして編集できる。
5. Offset と Rotation のキーフレーム駆動により、ノイズが「流れる」アニメーションを平面1枚で成立させる。
6. グレースケールノイズ値を2色グラデーションへマップし、「ノイズ駆動グラデーション」を表現できる。
7. GPU パイプライン（`ProceduralTextureComputePipeline`）でノイズテクスチャを生成し、QImage を経由せずレンダラーへ渡す。GPU 非対応時のみ CPU 生成（`ImageF32x4_RGBA`）へフォールバックする。

## 対象範囲

- `ArtifactSolidImageLayer`（`Artifact/src/Layer/ArtifactSolidImageLayer.cppm`）と `ArtifactSolidLayerInitParams`（`Artifact/include/Layer/ArtifactLayerInitParams.ixx` / `.cppm`）。
- `ArtifactSolidFillType` への `Noise` 追加と、ノイズパラメータの保持・JSON 保存・プロパティグループ・プロパティ値設定。
- `draw()` / `toQImage()` / `getThumbnail()` のノイズ経路。
- ノイズ生成は `ProceduralTextureGenerator`（CPU）と `ProceduralTextureComputePipeline`（GPU）を再利用。
- `CreatePlaneLayerDialog` / `EditPlaneLayerSettingDialog` のノイズ設定 UI。
- プリセットの呼び出し経路。

## 非対象

- ノイズ生成アルゴリズムそのものの新規実装（既存の `ProceduralTextureGenerator` を流用）。
- 物理反応型フィル（流体 / 砂 / パーティクルが平面を満たす）— 別マイルストーン。
- オーディオリアクティブフィル、式駆動フィル — 別マイルストーン。
- `ArtifactSolid2DLayer`（旧系統）へのノイズフィル追加。本マイルストーンでは `ArtifactSolidImageLayer` に一本化し、旧系統は「触らない」だけでなく廃止方向の確認に留める。
- 3D マテリアル / ボリュームレンダリングへのノイズ適用。
- `ReactiveEvents` の変更。

## データ契約

### Fill Type 拡張

`ArtifactLayerInitParams.ixx` の `ArtifactSolidFillType` に1値を追加する。

```cpp
enum class ArtifactSolidFillType {
  Solid = 0,
  LinearGradient = 1,
  RadialGradient = 2,
  ConicalGradient = 3,
  RepeatingGradient = 4,
  MirroredGradient = 5,
  Noise = 6
};
```

既存の enum 値（0-5）は変更せず、整数保存の後方互換を保つ。`setFillType()` の `std::clamp(..., 0, 5)` を `0, 6` へ更新し、`setLayerPropertyValue()` の fillType 分岐に `Noise` を追加する。

### ノイズパラメータ保持

`ArtifactSolidImageLayer::Impl` に `ProceduralTextureSettings` とカラーマップ用の2色を保持する。

```cpp
ProceduralTextureSettings noiseSettings_;
bool noiseColorMappingEnabled_ = false;
FloatColor noiseColorA_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
FloatColor noiseColorB_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
```

`ProceduralTextureSettings` が既に kind / seed / scale / offset / rotation / amplitude / octaves / lacunarity / gain / cellJitter / voronoiMode / gradientMode / post（secondary / warp / blend / gamma / invert / clamp / remap / normalize）を包含するため、新規構造体を追加しない。

### JSON 保存

既存の `solid.*` キー群に並ぶ形で `solid.noise.*` キーを追加する。enum は整数ではなく安定した文字列で保存する（`"perlin"`, `"fbm"`, `"voronoiDistance"` 等）。未知の kind は読み込み時に `Perlin` へ無効化フォールバックし、将来バージョンで復元可能にする。

```text
solid.fillType = 6
solid.noise.kind            = "perlin"
solid.noise.voronoiMode     = "distance"
solid.noise.seed            = 0
solid.noise.scaleX          = 8.0
solid.noise.scaleY          = 8.0
solid.noise.offsetX         = 0.0
solid.noise.offsetY         = 0.0
solid.noise.rotation        = 0.0
solid.noise.amplitude       = 1.0
solid.noise.octaves         = 4
solid.noise.lacunarity      = 2.0
solid.noise.gain            = 0.5
solid.noise.cellJitter      = 0.75
solid.noise.domainWarp      = false
solid.noise.warpKind        = "perlin"
solid.noise.warpAmplitude   = 0.25
solid.noise.useSecondary    = false
solid.noise.secondaryKind   = "white"
solid.noise.blendMode       = "add"
solid.noise.blendWeight     = 0.5
solid.noise.gamma           = 1.0
solid.noise.invert          = false
solid.noise.colorMapping    = false
solid.noise.colorA          = {r,g,b,a}
solid.noise.colorB          = {r,g,b,a}
```

## ノイズタイプ

`ProceduralTextureGeneratorKind` をそのまま流用する。

| Fill 名 | Core enum | 用途 |
|---|---|---|
| Perlin | `Perlin` | 定番の滑らかなノイズ |
| Simplex | `Simplex` | 格子アーティファクトが少ない |
| FBM | `FBM` | 雲・煙・地形。octaves/lacunarity/gain |
| Voronoi Distance | `Voronoi` + `Distance` | セル状、金属・泡 |
| Voronoi Cell | `Voronoi` + `Cell` | フラットセル、ステンドグラス |
| Voronoi Edge | `Voronoi` + `Edge` | 亀裂・網目 |
| White | `White` | 純ホワイトノイズ |
| Value | `Value` | 布・織物 |
| Gradient Linear | `Gradient` + `Linear` | 方向性のある波 |
| Gradient Radial | `Gradient` + `Radial` | 放射状の波 |

## プリセット

### 既存（`ProceduralTextureGenerator::makePreset` を流用）

- Marble / Clouds / Cellular / Fabric / Terrain / Metal

### 移植（`NoiseImageGenerator` から）

- Turbulence — `NoiseImageGenerator::turbulence` のパラメータを `ProceduralTextureSettings`（FBM + amplitude/invert 近似）へ写す。
- WoodGrain — `NoiseImageGenerator::woodGrain` の同心円ノイズを、Perlin ノイズ + 半径依存の周期成分として表現する。既存の `ProceduralTextureGenerator` に該当 kind がないため、初期は CPU 生成または低優先の拡張とする。

## レンダリングパス

### GPU 優先（正規経路）

1. レイヤーが `Noise` フィルのとき、`ProceduralTextureComputePipeline` を取得または生成する。
2. `ProceduralTextureComputePipeline::generate(ctx, uav, noiseSettings_)` でノイズテクスチャを GPU に生成する。
3. 生成したテクスチャを既存の `drawSpriteTransformed` 相当のテクスチャ描画経路へ渡す。`QImage` / `QPainter` を経由しない。
4. カラーマッピングは初期段階では単色ティント（`noise * color`）とし、GPU シェーダーの乗算で行う。多段グラデーションマップは Phase 4 で検討する。

### CPU フォールバック

GPU パイプラインが利用できない場合、`ProceduralTextureGenerator::generate(settings_, image)` で `ImageF32x4_RGBA` を生成し、既存の float 画像描画経路へ渡す。QImage への変換は IO / thumbnail 境界に限定する。

### 決定性とキャッシュ

- 同一の `ProceduralTextureSettings` と source size に対して生成結果をキャッシュし、パラメータ変更またはキーフレーム評価値の変更時だけ再生成する。
- Offset / Rotation / Scale などがキーフレームで時間変化する場合、現フレームの評価値を確定してから設定を pack する。同じフレームの preview と Render Queue で同じ結果になるようにする。
- `draw()` と `toQImage()` のキャッシュキーに `ProceduralTextureSettings` の全フィールドを含める。

## プロパティグループ

`getLayerPropertyGroups()` の `Solid` グループに、Fill Mode = Noise のときだけ表示するサブ項目を追加する。

- Noise Type（kind）
- Voronoi Mode（voronoiMode、kind が Voronoi のとき）
- Seed / Scale X / Scale Y / Offset X / Offset Y / Rotation / Amplitude
- Octaves / Lacunarity / Gain（FBM のとき）
- Cell Jitter（Voronoi のとき）
- Domain Warp（on/off）、Warp Kind、Warp Amplitude
- Use Secondary、Secondary Kind、Blend Mode、Blend Weight
- Gamma / Invert
- Color Mapping（on/off）、Color A / Color B

すべて `persistentLayerProperty` で登録し、`setAnimatable(true)` にする。`setLayerPropertyValue()` に対応する分岐を追加する。

## 実装フェーズ

### Phase 0 — ベースラインとデータ契約

- `ArtifactSolidFillType` に `Noise` を追加し、`setFillType` / `setLayerPropertyValue` の clamp と分岐を更新する。
- `ArtifactSolidImageLayer::Impl` に `noiseSettings_` / `noiseColorA_` / `noiseColorB_` / `noiseColorMappingEnabled_` を追加する。
- `toJson()` / `fromJsonProperties()` に `solid.noise.*` の保存・復元を追加する（安定文字列 enum）。
- `ArtifactSolidLayerInitParams` にノイズパラメータの setter/getter を追加する。

完了条件: Noise フィルを選んで保存・再読込でき、パラメータが失われない。

### Phase 1 — CPU ノイズ描画

- `draw()` / `toQImage()` に `Noise` 分岐を追加する。
- `ProceduralTextureGenerator::generate(settings_, image)` で `ImageF32x4_RGBA` を生成し、既存の描画経路へ接続する。
- カラーマッピング（2色 lerp）を CPU で実装する。
- 生成結果を `cachedImage_` 相当のノイズキャッシュに保持し、パラメータ変更時のみ再生成する。

完了条件: 単色・グラデーションと同じ平面で Perlin / FBM / Voronoi が表示される。

### Phase 2 — GPU パイプライン接続

- `ArtifactSolidImageLayer` から `ProceduralTextureComputePipeline` を利用できる経路を確立する。
- `draw()` の正規経路を GPU 生成に切り替え、QImage を経由しない。
- CPU フォールバックは Phase 1 の実装を維持する。

完了条件: GPU 経路でノイズが表示され、CPU 経路と視覚的に一致する。

### Phase 3 — プリセットとプロパティ UI

- 既存6プリセットを呼び出す導線を追加する。
- `CreatePlaneLayerDialog` にノイズ設定面を追加する（Fill Mode = Noise のとき表示）。
- `getLayerPropertyGroups()` にノイズプロパティを追加する。
- Turbulence / WoodGrain プリセットを移植または近似実装する。

完了条件: コード編集なしでノイズタイプ・プリセット・パラメータを設定できる。

### Phase 4 — アニメーションと「wow」要素

- Offset / Rotation / Scale のキーフレーム駆動を確認する。
- Domain Warp + Secondary Blend の組み合わせが実用的に編集できることを確認する。
- カラーマッピングの多段グラデーション対応（任意の色停止点列）を検討する。

完了条件: 雲が流れ、大理石の脈が動く、布が揺れる、の代表例を平面1枚で作成できる。

### Phase 5 — 制作受入

- preview と Render Queue のノイズ結果が一致する。
- シーク・逆再生・フレームステップでノイズが飛ばない。
- 巨大平面サイズでメモリ・処理時間が許容範囲に収まる。
- 旧 JSON（fillType 0-5）の後方互換を確認する。

## 実装制約

- 新規の `QImage` は Qt API／入出力互換境界に限定し、描画・合成・転送の主経路へ増やさない。
- `QPainter`／Qt CompositionMode による新規ノイズ合成は行わない。ノイズ生成は既存の `ProceduralTextureGenerator` と専用 CPU 処理に寄せる。
- QtCSS、`QColorDialog`、新規シグナル／スロットを追加しない。
- 新規ファイルは原則作成せず、既存の `ArtifactSolidImageLayer` / `ArtifactLayerInitParams` / `CreatePlaneLayerDialog` を拡張する。
- `.ixx` の変更は公開契約に不可欠な場合だけとし、実装側（`.cppm`）で閉じられる変更を優先する。
- `ArtifactSolid2DLayer`（旧系統）は本マイルストーンで変更しない。ノイズフィルは `ArtifactSolidImageLayer` に一本化し、旧系統の廃止は別途確認する。
- `ProceduralTextureGenerator`（ArtifactCore）は共有資産なので、本マイルストーンではレイヤー側から消費するだけで、Core 側のアルゴリズム変更は最小限にする。やむを得ず Core を変更する場合は影響範囲を分離して確認する。

## 完了条件

- [ ] `ArtifactSolidFillType::Noise` を選んで保存・再読込でき、パラメータが失われない。
- [ ] Perlin / Simplex / FBM / Voronoi（3種）/ White / Value / Gradient の7種が表示できる。
- [ ] 既存6プリセット（Marble/Clouds/Cellular/Fabric/Terrain/Metal）が呼び出せる。
- [ ] Offset / Rotation / Scale のキーフレームでノイズが時間変化する。
- [ ] Domain Warp と Secondary Blend が機能する。
- [ ] カラーマッピング（2色）が機能する。
- [ ] GPU 経路で QImage を経由せず描画できる。
- [ ] preview と Render Queue の結果が一致する。
- [ ] 旧 JSON（fillType 0-5）が後方互換を保つ。

## 関連文書

- [`MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`](MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md)
- [`MILESTONE_SURFACE_FX_SYSTEM_2026-07-22.md`](MILESTONE_SURFACE_FX_SYSTEM_2026-07-22.md)
- [`../analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md`](../analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md)

## 次の実装判断

最初の実装対象は Phase 0 + Phase 1 とする。`ProceduralTextureSettings` をレイヤーに保持して CPU 経路でノイズを表示し、保存・復元とプロパティ編集が成立することを確認してから GPU 接続へ進む。ビルドや runtime 検証は、実行許可を得てから行う。
