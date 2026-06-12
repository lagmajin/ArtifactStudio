# Milestone: LuminescenceCaustics (光輪集光コースティクス)

> 2026-06-13

## Purpose

`LuminescenceCaustics` は、水面下やクリスタル表面で見えるような
**きらきらと揺らめく光の網目模様** を、画像の輪郭・ハイライト・方向情報から動的に生成する
creative effect である。

狙いは、外部の水面テクスチャを重ねるのではなく、
**被写体そのものが光を屈折させているように見せる** ことにある。

## Why This Exists

- 液体金属、魔法のオーラ、氷、クリスタルなどの素材感と相性が良い
- 既存の glow / blur / edge 系では出し切れない「集光の網目」を足せる
- 画像の輪郭やハイライトを入力にできるため、被写体依存の個性を出しやすい
- まずは stylized 版から始めれば、重い物理計算なしで十分な見た目を得られる

## Core Idea

この effect は、入力画像の以下を材料にする。

1. 輪郭
   - Sobel / gradient / contrast edge から光の源を作る
2. ハイライト
   - 明るい部分を集光の起点として扱う
3. 波
   - 周期関数やノイズで、光の網目を揺らす
4. 投影
   - 生成した模様を、元画像の周囲や背後へ additive に重ねる

## Existing Signals

- `ArtifactCore/src/Graphics/Effect/EdgeEchoEffect.cppm` に、輪郭起点の発想がすでにある
- `ArtifactCore/include/ImageProcessing/Halation.ixx` や `ChromaSpreadGlow` に、ハイライト起点の拡散/発光表現がある
- `Artifact/App/shaders/causticsCS.hlsl` と `Artifact/shaders/causticsCS.hlsl` に、既存の caustics 生成シェーダがある
- `Artifact/App/shaders/ShaderInterop_Renderer.h` に `texture_caustics_index` が既に用意されている

## Recommended First Slice

### Phase 1: Stylized Source Mask

**目標**: 入力画像から、コースティクスの種になる輝度マスクを作る。

- luminance threshold を作る
- edge strength を抽出する
- highlight region を weight 化する
- alpha と premultiplied 入力を壊さない

### Phase 2: Wave Field Synthesis

**目標**: 光の網目そのものを生成する。

- sine / fbm / interference で網目を作る
- 変調に時間を入れる
- 形状に応じて局所的に密度を変える
- 単調な繰り返しにならないように phase をずらす

### Phase 3: Material Coupling

**目標**: 被写体に一体化した見え方へ寄せる。

- 輪郭の向きに沿って模様を少し引っ張る
- 明部ほど強く、暗部ほど薄くする
- 角や曲率の高い部分で集光が増えるようにする
- 色は白固定ではなく、素材色を少し拾えるようにする

### Phase 4: Projection / Composite

**目標**: 周囲へ投影される光として見せる。

- additive composite を基本にする
- glow / bloom と競合しないよう強度を分ける
- 背景が明るくても読めるコントラストを確保する
- 必要なら final effect 側に逃がせる形を残す

## In Scope

- 輪郭ベースの光網生成
- ハイライトベースの光増幅
- 時間変化する揺らぎ
- 液体金属 / クリスタル / 魔法表現の stylized look
- additive / screen 系の合成
- CPU reference と GPU path の分割

## Out Of Scope

- 厳密な物理屈折
- 水面法線の実シミュレーション
- caustics の完全な光学再現
- 大規模なマテリアル/光源編集 UI
- 常時高負荷のレイトレ依存

## Implementation Notes

- 最初は `Rasterizer` または `Final Effect` のどちらか 1 本で始める
- 被写体依存の見え方を重視するため、入力の luminance / edge / alpha を最小限に使う
- 既存の `causticsCS.hlsl` があるので、まずはそこを流用して stylized パスを作る
- 生成結果は `texture_caustics_index` 経由で別パスへ渡す構成も検討できる

## Success Criteria

- 1 枚の入力から、コースティクスらしい網目模様が見える
- 形のある被写体ほど個性的な模様になる
- 近い素材でも、輪郭の違いで表情が変わる
- glow だけでは出せない「集光感」が出る
- 既存の effect stack を壊さずに段階導入できる

## Likely Touch Points

- [ArtifactCore/src/Graphics/Effect/EdgeEchoEffect.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/Effect/EdgeEchoEffect.cppm)
- [ArtifactCore/include/ImageProcessing/Halation.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Halation.ixx)
- [ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cpp](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cpp)
- [Artifact/App/shaders/causticsCS.hlsl](X:/Dev/ArtifactStudio/Artifact/App/shaders/causticsCS.hlsl)
- [Artifact/App/shaders/ShaderInterop_Renderer.h](X:/Dev/ArtifactStudio/Artifact/App/shaders/ShaderInterop_Renderer.h)
- [docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md)

## Related

- [docs/EFFECT_SYSTEM_SPECIFICATION.md](X:/Dev/ArtifactStudio/docs/EFFECT_SYSTEM_SPECIFICATION.md)
- [docs/MILESTONE_EFFECT_SYSTEM_BRIDGE_2026-05-25.md](X:/Dev/ArtifactStudio/docs/MILESTONE_EFFECT_SYSTEM_BRIDGE_2026-05-25.md)
- [ArtifactCore/docs/PREMIUM_EFFECTS_MEMO.md](X:/Dev/ArtifactStudio/ArtifactCore/docs/PREMIUM_EFFECTS_MEMO.md)
