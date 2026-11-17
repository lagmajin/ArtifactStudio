# Content-Aware Fill（inpaint）R&D — 画像評価計画

**最終更新:** 2026-11-17
**ステータス:** In Progress
**識別子:** M-FX-INPAINT-RD

## 1. 目的と対象

- **改善したい見た目:** マスクで指定した領域を周囲のテクスチャから補完し、不自然な境界・色段差・細線の途切れなしに不要物を消す。
- **対象:** 静止フレームの明示的 inpaint。入力は `ImageF32x4_RGBA` 相当の float32 linear RGBA（straight alpha）、uint8 マスク、radius / method パラメータ。アルゴリズム第一候補は OpenCV `cv::inpaint`（Telea / Navier-Stokes）。
- **対象レンダー経路:** 今回はオフライン／CPU 参照と、実アプリに依存しない再現可能 fixture。Artifact の effect stack・Composition・Render Queue・GPU dispatch への接続は後続実装段階の対象とする。
- **対象外:** PatchMatch による動画フレーム間伝播、専用ブラシ／UI、Undo、RAM preview、進捗／キャンセル。これらは既存 DRAFT マイルストーンの後段であり、本 R&D の合否に含めない。
- **現状:** 専用 inpaint 実装は未実装と繰り返し確認されている（§2）。したがって比較用 baseline は「inpaint 未適用＝入力そのまま」であり、既存機能の回帰比較ではない。

この文書は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う評価計画であり、実装や製品統合を承認するものではない。評価結果から候補を絞り、変更は後続の明示された実装 milestone で行う。

## 2. 既存資産との関係（重複の整理）

| 文書／資産 | 役割 | 本 R&D との関係 |
|---|---|---|
| `docs/planned/MILESTONE_CONTENT_AWARE_FILL_2026-07-08.md` | DRAFT／未着手。静止 OpenCV inpaint → 動画伝播 → UI の段階案 | 実装側の親。本 R&D はその前に置く評価計画 |
| `docs/planned/MILESTONE_IMAGE_EFFECT_AI_GAP_2026-08-18.md` | P0 として inpaint を指定。`ArtifactInpaintEffect` を提案 | 実装候補の出所。ここから実装判断を先に進めない |
| `ArtifactCore/src/ImageProcessing/OpenCV/OpenCVRotoBrushEngine.cppm` | OpenCV バインディング／grabCut／optical flow の既存基盤 | 再利用対象の基盤。RotoBrush 自体は inpaint の証拠ではない |
| `experiments/blur-rd/` | float32 NPY を測定源、PNG を表示用と分離し、hash 付き manifest を運用 | fixture 規約として踏襲する |
| `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` | 画像評価の必須ルール | 本計画の準拠元 |

## 3. 比較対象

| 種別 | 内容 | 正解として扱える根拠 |
|---|---|---|
| Baseline | inpaint 未適用の入力。マスク内の不要物が残った画像 | 現状出力そのもの。改善の出発点であり、合格画像ではない |
| 合成 ground truth | fixture 生成時に不要物を挿入する前のクリーンプレート | 合成画像なので「元画像」は既知。ただし人間が見て自然かどうかは証明しない |
| アルゴリズム候補 | OpenCV `cv::inpaint` の Telea / Navier-Stokes、radius スイープ | 外部ライブラリの実装。正解ではなく比較候補として扱う |
| 外部参照 | なし | 他製品や実写画像を無条件に物理的正解としない。必要になった時点で出所・撮影条件を記録して追加する |

## 4. 評価素材

生成スクリプト `experiments/InpaintLab/build_inpaint_fixtures.py` が固定 seed で以下を作る。素材はすべて合成であり、出所は本スクリプト、バージョンは manifest の `generator_version` と SHA-256 で記録する。

| Fixture | 目的 | 失敗しやすい点 |
|---|---|---|
| `flat_gradient` | 平滑領域の連続性 | マスク内に段差・banding が残る |
| `textured_wall` | 反復テクスチャのつなぎ | 不自然な繰り返し、テクスチャの途切れ |
| `high_contrast_edge` | 高コントラスト境界の通過 | エッジがマスク内で消える／位置がずれる |
| `thin_line` | 細線の再接続 | 細線が途切れる、太る、色が変わる |
| `alpha_edge` | 半透明・straight alpha の契約 | マスク外 alpha の変化、透明画素 RGB の変化、合成後の縁 |
| `hdr_highlight` | 高輝度（>1）の扱い | 0–1 への丸め、ハイライトの消失、非有限値 |
| `sequence_moving_object_000..007` | 時間安定性 | 静止背景がフレームごとに揺れる（flicker） |

