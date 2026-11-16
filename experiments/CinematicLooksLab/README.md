# Cinematic Looks Lab

**最終更新:** 2026-11-16

静止画の cinematic look を、チャートとライセンス記録済み実写素材で比較するオフライン R&D ラボです。現在の画像測定は LUTEffect の9 builtin LUT lane のみです。ColorBalanceEffect と TritoneEffect にも適用可能な Cinematic preset が見つかりましたが、画像測定は未実施で評価計画を拡張中です。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) を参照してください。

## 現行コードとの関係

Artifact には `LUTEffect` と複数の `BuiltinLUTs` が既にあります。汎用 effect preset file API は LUT effect の preset index・LUT file path・intensity を含む effect state の保存／既存 effect object への復元を扱います。一方、静的監査では Composition-level effect の project reload が LUTEffect を concrete type として復元しない経路を確認しました。レイヤーの effect restore は別の factory-backed 経路です。詳細は [`captures/rd0/findings.json`](captures/rd0/findings.json) の `RD0-COMPOSITION-LUT-RESTORE-01` を参照してください。`ArtifactLooksPresetBrowser` は現状 placeholder thumbnails を生成し、LUT 適用との接続は確認できていません。このラボは同ブラウザーを模擬せず、現在の LUT 画像応答を評価します。

ブラウザーの24 preset ID と9 builtin LUT 名を source hash 付きで一覧化した [`REGISTRY_AUDIT.json`](REGISTRY_AUDIT.json) もあります。意味が似た名称から対応先を推測せず、明示 mapping がないものは未対応として記録しています。

## 実行

Python 3.10+ と `requirements.txt` の依存が必要です。Artifact の起動、ビルド、CMake、テストは行いません。

```powershell
python experiments/CinematicLooksLab/cinematic_looks_lab.py --rd rd0
python experiments/CinematicLooksLab/contract_probe.py
python experiments/CinematicLooksLab/make_alpha_association_fixture.py
```

出力は `captures/<rd>/` です。チャートと4枚の写真に各 Look を適用し、全体と各 source-ROI の統計、chart の灰色軸／グラデーション応答と8色パッチの RGB code delta・HSV hue / saturation 応答の補助診断、通常 comparison sheet、candidate-named full frames / ROI crops、blind full-frame / ROI contact sheet、blind ID 名の full-resolution ROI crops、manifest、未確定 gate を保存します。HSV は8-bit sRGB code 値由来の記述指標で、知覚色差ではありません。補助診断値は品質判定や gate ではありません。別の float 契約 probe は9 builtin LUTに対し、通常値・範囲外値・非有限値など6入力を強度0/0.5/1で評価します。さらに同一 RGB `(0.8,0.2,0.1)` を alpha 0/0.1/0.25/0.5/1 の straight/premultiplied 入力で比較し、fixture の8色 × 6固定 alpha patch でも全9 LUT・強度0/0.5/1を比較します。式 mirror 応答は `captures/rd0/float_contract_probe.json` に保存します。これは C++ 実行や実アプリ出力ではありません。ソースから見つかった alpha association の候補欠陥は `captures/rd0/findings.json` に根拠と未確認事項を添えて記録しています。初回 `rd0` は baseline と候補の出力・統計を記録しますが、品質 gate や勝者を決めません。人が画像を確認した後、評価計画に従って `gates.json` の閾値を根拠付きで固定してから `rd1` を実施します。

## 出力上の制限

Python harness は `ArtifactCore/src/Color/ColorLUT.cpp` の 17³ `BuiltinLUTs` 生成式（各 `setValue` 格子点の clamp を含む）と `ColorLUT` の trilinear sampler を mirror し、manifest に harness 自身・C++ source・pinned [`SOURCE_CONTRACT.json`](SOURCE_CONTRACT.json) の SHA-256 を記録します。ブラインド素材順は case ごとの導出 seed と shuffle 方法も記録します。pin と一致しない C++ source に対しては古い mirror が誤って実行されないよう停止します。source を更新する場合は変更内容を調査し、mirror と証跡を更新してから pin を変更してください。これは現行の CPU 式を独立計算した証拠であり、C++ 関数を直接実行した数値一致テストではありません。Composition の適用、GPU parity、色管理経路も証明しません。

夜景素材の provenance は [`../GlowLab/assets/SOURCES.md`](../GlowLab/assets/SOURCES.md)、人物・森林素材は [`assets/SOURCES.md`](assets/SOURCES.md) にあります。候補名を隠した接触シートと対応表 `blind_review_key.json` も出力します。8-bit sRGB 画像の見た目評価では alpha を含まず、HDR の画質も未評価です。float probe は式レベルの HDR 値と alpha association を扱いますが、C++ 実行一致、実アプリ経路、透明画像を複数背景へ合成した表示品質は未確認です。性能は NumPy/Python reference operation のみを測定し、製品性能ではありません。

Alpha association の実経路確認用に、複数 RGB 色と固定 alpha patch / 水平 alpha ramp を持つ synthetic straight-alpha PNG [`assets/alpha-association-chart.png`](assets/alpha-association-chart.png) を追加しました。条件と SHA-256 は隣接 JSON にあります。runtime への適用・capture はまだ行っていません。
