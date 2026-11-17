# Precision Lab — Buffer Precision / Alpha Conversion Chain R&D

**最終更新:** 2026-11-17

画像エフェクト R&D「Buffer Precision / Alpha Conversion Chain」の評価用ラボ。
評価計画は `docs/planned/MILESTONE_BUFFER_PRECISION_ALPHA_CHAIN_RD_2026-11-17.md`。

## 目的

レイヤー入口の sRGB 8bit 量子化と、premultiplied alpha × sRGB の変換順序が
合成画像に与える影響を、実アプリに依存しない数式モデルと fixture で切り分ける。
根拠は `docs/analysis/IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md` の弱点 1〜4。

## 必要なもの

- Python 3
- NumPy
- OpenCV（`opencv-python` または同等の `cv2`）

## 実行

リポジトリルートから:

```powershell
python experiments/PrecisionLab/build_precision_fixtures.py
python experiments/PrecisionLab/analyze_precision_chain.py
python experiments/PrecisionLab/separate_contributions.py
```

出力（既定）:

- `experiments/PrecisionLab/fixtures/precision-rd1/`
- `experiments/PrecisionLab/captures/chain-rd1/`

## データ契約

- 測定源は float32 RGBA NPY（linear scene-referred、straight alpha）。
- PNG は表示用であり、0–1 クランプと sRGB OETF 済み。HDR fixture の表示画像は測定元と同じではない。
- 変換連鎖は Python 側のモデルであり、Artifact の实数値を証明しない。

## これは何を証明しないか

- 現行実装がどの変換順序を使うか（静的監査の仮説をモデル化しているだけ）。
- GPU dispatch、スワップチェーン、実アプリの readback／upload、性能。
- 数値差から直接的な画質合格／不合格を断定すること。目視レビューを併用する。