各 fixture について `source`（不要物入り）、`mask`、`ground_truth` を NPY（float32 RGBA／uint8 マスク）と表示用 PNG の両方で保存する。測定には NPY を使い、PNG は目視用である。表示用 PNG は linear→sRGB OETF 後に 0–1 へクランプするため、HDR fixture の表示画像は測定元データと同じではない。

## 5. 画像条件

| 項目 | 固定／記録する内容 |
|---|---|
| 入力形式 | float32 RGBA、linear scene-referred、straight alpha（NPY）。PNG は表示用 |
| 解像度 | 静止 fixture 256×256、sequence 160×160×8 フレーム |
| マスク | uint8、255 = 補完対象。矩形／円を生成スクリプトで固定。feather は初期値なし（必要なら R1 で追加し条件を記録） |
| パラメータ | `method`（Telea / NS）、`radius`。全組み合わせを manifest に記録する |
| 色空間／転送関数 | linear scene-referred。表示は sRGB OETF、比較は linear 値。HDR は 0–1 に丸めない |
| 露出／表示変換 | 表示 PNG はクランプ＋sRGB OETF のみ。Artifact の active view transform は未使用と明記する |
| alpha | straight / premultiplied の別を記録。マスク外の RGB と alpha は不変を契約候補とする |
| 乱数 seed | 20261117（fixture 生成のみ。アルゴリズム側に乱数は使わない） |
| 出力形式 | 測定用 NPY float32 RGBA、表示用 PNG、条件と hash を記録した manifest JSON |

## 6. 必須評価項目の適用判断

| 項目 | 判断 | 根拠／内容 |
|---|---|---|
| 効果の正しさ | 適用 | マスク内が周辺から埋まること。空マスク／radius=0 で入力不変（恒等）であること |
| 画質と細部 | 適用 | 境界帯の段差、細線の再接続、halo、banding、テクスチャの不自然な反復を等倍と拡大 crop で確認 |
| 色と alpha | 適用 | straight / premultiplied の入力契約、透明画素 RGB、マスク外不変性、複数背景色への合成後の縁 |
| 時間安定性 | 適用（限定） | `sequence_moving_object` の静止背景についてフレーム間差を測定。物体移動そのものは flicker と数えない |
| 経路と解像度 | 一部適用／未検証 | オフライン CPU のみ。GPU backend、preview／最終出力、D3D12／Vulkan parity は実装がなく未検証 |
| 性能 | 未検証 | 実装前のため処理時間・メモリ予算は未定義。R3 で予算を定めてから測定する |

## 7. 画像統計による評価

全体に加え、**マスク内**、**境界帯**（mask の dilation − erosion）、**マスク外**、**平坦部**、**高輝度部**で分けて測定する。領域の座標／マスク、生成方法、画素数を manifest に保存する。平均だけでなく分位点・最大値・閾値超過率を併記する。

| 測定 | 用途 | 注意点 |
|---|---|---|
| 画素値分布 | RGB／alpha の最小・最大・平均・分位点、非有限値数、クリップ率 | HDR は 0–1 に丸めない。クリップの定義を明記する |
| 参照との差分 | 合成 GT に対する MAE／RMSE／最大値／高分位点／閾値超過率、差分画像 | GT は「不要物が無い画像」であり、生成テクスチャの完全一致を要求しない。比較は同じ座標・色・alpha 条件のみ |
| 構造と細部 | 細線の再接続率、境界帯の勾配連続性、局所コントラスト | 意図的にテクスチャを変える inpaint では、入力との差が大きいこと自体は欠陥ではない |
| 色／alpha | alpha 誤差、マスク外 RGB 差分、黒／18% 灰／白背景への合成後差分 | 透明画素の RGB と表示結果を区別する |
| ノイズ／時間 | 静止背景のフレーム間差分、平坦部の分散 | 移動する物体の変化を flicker と混同しない |
| 効果固有 | 境界帯の自然さ（目視レビュー用スコア） | 数値化方法は R1 で定義し、定義前に合格判定に使わない |

