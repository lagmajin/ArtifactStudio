# Image Correction Suite

**最終更新:** 2026-11-16
**ステータス:** 初期実装（決定論的fixture・reference候補・共通ROI計測を用意。製品 capture と視覚判定は未実施）

優先R&D候補2〜5および10〜11向けの軽量オフライン研究suite。製品アルゴリズムを移植したものではなく、幾何・色応答の既知入力に対する計測土台。候補結果は製品品質を証明しない。全候補の順序と受入範囲は [`IMAGE_RND_PRIORITY_PORTFOLIO_2026-11-16.md`](../../docs/planned/IMAGE_RND_PRIORITY_PORTFOLIO_2026-11-16.md) を参照。

## 実行

```powershell
python experiments/ImageCorrectionSuite/run_rd0.py
```

出力は `captures/rd0/`。決定論的fixtureはseed 20261116で生成する。JSON manifestにfixture・マスクのhashと条件を記録する。

## テーマ別の初期範囲

- 自動修復: deterministic scratch/dust corruption と clean truth。小傷・細線損傷を評価する。これはAI repairや製品修復機能ではない。
- モアレ: synthetic periodic interferenceを抑える周波数-domain low-pass referenceを複数強度で測る。細部損失も記録する。
- 色収差: grid/edge chartを使い、RGB channel radial displacement referenceを測る。Core `ChromaSpread` mirrorとの対応は未実施。
- レンズ歪曲: grid chartに樽型/糸巻き型を加えて既知逆写像で戻すreferenceを測る。Core `makeOpticsCompensation` との一致は未実施。
- 色合わせ: source/reference chart対でReinhard風 mean/std transferと保護色ROIを記述測定する。Core API出力とは未比較。
- 配色抽出: SmartPaletteAnalyzerと既存alpha-weighted median-cut研究を監査し、ここでは色数/色面積の入力条件chartを追加する。抽出器を製品移植しない。

## 解釈

統計は正解と測定器の sanity check に使う。gateは未確定、目視判定なし、実写・HDR/alpha・GPU/CPU parity・性能・実アプリ経路は未評価。

## 既存研究への接続

- 色収差（候補4）: `experiments/RetroImageLab/tools/core_chromatic_angle_study.py` と `retro_lab.py --chromatic-study / --radial-chromatic-study`。Core integer shift の角度応答、clamp、alpha/hidden-RGBを含む資産がある。ここでは再実装しない。
- 配色抽出（候補11）: `experiments/RetroImageLab/tools/adaptive_palette_weighting_study.py`、`adaptive_palette_temporal_stability_study.py`。製品側 `ArtifactCore::SmartPaletteAnalyzer` との同条件比較は未実施。
- ディザリング（候補13）: `retro_lab.py --dither-pattern-study` および `RESEARCH_NOTES.md` の domain / phase / temporal studies。
- ドット絵化（候補14）: `retro_lab.py --alpha-pixel-art-crop-study` と `tools/adaptive_palette_resize_phase_study.py`。alpha境界・crop phaseを含む既存研究がある。
