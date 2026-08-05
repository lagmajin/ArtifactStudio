> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_GLITCH_EFFECTS_PROPOSAL_2026-06-13.md](MILESTONE_GLITCH_EFFECTS_PROPOSAL_2026-06-13.md)

# Milestone: AnisotropicFlowBlur (異方性オプティカルフロー・ブラー)

> 2026-06-13

## Purpose

`AnisotropicFlowBlur` は、画像の構造テンソルから得た局所方向場に沿ってのみボケる、
**輪郭追従型の異方性ブラー**である。

ただ丸くぼかすのではなく、髪の毛、水流、木目、炎、布の繊維のような
「流れのあるテクスチャ」に沿って、楕円状のサンプルを動かすことで、
有機的で読みやすいブラーを作る。

## Why This Exists

- 一般的な blur よりも、素材の流れを壊しにくい
- 肌、髪、炎、煙、布などの方向性を保ったままソフト化できる
- ゴッホ風、水彩風、美肌補正風の look に繋げやすい
- すでに `StructureTensor` と組み合わせる実装があるので、土台が明確

## Existing Signals

- `ArtifactCore/src/ImageProcessing/AnisotropicFlowBlur.cppm` に CPU 実装がある
- `ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx` に公開 API がある
- `ArtifactCore/include/ImageProcessing/StructureTensor.ixx` に方向場の解析基盤がある
- `ArtifactCore/include/ImageProcessing/Distortion.ixx` に補助的な変形経路がある
- `ArtifactCore/include/ImageProcessing/VectorFlowGlitch.ixx` と共通の方向場思想を共有できる

## Core Idea

この effect は次の 3 層で構成する。

1. Direction Field
   - structure tensor から局所の主方向を得る
2. Oriented Sampling
   - 方向に沿って長く、直交方向には短いカーネルでサンプリングする
3. Flow Preservation
   - texture の流れを残しつつ、粗さだけを落とす

## Recommended First Slice

### Phase 1: Directional Blur Base

**目標**: 方向場に沿う基本的な異方性ブラーを安定化する。

- structure tensor の angle / coherence を使う
- ブラー半径を coherence で制御する
- エッジに直交する成分を抑え、流れの向きは残す

### Phase 2: Texture-Aware Softening

**目標**: 素材ごとに見え方を変える。

- 髪、布、木目、水流のような素材を壊しにくくする
- 肌の微細なざらつきだけを落とす
- 強い輪郭は残し、ディテールだけを柔らかくする

### Phase 3: Stylized Presets

**目標**: 画作りの用途別に見た目を分ける。

- `oil paint`
- `watercolor`
- `beauty softening`
- `flow-preserve blur`

### Phase 4: Preview / Final Parity

**目標**: プレビューと最終レンダで見え方を揃える。

- 低解像度 preview と高解像度 final の差を抑える
- 既存の render path と矛盾しないようにする
- 速度と品質の切り替えをわかりやすくする

## In Scope

- structure tensor ベースの方向場ブラー
- 楕円カーネル的な局所サンプリング
- edge / texture direction の保持
- 肌や繊維の自然な softening
- preview / final の両対応

## Out Of Scope

- 完全な学術的 LIC 実装
- 3D volume ブラー
- GPU 専用の超高精度畳み込みだけに依存する実装
- 方向場を無視した単純な高周波ノイズ混在ブラー

## Implementation Notes

- 既に `StructureTensor` を使っているので、方向場の取得は再利用しやすい
- まずは CPU 版の挙動を基準に、見た目の安定性を固める
- ブラー量を上げすぎるとディテールが潰れるので、coherence による抑制が重要
- `VectorFlowGlitch` とは逆に、こちらは「壊す」のではなく「流れを残して柔らかくする」側に寄せる

## Success Criteria

- 単純な平均化 blur ではなく、方向性のあるボケに見える
- 髪、木目、水流の流れが自然に残る
- 肌のノイズだけを落として、輪郭は読みやすい
- Sapphire 的な fixed-angle blur よりも、素材依存の賢い見え方になる
- 既存の core image processing 資産を活かして実運用できる

## Likely Touch Points

- [ArtifactCore/src/ImageProcessing/AnisotropicFlowBlur.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/AnisotropicFlowBlur.cppm)
- [ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx)
- [ArtifactCore/include/ImageProcessing/StructureTensor.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/StructureTensor.ixx)
- [ArtifactCore/include/ImageProcessing/Distortion.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)
- [ArtifactCore/include/ImageProcessing/VectorFlowGlitch.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/VectorFlowGlitch.ixx)

## Related

- [docs/planned/MILESTONE_VECTOR_FLOW_GLITCH_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VECTOR_FLOW_GLITCH_2026-06-13.md)
- [docs/planned/MILESTONE_REACTION_DIFFUSION_STYLIZER_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_REACTION_DIFFUSION_STYLIZER_2026-06-13.md)

## 2026-07-25 実装監査

判定: Phase 1 の CPU 基盤とエフェクト登録は実装済み。Phase 2〜4 と GPU / preview-final parity は未確認・未接続。

- `ArtifactCore::AnisotropicFlowBlur` は `StructureTensor` の angle / coherence を使い、局所方向に沿った 9 サンプルの CPU 異方性フィルタを実装している。
- `Artifact.Effect.Rasterizer.AnisotropicFlowBlur` は Rasterizer CPU effect として登録され、Blur Amount / Tensor Noise Scale / Tensor Integration Scale / Edge Adherence を編集できる。
- 現在のサンプリングは固定 9 点・固定 Gaussian weight で、coherence は横方向成分の抑制に使われる。素材別の texture-aware softening や oil paint / watercolor 等の preset は未実装。
- GPU 実装、preview / final の品質整合、実際の render path への接続はソース上で確認できない。`OpticalFlowBlur` の公開 API は別系統で GPU 非対応の宣言に留まる。
- 次の実装単位は、既存 CPU 経路を基準に coherence と半径の評価を固定し、preview / final の適用契約を決めること。

ビルド・実行確認はリポジトリ方針により未実施。