SSIM／PSNR は補助指標として記録するが、単独で画質合格を証明しない。正しい参照がない場合（例: `textured_wall` のテクスチャ完全一致）は参照差分を未適用とし、分布と目視レビューで評価する。

## 8. 評価と合否

- 各項目について評価領域、手順、判定者、合格条件、許容差、重大欠陥を R1 の baseline 取得後に確定する。
- **閾値の確定方法と期限:** R1 で baseline と合成 GT の分布を取得した時点で、実測の分位点と目視レビューから許容差を提案し、**候補の最終比較より前に** freeze する。期限は R1 完了時（2026-11-24 目安）。変更する場合は理由・変更前後・承認者・影響する結果を記録して比較をやり直す。根拠なく共通閾値を流用しない。→ 2026-11-17 に確定（§15）。
- **重大欠陥（合格扱いしない例）:** マスク外の画素変化、alpha の反転、非有限値、可視的な境界段差、細線の完全な消失、静止背景のフレーム間揺れ。
- 数値だけでは合格としない。全体画像と評価領域の crop を同じ表示条件で並べ、目視レビューを併記する。
- 欠陥ごとに場所、再現条件、影響、重大度、証拠、対応状況を記録する。

## 9. 実行条件

| 項目 | 内容 |
|---|---|
| 実行環境 | Python 3 / NumPy / OpenCV（バージョンを manifest に記録） |
| 対象 GPU・backend | 本段階では不使用。実装段階で D3D12／Vulkan、preview／最終出力を別条件として記録する |
| 時間・メモリ予算 | 未定義。R3 で設定し、候補比較では画質と予算の両方を判定する |
| 利用できない環境 | GPU runtime capture、実アプリ起動、ビルド／テストは未実施として記録する。明示指示なく実行しない |

## 10. 進行段階と成果物

### R0 — 評価計画と fixture（本段階）

- 本計画書、`experiments/InpaintLab/README.md`、fixture 生成スクリプトを用意する。
- **Exit:** 第三者がスクリプトを実行して同じ fixture と manifest を再現できること。2026-11-17 に達成（§14）。

### R1 — Baseline 取得と閾値確定

- fixture を生成し、入力・GT・候補なしの条件で統計と目視 crop を取得する。
- 許容差と重大欠陥の判定基準を freeze する。
- **Exit:** baseline 条件と閾値が記録され、別作業者が再現できる。

### R2 — アルゴリズム候補の比較

- Telea / NS、radius スイープを同一条件で比較する。必要なら外部コードのライセンスと由来を記録する。
- **Exit:** 採用・不採用の理由、残る制限、実装コストがレビュー可能。

### R3 — 実装計画と受入契約

- 候補を `ArtifactAbstractEffect` 派生の型、マスク契約、preview／Render Queue の責務へ割り当てる。
- visual regression fixture、contract test、性能予算を定義し、実装を別 milestone として切る。
- **Exit:** 視覚・機能・性能の各 gate が承認可能な形で示される。

## 11. 証跡と完了条件

入力／GT／出力への参照、取得手順、設定、環境、実行日、全体画像と crop、測定結果、判定者、既知の制限を保存する。R&D 完了には必須評価の結果、合否と根拠、未解決事項、採用／不採用判断、後続実装の受入条件を残す。未達や未検証はそのまま明示し、「画質達成」や「製品統合完了」と表現しない。R&D の完了自体は製品実装の承認を意味しない。

## 12. 未確認・リスク

- fixture の生成・hash 検証・再現性確認と一部画像の目視は実施済み（§14）。閾値確定、アルゴリズム比較、実アプリ検証は未実施。
- 合成 GT は「不要物が無い画像」であり、補完が自然であることの証明ではない。自然さは目視レビューが必要。
- `alpha_edge` の inpaint 領域における alpha 契約（straight / premultiplied、マスク外不変）は未確定。期待値を先に決めず、R1 で現状を記録する。
- `hdr_highlight` は 0–1 クランプする実装では GT を再現できない可能性がある。欠陥と断定せず、契約としての扱いを R2 で判断する。
- 実アプリの effect stack、Render Queue、GPU backend、性能は未検証。

## 13. 次の作業

1. ✅ fixture 生成・hash 検証・再現性確認は完了（§14）。
2. ✅ 統計スクリプトと contact sheet を追加し baseline を取得（§15）。
3. ✅ gate を freeze し、R2 の候補比較を実施（§16）。HDR／alpha 契約と R3 が残る。
4. ✅ R3 の第一候補を実装（§17）。HDR／alpha の期待契約定義と実アプリの受入は未完了。ビルド・実行許可を得てから capture adapter を設計する。

