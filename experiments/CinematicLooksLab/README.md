# Cinematic Looks Lab

**最終更新:** 2026-11-16

静止画の cinematic look を、チャートとライセンス記録済み実写素材で比較するオフライン R&D ラボです。LUTEffect の9 builtin LUT に加え、ColorBalanceEffect / TritoneEffect の Cinematic lane も pinned source equation mirror で5素材測定しました。結果は `captures/rd0_effect_extension/` にあります。これは C++ differential や製品画像評価ではなく、人手 blind review と runtime 検証は未完了です。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) を参照してください。

## 現行コードとの関係

Artifact には `LUTEffect` と複数の `BuiltinLUTs` が既にあります。汎用 effect preset file API は LUT effect の preset index・LUT file path・intensity を含む effect state の保存／既存 effect object への復元を扱います。一方、静的監査では Composition-level effect の project reload が LUTEffect を concrete type として復元しない経路を確認しました。レイヤーの effect restore は別の factory-backed 経路です。詳細は [`captures/rd0/findings.json`](captures/rd0/findings.json) の `RD0-COMPOSITION-LUT-RESTORE-01` を参照してください。`ArtifactLooksPresetBrowser` は現状 placeholder thumbnails を生成し、LUT 適用との接続は確認できていません。このラボは同ブラウザーを模擬せず、現在の LUT 画像応答を評価します。

ブラウザーの24 preset ID と9 builtin LUT 名を source hash 付きで一覧化した [`REGISTRY_AUDIT.json`](REGISTRY_AUDIT.json) もあります。意味が似た名称から対応先を推測せず、明示 mapping がないものは未対応として記録しています。

## 実行

Python 3.10+ と `requirements.txt` の依存が必要です。Artifact の起動、ビルド、CMake、テストは行いません。

```powershell
python experiments/CinematicLooksLab/cinematic_looks_lab.py --rd rd0
python experiments/CinematicLooksLab/contract_probe.py
python experiments/CinematicLooksLab/make_alpha_association_fixture.py
python experiments/CinematicLooksLab/cinematic_effects_extension.py
```

LUT 実験は `captures/<rd>/` に素材ごとの統計、chart diagnostics、通常／blind contact sheets、full-resolution ROI crops、manifest、未確定 gate を保存します。Color Balance / Tritone extension は同じ5素材に baseline、LUT Cinematic、Color Balance Cinematic 0.5/1、Tritone Cinematic 0.5/1、TealAndOrange 1 を適用し、応答統計・blind contact sheets・空欄票を `captures/rd0_effect_extension/` に作成します。RGB code 差は記述指標で、知覚色差ではありません。式 mirror、float domain probe、alpha sweep は C++ 実行や実アプリ出力ではありません。現行の source candidate findings と開いた gate は `captures/rd0/findings.json` / `gates.json` に残し、人手レビューと runtime capture 前に品質判定・勝者を決めません。

## 出力上の制限

Python harness は `ArtifactCore/src/Color/ColorLUT.cpp` の 17³ `BuiltinLUTs` 生成式（各 `setValue` 格子点の clamp を含む）と `ColorLUT` の trilinear sampler を mirror し、manifest に harness 自身・C++ source・pinned [`SOURCE_CONTRACT.json`](SOURCE_CONTRACT.json) の SHA-256 を記録します。ブラインド素材順は case ごとの導出 seed と shuffle 方法も記録します。pin と一致しない C++ source に対しては古い mirror が誤って実行されないよう停止します。source を更新する場合は変更内容を調査し、mirror と証跡を更新してから pin を変更してください。これは現行の CPU 式を独立計算した証拠であり、C++ 関数を直接実行した数値一致テストではありません。Composition の適用、GPU parity、色管理経路も証明しません。

夜景素材の provenance は [`../GlowLab/assets/SOURCES.md`](../GlowLab/assets/SOURCES.md)、人物・森林素材は [`assets/SOURCES.md`](assets/SOURCES.md) にあります。LUT と effect extension の対応表はそれぞれの blind directory に分離されています。8-bit sRGB 画像には alpha を含めず、HDR は式 probe のみで実画像・runtime品質は未評価です。ColorBalance の source path は premultiplied RGB を unpremultiply/process/repremultiply し、Tritone の CPU path は RGB をそのまま処理します。extension probe では alpha 0.1 の最大 straight-color差が ColorBalance 約2.4e-7、Tritone Cinematic 約0.349、TealAndOrange 約0.517でした。これは pinned source-equation mirror の値で、C++ 一致、実経路、背景合成品質を証明しません。性能は Python reference operation で製品性能ではありません。

Alpha association の実経路確認用に、複数 RGB 色と固定 alpha patch / 水平 alpha ramp を持つ synthetic straight-alpha PNG [`assets/alpha-association-chart.png`](assets/alpha-association-chart.png) を追加しました。条件と SHA-256 は隣接 JSON にあります。runtime への適用・capture はまだ行っていません。
