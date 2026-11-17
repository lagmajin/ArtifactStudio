# Cinematic Looks Lab

**最終更新:** 2026-11-17

静止画の cinematic look を、チャートとライセンス記録済み実写素材で比較するオフライン R&D ラボです。LUTEffectの9 builtin LUTに加え、ColorBalanceEffect / TritoneEffectのCinematic laneをCPU/GPU source-equation mirrorで5素材測定しました。rd0ではTritoneのCPU/GPU式差を検出し、製品HLSLをCPU式へ寄せた後、rd1 mirrorでは5素材×4設定の量子化コード差が0、rd2ではTritone alpha sweepのCPU/GPU wrapper mirror差も最大約1.2e-7でした。これはshader/C++実行の差分ではありません。rd0/rd1/rd2証跡は各`captures/rd*_effect_extension/`に保持し、LUT alpha wrapper更新後のsource equation probeは`captures/rd1_lut_alpha_wrapper/`、run-ID manifestを修正した再実行は`captures/rd3_lut_alpha_wrapper/`に保存しました。人手blind review・runtime検証・品質ゲートは未完了です。評価計画は[`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)を参照してください。

## 現行コードとの関係

Artifact には `LUTEffect` と複数の `BuiltinLUTs` が既にあります。汎用 effect preset file API は LUT effect の preset index・LUT file path・intensity を含む effect state の保存／既存 effect object への復元を扱います。rd0監査ではComposition-level effect reloadがLUTEffectをconcrete typeに復元しない経路を確認し、follow-upでLUT / Color Balance / TritoneのComposition deserializer factory分岐を追加しました。詳細は [`captures/rd0/findings.json`](captures/rd0/findings.json) の `RD0-COMPOSITION-LUT-RESTORE-01` を参照してください。project save/reloadの実行確認は未実施です。`ArtifactLooksPresetBrowser` はplaceholder thumbnailsを生成し、LUT適用との接続は確認できていません。このラボは同ブラウザーを模擬せず、現在のLUT画像応答を評価します。

ブラウザーの24 preset ID と9 builtin LUT 名をsource hash付きで一覧化した [`REGISTRY_AUDIT.json`](REGISTRY_AUDIT.json)もあります。初期監査後、2026-11-17の製品フォローアップで`teal_orange`を既存Tritone `TealAndOrange`へ暫定接続し、Inspector Add Effect pickerから開けるようにしました。この1件は名前一致から作った製品仮説で、残り23 IDは未対応です。人手視覚判定・runtime検証・品質ゲートは未完了です。

## 実行

Python 3.10+ と `requirements.txt` の依存が必要です。Artifact の起動、ビルド、CMake、テストは行いません。

```powershell
python experiments/CinematicLooksLab/cinematic_looks_lab.py --rd rd0
python experiments/CinematicLooksLab/contract_probe.py
# A separate run can retain its own output and manifest identifier:
python experiments/CinematicLooksLab/contract_probe.py --output experiments/CinematicLooksLab/captures/rd3_lut_alpha_wrapper/float_contract_probe.json
python experiments/CinematicLooksLab/make_alpha_association_fixture.py
python experiments/CinematicLooksLab/cinematic_effects_extension.py
```

LUT 実験は `captures/<rd>/` に素材ごとの統計、chart diagnostics、通常／blind contact sheets、full-resolution ROI crops、manifest、未確定 gate を保存します。Color Balance / Tritone extension は同じ5素材に baseline、LUT Cinematic、Color Balance Cinematic 0.5/1、Tritone CPU Cinematic 0.5/1、TealAndOrange 1 の画像を出力します。別にTritone HLSL式を独立mirrorし、0/0.5/1 strengthのCPU比較、ROI統計、alpha/float probesを作成します。extensionの既定出力は`captures/rd2_effect_extension/`です。既存の出力先には停止し、明示的に`--overwrite`を指定した場合だけ置き換えます。新しい`--output`と`--rd`を指定すればrd0を保持して別runを作れます。RGB code差は記述指標で、知覚色差ではありません。式mirrorはC++実行や実アプリ出力ではありません。現行のsource candidate findingsと開いたgateは`captures/rd0/findings.json` / `gates.json`に残し、人手レビューとruntime capture前に品質判定・勝者を決めません。

## 出力上の制限

Python harness は `ArtifactCore/src/Color/ColorLUT.cpp` の 17³ `BuiltinLUTs` 生成式（各 `setValue` 格子点の clamp を含む）と `ColorLUT` の trilinear sampler を mirror し、manifest に harness 自身・C++ source・pinned [`SOURCE_CONTRACT.json`](SOURCE_CONTRACT.json) の SHA-256 を記録します。ブラインド素材順は case ごとの導出 seed と shuffle 方法も記録します。pin と一致しない C++ source に対しては古い mirror が誤って実行されないよう停止します。source を更新する場合は変更内容を調査し、mirror と証跡を更新してから pin を変更してください。これは現行の CPU 式を独立計算した証拠であり、C++ 関数を直接実行した数値一致テストではありません。Composition の適用、GPU parity、色管理経路も証明しません。

夜景素材の provenance は [`../GlowLab/assets/SOURCES.md`](../GlowLab/assets/SOURCES.md)、人物・森林素材は [`assets/SOURCES.md`](assets/SOURCES.md) にあります。LUT と effect extension の対応表はそれぞれの blind directory に分離されています。8-bit sRGB 画像には alpha を含めず、HDRは式probeのみで実画像・runtime品質は未評価です。LUT / ColorBalance CPU wrapperとTritone CPU/GPU wrappersはpremultiplied descriptor時にunpremultiply/process/repremultiplyします。LUTの修正前mirrorではfixture alpha sweepの最大straight RGB差が約1.321、更新後source mirrorでは全9 LUTの最大差が2.3e-16未満でした。Tritoneはrd0で式差がありましたが、rd1/rd2 mirrorで5素材の量子化差0を記録しました。これは実HLSL/C++一致、実経路、backend runtime差を証明しません。性能はPython reference operationで製品性能ではありません。

Alpha association の実経路確認用に、複数 RGB 色と固定 alpha patch / 水平 alpha ramp を持つ synthetic straight-alpha PNG [`assets/alpha-association-chart.png`](assets/alpha-association-chart.png) を追加しました。条件と SHA-256 は隣接 JSON にあります。runtime への適用・capture はまだ行っていません。