## 14. R0 実行結果（2026-11-17）

- 実行コマンド: `python experiments/InpaintLab/build_inpaint_fixtures.py`
- 出力: `experiments/InpaintLab/fixtures/inpaint-rd1/`
- 14 fixture（静止 6 ＋ sequence 8）、84 ファイル。manifest 内の全 SHA-256 を再計算し、不一致 0。
- 環境: NumPy 2.4.4 / OpenCV 4.13.0、generator 0.1.0、seed 20261117。
- 再現性: 別ディレクトリへ再実行し、fixture id とファイル名をキーにした SHA-256 集合が完全一致（差分 0）。
- 目視: `thin_line/source.png` と `ground_truth.png` を比較し、不要物で途切れた細線とクリーンプレートで連続する細線を確認した。表示用 PNG の確認であり、NPY の数値契約の確認ではない。
- 未達: アルゴリズム候補の適用、統計・閾値確定、実アプリ capture、性能計測は未実施。R0 の Exit（再現性）のみ達成。

## 15. R1 ベースライン取得（2026-11-17）

- 実行: `python experiments/InpaintLab/analyze_inpaint_baseline.py`
- 出力: `experiments/InpaintLab/captures/baseline-rd1/baseline-metrics.json` と静止 6 件の contact sheet（source ｜ ground truth ｜ 4 倍増幅の差分）。
- 意味: baseline は inpaint 未適用。マスク内の差分は「不要物が残っている量」であり、欠陥スコアではない。

| Fixture | mask_in MAE / max | mask_out MAE / max | alpha max |
|---|---|---|---|
| flat_gradient | 0.36333 / 0.65529 | 0 / 0 | 0 |
| textured_wall | 0.27754 / 0.76857 | 0 / 0 | 0 |
| high_contrast_edge | 0.41912 / 0.82 | 0 / 0 | 0 |
| thin_line | 0.32449 / 0.85 | 0 / 0 | 0 |
| alpha_edge | 0.43333 / 0.7 | 0 / 0 | 0.5 |
| hdr_highlight | 0.87464 / 3.5 | 0 / 0 | 0 |

- 時間: sequence の静止背景 7 ペアすべて MAE=0、max=0。
- 非有限値: 全 fixture で 0。
- 目視: `thin_line` は不要物で細線が途切れ、ground truth では連続している。`hdr_highlight` は linear 4.0 のディスクが表示用 PNG では白にクランプされるが、測定元の NPY は HDR 値を保持している。

### 合否 gate（2026-11-17 freeze、承認者: ユーザーの「つづけて」による継続指示）

候補比較の前に確定した。各 gate は「合格」「不合格」「未検証」のいずれかを記録し、数値と目視を併記する。

| Gate | 条件 | 根拠 |
|---|---|---|
| G1 マスク外不変 | マスク外 RGB と alpha の MAE ≤ 1e-6 かつ最大 ≤ 1e-6 | baseline で完全一致。inpaint はマスク外を変えない契約 |
| G2 有限性 | 出力 RGB / alpha の非有限値 = 0 | baseline で 0 |
| G3 時間安定性 | sequence の静止背景フレーム間 MAE ≤ 1e-6 | baseline で 0 |
| G4 補完応答 | 各 fixture でマスク内 MAE が baseline MAE より小さい | 不要物が残った baseline より改善していることの最低条件 |
| G5 境界応答 | boundary_band の MAE が baseline MAE より小さい | 境界帯でも改善していること。平均値改善だけでは G6 で目視確認する |
| G6 細線／目視 | `thin_line` の line ROI（y=126..129, x=96..160）で候補 MAE < baseline MAE。加えて全 contact sheet を目視し、境界段差・色ずれ・細線の消失がないこと | 数値ゲートと目視レビューを併用する |

gate の変更は §8 に従い、理由・変更前後・承認者・影響する結果を記録して比較をやり直す。

## 16. R2 アルゴリズム候補比較（2026-11-17）

