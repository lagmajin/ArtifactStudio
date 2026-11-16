# Cinematic Looks Lab

**最終更新:** 2026-11-16

静止画の cinematic look を、チャートとライセンス記録済みの実写素材で比較するオフライン R&D ラボです。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) を参照してください。

## 現行コードとの関係

Artifact には `LUTEffect` と複数の `BuiltinLUTs` が既にあります。一方、`ArtifactLooksPresetBrowser` は現状 placeholder thumbnails を生成し、LUT 適用との接続は確認できていません。このラボは同ブラウザーを模擬せず、現在の LUT 画像応答を評価します。

ブラウザーの24 preset ID と9 builtin LUT 名を source hash 付きで一覧化した [`REGISTRY_AUDIT.json`](REGISTRY_AUDIT.json) もあります。意味が似た名称から対応先を推測せず、明示 mapping がないものは未対応として記録しています。

## 実行

Python 3.10+ と `requirements.txt` の依存が必要です。Artifact の起動、ビルド、CMake、テストは行いません。

```powershell
python experiments/CinematicLooksLab/cinematic_looks_lab.py --rd rd0
python experiments/CinematicLooksLab/contract_probe.py
```

出力は `captures/<rd>/` です。チャートと写真候補に各 Look を適用し、レビュー用 comparison sheet、入力/出力画像、ROI、pixel distribution、応答差分、manifest、未確定 gate を保存します。別の float 契約 probe は9 builtin LUTに対し、通常値・範囲外値・非有限値・alphaを含む6入力を強度0/0.5/1で評価し、`captures/rd0/float_contract_probe.json` に式 mirror の応答を保存します。初回 `rd0` は baseline と候補の出力・統計を記録しますが、品質 gate や勝者を決めません。人が画像を確認した後、評価計画に従って `gates.json` の閾値を根拠付きで固定してから `rd1` を実施します。

## 出力上の制限

Python harness は `ArtifactCore/src/Color/ColorLUT.cpp` の 17³ `BuiltinLUTs` 生成式（各 `setValue` 格子点の clamp を含む）と `ColorLUT` の trilinear sampler を mirror し、manifest に source SHA-256 と pinned [`SOURCE_CONTRACT.json`](SOURCE_CONTRACT.json) の hash を記録します。pin と一致しない C++ source に対しては古い mirror が誤って実行されないよう停止します。source を更新する場合は変更内容を調査し、mirror と証跡を更新してから pin を変更してください。これは現行の CPU 式を独立計算した証拠であり、C++ 関数を直接実行した数値一致テストではありません。Composition の適用、GPU parity、色管理経路も証明しません。

夜景素材の provenance は [`../GlowLab/assets/SOURCES.md`](../GlowLab/assets/SOURCES.md)、人物・森林素材は [`assets/SOURCES.md`](assets/SOURCES.md) にあります。候補名を隠した接触シートと対応表 `blind_review_key.json` も出力します。分析は 8-bit sRGB 表示用画像で行うため HDR/alpha の評価は未実施です。HDR/alpha の数値評価は実データ向けに別途追加が必要です。性能は NumPy/Python reference operation のみを測定し、製品性能ではありません。
