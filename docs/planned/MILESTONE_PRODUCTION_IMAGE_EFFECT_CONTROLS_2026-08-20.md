# Production Image Effect Controls

**最終更新:** 2026-08-20

**ステータス:** In Progress

## 目的

画像エフェクトを「数がある」状態から、静止画・連番画像の実制作で再現可能に調整、アニメーション、比較できる状態へ進める。GPU を通常経路とする方針を維持し、CPU は比較・安全な fallback として残す。

## 現状と判断

- factory 登録は約141件、`getProperties()` 実装は **98/101**（BlurEffect / NoiseEffect / SurfaceFXEffect の3件が未実装）。
- 2026-08-22 の実測によるパラメータ数分布（getProperties body 内の property 登録呼び出しカウント）:
  - **8+ params (production): 18 effects** — Glow(12), ApertureShapeBlur(10), ChromaKey(9), RadioWaves(9), LiftGammaGain(9), LensDistortion(9) 等
  - **5–7 params (standard): 39 effects** — 大半の標準エフェクト
  - **3–4 params (basic): 32 effects** — Exposure(3), Sharpen(3), Kuwahara(3) 等
  - **1–2 params (minimal): 9 effects** — FreezeFrame(1), LumaKey(1), Invert(2), Grayscale(2) 等。機能が単純なため妥当なものも含む
- Levels、Curves、IBK Keyer、Glow、Pixel Sort Pro は実制作に近い項目を持つ。
- パラメータ「数」自体は中央値5前後で標準的だが、PIEC-2 のメタデータ正規化（tooltip/unit/animation flag/soft range）が全 effect の ~5% のみ完了しているのが主要ギャップ。
- 一部 GPU-only effect（BlurEffect等）は cbuffer を直接持ち getProperties() を経由しないため Inspector から操作不可。
- preview / GPU / CPU / render queue の画素 parity、HDR・premultiplied alpha の runtime 受入は未完了または未検証である。

## 範囲

- `ArtifactAbstractEffect` の共通編集契約と共通合成。
- Inspector / Property Widget における共通プロパティの編集・キーフレーム反映。
- Blur / Bloom、Keying、Lens / Distortion、Temporal、Color の優先 effect 群のパラメータ拡張。
- GPU-first のパラメータ反映と CPU fallback の比較可能性。

## 非目標

- 全 effect を一度に同じ項目数へ揃えること。
- ソフトレンダラーの新機能化。
- 新しい signal / slot、QtCSS、QImage hot path の導入。
- runtime 検証なしで production-ready と宣言すること。

## フェーズ

### PIEC-1: 共通制作コントロール

- 全 effect に Effect Enabled、Effect Mix、Allow Overscan、既存の effect region / mask を一貫して公開する。
- Mix は effect 実装固有ではなく、処理済み結果と入力を共通合成して 0–1 の範囲へ制限する。
- shared property は Inspector と layer Property Widget から編集・アニメーション可能にする。

**完了条件:** 代表 effect を含む全 effect で共通 controls が同じ名前・意味・安全範囲で働く。

**進捗:** 実装済み・runtime未検証。Effect Mix / Enabled / Allow Overscan の契約と共通合成を追加し、既存 Property Widget、project保存、preset、effect複製、Inspector copy/paste、WebUI状態出力、Effect Service（layer / composition）を共通契約へ接続した。実機での preview / export / keyframe 受入は PIEC-5 で確認する。

### PIEC-2: パラメータ品質の正規化

- 各公開パラメータに stable name、表示名、既定値、hard/soft range、step、単位、tooltip、アニメーション可否を付与する。
- 数値 enum を可能な範囲で選択肢として表現し、曖昧な生 float を減らす。
- effect 固有の Mix がある場合は PIEC-1 の Effect Mix と二重の意味を持たないよう整理する。

**完了条件:** 優先 effect 群に安全な編集範囲と保存・キーフレームに耐える stable property identity がある。

**進捗:** Blur、Edge Bloom、Chroma Key、Lens Distortion、Displacement Map、Aperture Shape Blur、Temporal Denoise、Color Wheels、Drop Shadow の既存項目へ表示名、既定値、hard/soft range、step、単位、tooltip を追加した。Blurは従来非公開だった Iterations / Mode / Edge Threshold / Premultiplied Alpha も公開した。2026-08-20 に EdgeBloom、ApertureShapeBlur、ChromaKey、LensDistortion、TemporalDenoise の表示名・min/max・tooltip の不足を正規化した。全 effect 群への展開は継続する。

### PIEC-3: 高頻度 effect の拡張

1. Blur / Bloom: threshold knee、quality/downsample、channel、edge mode、overscan。
2. Keying: black/white clip、matte choke/feather、despill controls、edge/detail preview、入力色空間。
3. Lens / Distortion: radial/tangential coefficients、anamorphic、edge fill、profile/overscan。
4. Temporal: scene-cut、motion-adaptive、ghost suppression、luma/chroma、confidence preview。
5. Color: linear/log 前提、HDR 値、luma preservation、LUT/working-space との責務分離。

**完了条件:** 各群で代表 effect 1件以上が実制作の調整・比較に必要な controls を持つ。

**進捗:** Chroma Key に Matte Black Clip / Matte White Clip、opaque grayscale の Preview Matte、RGB距離／輝度距離を切り替える Luma Only、Spill Desaturation を追加し、Similarity / Edge Softness の出力から matte の下限・上限と despill 強度を明示的に調整できるようにした。Lens Distortion には CPU/GPU 同一式で Tangential X / Y（decentering）、Radial Quadratic（2次半径係数）、Transparent Edges を追加した。Edge Bloom には CPU/GPU 共通の Threshold Softness、Edge Boost、Radius サンプル距離、Draft／Standard／High の Quality を追加した。Temporal Denoise は sampled average-luma scene-cut rejection、分散ベースの Motion Adaptive、Ghost Suppression の調整を持つ。keyer の複数 sample、色空間指定、Lens profile の読込 UI、GPU runtime parity は未実装または未検証。

### PIEC-4: 制作診断と比較

- before/after、effect contribution、alpha / matte / channel preview、clipping と範囲外警告を整備する。
- GPU / CPU fallback の理由、effect cost、preview / export 差分を追跡できるようにする。

**完了条件:** 見た目の問題をパラメータ、色管理、backend 差のどれとして調べるべきか判断できる。

### PIEC-5: Production Acceptance

- 静止画・連番画像に対し、HDR値、透明境界、mask、複数 effect stack、GPU/CPU fallback を含む固定 fixture を定義する。
- Preview / Render Queue での output parity、保存・再読込、keyframe を runtime で確認する。

**完了条件:** 優先 effect 群を production-ready と判断できる実測・画素比較の記録がある。

## 依存・関連

- [GPU Effect Parity](MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md)
- [Color Grading](MILESTONE_COLOR_GRADING_2026-03-29.md)
- [Effect-level Mask](FEATURE_EFFECT_LEVEL_MASK_2026-07-02.md)
- [Color / Alpha Contract](MILESTONE_COLOR_ALPHA_CONTRACT_UNIFICATION_2026-07-18.md)

## 次の実装単位

PIEC-2 の代表5 effect（`EdgeBloomEffect`、`ApertureShapeBlurEffect`、`ChromaKeyEffect`、`LensDistortionEffect`、`TemporalDenoiseEffect`）の property metadata 正規化は完了した。次は PIEC-3 として keyer の複数 sample / 色空間、Lens profile、Temporal の motion-adaptive / ghost suppression、Blur/Bloom の quality / edge mode を effect 群ごとに小さく追加する。