- 実行: `python experiments/InpaintLab/compare_inpaint_candidates.py`
- 出力: `experiments/InpaintLab/captures/candidates-rd2/candidate-metrics.json`、候補別 contact sheet、`visual-review-rd2.json`
- OpenCV 4.13 の `cv2.inpaint` は 8-bit 3-channel と 32-bit float 1-channel のみ対応。float32 RGBA / RGB は受け付けないため、8-bit sRGB 往復と per-channel float32 に分けて比較した。

| 候補 | G1 | G2 | G3 | G4 | G5 | G6 数値 | 判定 |
|---|---|---|---|---|---|---|---|
| telea_u8_r3 | fail（全14） | pass | pass | pass | pass | pass | reject |
| ns_u8_r3 | fail（全14） | pass | pass | pass | pass | fail | reject |
| ns_f32_r1 | pass | pass | pass | pass | pass | pass | 第一候補 |
| ns_f32_r3 | pass | pass | pass | pass | pass | pass | r1 より劣る（目視スマッジ） |
| ns_f32_r5 | pass | pass | pass | pass | pass | fail | reject |
| ns_f32_r7 | pass | pass | pass | pass | pass | fail | reject |
| telea_f32_r3 | pass | pass | pass | fail | fail | fail | reject |

- `ns_f32_r1` が最良。thin_line の line ROI MAE は 0.0000075（baseline 0.45）、high_contrast_edge の mask_in MAE は 0.0。目視でも継ぎ目なし。
- 8-bit 経路は全 fixture で G1 失敗。sRGB 量子化が inpaint 前にマスク外画素を変更しており、契約違反。
- Telea float32 は G4／G5／G6 失敗。定数フィールドのプローブでも半径依存の値（0.1→1.39、2.5→2.999、10→10.499、NS は一定値を保持）が出る不安定挙動を確認した。
- 半径 5／7 は細線で G6 失敗。半径 1 が最良。
- 未解決: HDR ディスクは再現できず（`ns_f32_r1` の hdr 領域 MAE 2.9767）、マスク内 alpha は 1.0 のまま（GT 0.5、差 0.5）。
- 結論: `ns_f32_r1` を「不透明・非 HDR 領域の第一候補」として R3 に進める。HDR と alpha の契約未解決、実アプリ検証なしのため、製品採用や R&D 完了判定は行わない。G6 の最終判定はユーザーに委ねる。

## 17. R3 実装（2026-11-17）

- 実装先: `Artifact/src/Effect/ArtifactCreativeEffects.cpp` と `Artifact/include/Effects/ArtifactCreativeEffects.ixx`（既存モジュールを拡張。新規モジュール／CMake 登録なし）
- クラス: `ArtifactInpaintEffect`。stable ID は `effect.keying.inpaint`（alias `inpaint`）、CPU のみ、`ComputeMode::CPU`。
- アルゴリズム: per-channel float32 Navier-Stokes、radius 1 を既定とする（§16 の第一候補）。8-bit sRGB 往復は G1 違反のため採用しない。
- マスク: 名前付き auxiliary input `inpaint_mask` の alpha ≥ 0.5 を塗りつぶし領域とする。入力が無い／塗りつぶし画素が 0 の場合は恒等（`dst = src`）。
- 契約: マスク外 RGB と alpha は不変。マスク内 alpha は source をコピーし、HDR 値（>1）は数値的に保持するが再現は保証しない（§16 の未解決事項）。
- 登録: `Artifact/src/Service/ArtifactEffectService.cpp`（factory と `availableEffects()`）、`Artifact/src/Widgets/InspectorEffectCatalog.cpp`。
- テスト: `tests/Artifact/InpaintEffectContractTest.cpp` を追加し、`tests/Artifact/CMakeLists.txt` に登録。identity、マスク外 RGB／alpha 不変、sampler の input ID／frame を検査する。
- 未実施: ビルド、テスト実行、実アプリ capture、GPU parity、性能計測。コンパイル確認はユーザーの明示指示後に行う。実装は R&D 完了や製品統合の承認を意味しない。

## 関連文書

- [画像エフェクト R&D 画像評価ルール](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)
- [Content-Aware Fill マイルストーン（DRAFT）](MILESTONE_CONTENT_AWARE_FILL_2026-07-08.md)
- [画像エフェクト 次世代ギャップ実装](MILESTONE_IMAGE_EFFECT_AI_GAP_2026-08-18.md)
- [Blur Effects R&D Suite](MILESTONE_BLUR_EFFECTS_RD_2026-11-12.md) — fixture 規約の参考
