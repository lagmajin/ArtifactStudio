# Cinematic Looks Lab

**最終更新:** 2026-11-16

静止画の cinematic look を、チャートとライセンス記録済みの実写素材で比較するオフライン R&D ラボです。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) を参照してください。

## 現行コードとの関係

Artifact には `LUTEffect` と複数の `BuiltinLUTs` が既にあります。一方、`ArtifactLooksPresetBrowser` は現状 placeholder thumbnails を生成し、LUT 適用との接続は確認できていません。このラボは同ブラウザーを模擬せず、現在の LUT 画像応答を評価します。

## 実行

Python 3.10+ と `requirements.txt` の依存が必要です。Artifact の起動、ビルド、CMake、テストは行いません。

```powershell
python experiments/CinematicLooksLab/cinematic_looks_lab.py --rd rd0
```

出力は `captures/<rd>/` です。チャートと写真候補に各 Look を適用し、レビュー用 comparison sheet、入力/出力画像、ROI、pixel distribution、応答差分、manifest、未確定 gate を保存します。初回 `rd0` は baseline と候補の出力・統計を記録しますが、品質 gate や勝者を決めません。人が画像を確認した後、評価計画に従って `gates.json` の閾値を根拠付きで固定してから `rd1` を実施します。

## 出力上の制限

Python harness は `ArtifactCore/src/Color/ColorLUT.cpp` の 17³ `BuiltinLUTs` 生成式と `ColorLUT` の trilinear sampler を mirror し、manifest に source SHA-256 を記録します。これは現行の CPU 式を独立計算した証拠であり、C++ 関数を直接実行した数値一致テストではありません。C++ source が変更された場合、mirror の更新とサンプル点 differential 契約を再確認するまで古い出力を現行結果として扱わないでください。Composition の適用、GPU parity、色管理経路も証明しません。

実写素材の provenance は [`../GlowLab/assets/SOURCES.md`](../GlowLab/assets/SOURCES.md) にあります。分析は 8-bit sRGB 表示用画像で行うため HDR/alpha の評価は未実施です。HDR/alpha の数値評価は実データ向けに別途追加が必要です。性能は NumPy/Python reference operation のみを測定し、製品性能ではありません。
