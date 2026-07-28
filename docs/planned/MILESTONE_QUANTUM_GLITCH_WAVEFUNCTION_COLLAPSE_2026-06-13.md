# Milestone: QuantumGlitch / WavefunctionCollapse

> 2026-06-13

## Purpose

`QuantumGlitch / WavefunctionCollapse` は、入力画像を壊すのではなく、
**細かいパターンタイルへ分解し、隣接ルールと確率遷移に従って再構成する**
stylistic effect である。

狙いは、JPEG 破損やアナログノイズのような「破壊型グリッチ」ではなく、
**自己修復・自己崩壊を繰り返す抽象コラージュ** を作ることにある。

## Why This Exists

- 既存の glitch 系との差別化が明確
- 画像の輪郭や色相を保ちながら、構造だけを再配置できる
- SF / 生成アート / UI トランジション / 未来的なビジュアルに使いやすい
- パターン分解と再配置なので、素材依存の表情を出しやすい

## Core Idea

この effect は、入力画像から次の要素を抽出する。

1. タイル化
   - 画像を小さなパッチに分割する
2. 量子化
   - 色 / 輝度 / エッジ方向を離散化する
3. 隣接ルール
   - タイル同士の遷移可能性をルール化する
4. 再構成
   - 確率的に tile を並べ直し、画像を再生成する
5. 崩壊と修復
   - 時間変化でルールが少しずつ崩れ、また収束する

## Existing Signals

- 既存コードには、`mosaic` / `halftone` / `noise` / `glitch` 系の近い表現がある
- `ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cppm` や `OpenCV/Glow` には、合成寄りの画作りがある
- `ArtifactCore/src/Graphics/Effect/LightPressureEffect.cppm` には、輝度と局所勾配を使う stylized warping がある
- `ArtifactCore/include/ImageProcessing/ChromaSpread.ixx` のように、局所タイル／分散処理へ寄せやすい土台がある

## Recommended First Slice

### Phase 1: Tile Extractor

**目標**: 入力画像を小さな tile へ分解し、局所特徴を持たせる。

- fixed size tile で分割する
- tile ごとに平均色 / 分散 / edge strength を取る
- alpha と premultiplied state を壊さない
- tile のメタデータを compact に持つ

### Phase 2: Adjacency Rule Builder

**目標**: どの tile が隣接可能かを決める。

- 色差のしきい値を持つ
- edge orientation の相性を入れる
- high-frequency tile と smooth tile を分ける
- ルールが単調になりすぎないように少量の randomness を入れる

### Phase 3: Collapse / Reconstruction

**目標**: tile を確率的に並べ直し、再構成する。

- 近傍制約を満たす候補から選ぶ
- 行単位だけでなく局所領域単位で再配置する
- 画面全体を一度に崩さず、局所的に収束させる
- 崩壊中も元画像の輪郭が何となく追える状態を保つ

### Phase 4: Temporal Instability

**目標**: 自己構成が揺らぎ続ける感じを出す。

- time-based rule perturbation
- tile swap / reseed / phase shift
- 短い周期で安定と崩壊を行き来する
- 画面遷移やループ演出に使えるようにする

### Phase 5: Stylized Output Modes

**目標**: 用途ごとに見た目を分ける。

- `mosaic collage`
- `quantum repair`
- `abstract glitch`
- `structural drift`

## In Scope

- tile 分解
- 色 / 輝度 / エッジの量子化
- 隣接制約に基づく再構成
- 時間変化する崩壊と収束
- stylized glitch / collage / SF transition
- CPU reference と GPU 実装の分離

## Out Of Scope

- 厳密な量子力学シミュレーション
- 本物の Wave Function Collapse の完全一般化
- 任意サイズでの重いグローバル最適化
- 破綻ゼロの完全復元
- 大規模な編集 UI

## Implementation Notes

- 最初は `Rasterizer` として単独 effect にするのが安全
- まずは tile サイズと隣接ルールの 2 軸で十分
- WFC の厳密版にこだわらず、`constraint solving + probabilistic tile sampling` として始める
- 必要になってから final effect や bus に広げる

## Success Criteria

- 入力画像の構造を残しつつ、抽象的なモザイク再構成ができる
- 単なるノイズではなく、ルール感のある崩壊に見える
- 色だけでなく輪郭の記憶が残る
- 時間で「収束」と「乱れ」を切り替えられる
- 既存の glitch 系と見た目の役割がかぶりすぎない

## Likely Touch Points

- [ArtifactCore/src/Graphics/Effect/LightPressureEffect.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/Effect/LightPressureEffect.cppm)
- [ArtifactCore/include/ImageProcessing/ChromaSpread.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/ChromaSpread.ixx)
- [ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cppm)
- [Artifact/src/Effects/AutoMosaicEffect.cppm](X:/Dev/ArtifactStudio/Artifact/src/Effects/AutoMosaicEffect.cppm)
- [ArtifactCore/include/ImageProcessing/NoiseImageGenerator.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/NoiseImageGenerator.ixx)
- [Artifact/include/Effects/Mosaic/MosaicEffect.ixx](X:/Dev/ArtifactStudio/Artifact/include/Effects/Mosaic/MosaicEffect.ixx)

## Related

- [docs/planned/MILESTONES_BACKLOG.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
- [docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md)
- [ArtifactCore/docs/PREMIUM_EFFECTS_MEMO.md](X:/Dev/ArtifactStudio/ArtifactCore/docs/PREMIUM_EFFECTS_MEMO.md)

## Static Audit (2026-07-25)

現状のコードには既存の digital／vector-flow／transition glitch、mosaic 等の近縁実装はあるが、`QuantumGlitch` または `WavefunctionCollapse` としての専用 effect、tile extractor、adjacency rule builder、collapse／reconstruction、時間的な収束・崩壊モデルは確認できなかった。検索で得られた `glitch` 実装は、主に displacement、block corruption、RGB separation、noise、vector flow など既存の破壊型／変形型処理である。

したがって Phase 1 の tile metadata 生成から未着手で、GPU／CPU reference 分離、登録、Inspector property、出力 mode、success criteria も未達。これは関連実装の再利用候補を確認した段階であり、マイルストーンは Planned／未実装のままとする。

確認対象:

- `Artifact/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm`
- `ArtifactCore/src/ImageProcessing/VectorFlowGlitch.cppm`
- `Artifact/src/Effect/ArtifactTransition.cppm`
- `Artifact/src/Service/ArtifactEffectService.cppm`
- `ArtifactCore/src/Graphics/Effect/GlitchCreativeEffect.cppm`
