# FSR1 空間アップスケール R&D — 画像評価計画

**最終更新:** 2026-11-17
**ステータス:** R&D 計画。製品 GPU render pass 接続・UI・既定値変更・書き出し自動適用は**承認しない**。既存のフレーム描画経路は変更しない。

## 0. なぜ計画から始めるか

FSR1 接続は出力画像の品質を目的とする変更であり、
[`IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md) の適用対象。
同ルールは「新規プロジェクトは開始前に評価計画を作る」「調査開始のための準備や baseline 取得は可能だが、
評価計画の不足を品質達成の根拠にしない」と定める。本書がその計画に当たる。

### 予備調査で判明した事実（2026-11-17、静的監査）

| 項目 | 事実 | 根拠 |
|---|---|---|
| シェーダ資産 | `Artifact/shaders/fsr_upscalingCS.hlsl`（EASU）/ `fsr_sharpenCS.hlsl`（RCAS）が存在し、`Artifact/App/shaders/` とバイト同一 | ファイル比較 |
| ホスト配線 | 両シェーダの compile / PSO / dispatch 参照は C++ ソースにゼロ | `tools/render_pass_audit.py --check` |
| シェーダ形式 | Wicked Engine フレームワーク前提（`CONSTANTBUFFER` マクロ、`sampler_linear_clamp : s100`、`globals.hlsli` / `ffx-*.h` の include、`register(spaceN)`）。`ComputeExecutor::build()` は `#include` ハンドラを設定せず生文字列を `CreateShader` へ渡すため、**そのまま埋め込めない** | `ArtifactCore/src/Graphics/Compute.cpp:63-73` |
| cbuffer | `struct FSR { uint4 Const0..Const3; }` = 64 バイト、EASU は 4 つ全部、RCAS は `Const0` のみ使用。`CBSLOT_FSR = 4`（元の register は b4） | `Artifact/shaders/ShaderInterop_Postprocess.h:107-114` |
| 定数生成 | `ArtifactCore/include/FSR/ffx_fsr1.h` に `FsrEasuCon` / `FsrRcasCon`（`A_CPU` 定義で使用可）が既存 | 同ヘッダ `:156, :662` |
| dispatch | `[numthreads(64,1,1)]`、1 スレッドが 4 フィルタ、1 グループで 16×16 px。`ComputeExecutor::makeDispatchAttribs(w,h,1,8,8,1)` は**誤り**で `ceil(w/16) × ceil(h/16)` が必要 | 両 HLSL `:30/:28` |
| 既存「動的解像度」 | `ArtifactIRenderer::setUpscaleConfig()` は源代码に**呼び出し元ゼロ**。実プレビューの低解像度化は `previewDownsample_` が駆動（`ArtifactCompositionRenderController.cpp:42640-42667`） | 静的 grep |
| 現状の拡大 | `finalizeGpuRenderToViewport()` の全画面 `drawSprite` による単純ストレッチ。品質パスなし | 同 `:14305-14320` |
| 解像度の壁 | Resolve 後の `accumSRV` は `passResolution`（= preview 低解像度）サイズ。EASU 出力は全ビューポート解像度で、**既存 RT に受け皿がない**。さらに `accumSRV` は BGRA で、RCAS は RGBA を期待する | 同 `:43779-43805`、FXAA の `.bgra` スワizzle |

**判定:** 製品配線には (a) 自己完結 HLSL への移植、(b) 全解像度 RT の確保とライフタイム設計、
(c) BGRA→RGBA 契約の整理、(d) 実 GPU でのシェーダコンパイル確認 が必要。
これらはビルド／GPU capture を伴い、現状の許可範囲（ビルド禁止）では完了できない。
よって本 R&D の第一段は**静的に完結する準備と評価計画の確定**までとする。

## 1. 目的と対象

### 目的
プレビューの低解像度描画（`previewDownsample_`）を、単純ストレッチではなく FSR1 の
EASU（Edge Adaptive Spatial Upscaling）→ RCAS（Robust Contrast Adaptive Sharpening）で
全解像度へ復元し、エッジのジャギーと細線の消失を減らす。

### 対象
- レンダー経路: GPU / Diligent（DX12 および Vulkan）の Composition preview のみ
- 対象レイヤー: 静止画・連番画像・シェイプ・テキスト・2.5D（AGENTS.md の現行優先方針に合わせる）
- 操作: 既存のプレビュー解像度低下（downsample 1/1, 1/2, 1/4, 1/8 相当）からの復元
- 第一段の成果物: 自己完結 HLSL と定数生成の契約、baseline 取得手順、合否基準

### 対象外
- 時間軸アップスケーラ（FSR2+/DLSS/XeSS）。モーションベクタ・深度・ジッタ基盤が必要で、
  静止画/シェイプ中心方針と合わない（既存判断 `REPORT_UPSCALE_TECH_FEASIBILITY_2026-07-28.md` に同意）
- 書き出し時アップスケール、ML 超解像（ONNX/DML）、Anime4K 風経路
- ソフトレンダラのフォールバック（GPU 経路が使えない環境の最低限維持のみ）
- 3D レイヤー、レイトレ、仮想テクスチャ

## 2. 比較対象

| 種別 | 役割 | 正解として扱える根拠 |
|---|---|---|
| **Baseline A** | 現行の単純 `drawSprite` ストレッチ | これは「改善前」であり正解ではない。同一条件で毎回取得する |
| **Baseline B** | 1/1（downsample なし）で描いた全解像度フレーム | 当作成物の上限品質。復元結果がこれに近づくことを目指すが、完全一致を要求しない（情報量が異なるため） |
| **Ground truth** | 設定しない | 合成・実写画像を無条件に物理的正解としない（ルール§2 に準拠）。必要なら承認済み目標画像を個別に追加し、その根拠を記録する |
| 外部参照 | AMD FSR1 リファレンス実装（`libs/DiligentEngine/.../SuperResolution/FSRProvider.cpp`） | アルゴリズム契約の照合にのみ使用。画質の正解ではない |

Baseline A/B は、まず既存の `ArtifactUiTest` キャプチャ経路で取得する。
GPU capture が可能になった環境で、同じフレーム・同じViewport条件の PNG を SHA-256 付きで保存する。

## 3. 評価素材

Effect 固有の失敗しやすい箇所を選ぶ。各素材の出所・バージョン・ハッシュを記録する。

| ID | 素材 | 狙う欠陥 |
|---|---|---|
| F1 | 高コントラストの斜線グリッド（1px 細線含む） | ジャギー、細線の消失、方向判定ミス |
| F2 | 文字レイヤー（日本語 12px / 英語 9px、アンチエイリアス境界） | 文字のかすれ、ボールド化、ハロー |
| F3 | 急激な明度遷移のあるグラデーション + 半透明レイヤー重ね | banding、alpha 縁の色にじみ |
| F4 | 高輝度小ドット（HDR 相当の値を含む） | クリップ、RCAS によるリンギング |
| F5 | 連番画像の連続 8 フレーム（視差のある細線） | flicker / shimmer（空間フィルタなので時間変化は意図しない） |
| F6 | 1/2, 1/4, 1/8 の各 downsample 段 | 縮小率に対する応答の連続性 |

素材はリポジトリに置ける場合は `tests/` の fixture として参照し、置けない場合は入手先とハッシュを記録する。
F5 のみ連続フレームを使う。それ以外は単一フレームで条件を固定する。

## 4. 画像条件（固定／記録）

| 項目 | 条件 |
|---|---|
| 入力 | Composition preview の `accumSRV`（BGRA）。必要なら RGBA 変換後の SRV を別途定義し、契約を明記 |
| 設定 | downsample 段（F6）、RCAS sharpness（既存 `setUpscaleConfig` の sharpness 0..1 を流用する場合その値） |
| 解像度 | 入力 = preview 低解像度、出力 = viewport 物理解像度（`origViewW × origViewH`、DPI 考慮）。両方を記録 |
| 色空間 / 転送関数 | 現行 preview のまま（sRGB RT）。変換を追加する場合は変換式と適用順を記録 |
| 露出 / 表示変換 | `applyViewportExposure` の現行設定で固定 |
| alpha | straight / premultiplied の契約を実装前に明記。背景合成後の縁を必ず確認 |
| フレーム | F1〜F4, F6 は同一フレーム番号。F5 は連続 8 フレーム |
| 乱数 seed | 使用しない（空間フィルタ）。使用する場合は固定値を記録 |
| 出力形式 | 比較用 PNG（8bit）とは別に、測定用は情報を保持できる形式（EXR float）を残す |
| Backend | D3D12 / Vulkan の両方で同じ条件を揃える |

## 5. 評価と合否

### 必須評価項目（ルール§18 に準拠。対象外・未検証を明記）

| 項目 | 適用 | 評価領域 | 手順 | 合格条件 | 重大な欠陥 |
|---|---|---|---|---|---|
| 効果の正しさ | 適用 | 全体 + エッジ領域 | downsample 段を変えて応答を連続確認。sharpness=0 で中立に近づくか | 段階変更で急変しない。数值パラメータが単調に応答 | 画像が反転／破損、パラメータが無効 |
| 画質と細部 | 適用 | F1, F2 の等倍 + 4 倍拡大 crop | 細線保持、halo、banding を目視 | Baseline A より細線が明瞭で、新たな halo が许容範囲内 | 文字が読めない、過度な halo |
| 色と alpha | 適用 | F3, F4 + 複数背景への合成 | straight/premultiplied 双方と背景色 3 種で比較 | 縁に色にじみが出ない。HDR 値が不当にクリップされない | alpha が 0/1 に丸められる、色反転 |
| 時間安定性 | 適用（F5 のみ） | F5 の同一座標 | 固定条件で 8 フレームの差分を測定 | 空間フィルタ起因の flicker が現れない | フレーム間のちらつき |
| 経路と解像度 | 適用 | 全素材 × D3D12/Vulkan | 両 backend の出力を比較 | backend 間差が決めた許容差内 | 片方が未描画／真っ黒 |
| 性能 | 適用 | 1920×1080 preview | 処理時間と GPU メモリを Baseline A と比較 | 予算内（数値は baseline 取得後に確定） | フレーム落ち、VRAM 増大が予算超過 |

**未検証として記録する項目:** 現時点では GPU 実行が不可能なため、全項目が「未検証」。
計画ではあるが、実行結果をもって初めて合否を判定する。

### 画像統計（ルール§31 に準拠）

- **画素値分布**: RGB / 輝度 / alpha の min/max、平均、中央値、分位点、クリップ率、非有限値数。HDR は 0..1 に丸めない
- **参照との差分**: Baseline B（1/1 全解像度）に対する MAE / RMSE / 高分位点。同じ座標・色条件でのみ比較する
- **構造と細部**: F1 のエッジ幅・強度、細線の保持率。PSNR/SSIM は補助指標とし単独で合格としない
- **色**: 現行 sRGB 表示空間での輝度・色差分布。HDR へ無条件拡張しない
- **透明度・境界**: alpha 分布、前景/背景漏れ、複数背景合成後の差
- **ノイズ・時間**: F5 の平坦領域分散とフレーム間差
- **効果固有**: EASU のエッジ方向判定の正しさ、RCAS のピーク付近のリンギング量

領域は全体に加え、前景・背景・境界・平坦部・高輝度部を分けて測定する。
平均と併せて分位点・最大値・閾値超過率を確認し、少数の重大異常が埋もれないようにする。
測定結果には入力/参照 ID、領域、測定空間、alpha 扱い、指標定義、ツール版数、許容値を残す。

### 閾値の確定方法

数値閾値は **Baseline A/B の取得後に、候補の最終比較より前に確定する**。
確定時は根拠・変更前後・承認者を記録する。現時点で具体的数値は定めない（ルール§15 に準拠）。

## 6. 実行条件

| 項目 | 条件 |
|---|---|
| GPU / backend | D3D12 と Vulkan の両方。RT 対応デバイスが無い環境は skip と記録 |
| プレビュー / 最終出力 | まず preview のみ。書き出しは対象外 |
| 処理時間 | 既存 `frameDebugSnapshot` / opt-in profile で取得。目標は preview 予算内（値は baseline 後に確定） |
| GPU メモリ | 全解像度 RT を 2 枚（EASU 出力 + RCAS 出力）追加する見込み。増分量を測定し記録 |
| 利用できない環境 | 現状はビルド／GPU capture が許可されていないため、実行条件を「未検証」とする |

## 7. 第一段の作業単位（静的に完結する範囲）

1. 自己完結 HLSL（ffx ヘッダを展開した EASU / RCAS）を R&D 用に用意する。本番シェーダ一覧には登録しない
2. 定数生成の C++ 側契約（`FSRAttribs` 互換 64 バイト + RCAS 用 16 バイト）を既存 `ffx_fsr1.h` から導出し、レイアウトを文書化する
3. 全解像度 RT の確保方針を決める（呼び出し側所有・事前確保、ホットパスでの再確保禁止。ルール `HOT_PATH_RULES.md` 準拠）
4. BGRA → RGBA 契約を整理し、変換を明示関数として境界に置く（暗黙変換禁止）
5. Baseline A/B の取得手順を `ArtifactUiTest` キャプチャで確立する

いずれも製品描画経路への接続を含まない。接続は、本計画の必須評価が揃い、
ビルド／GPU 検証の明示许可が得られた後の個別依頼で判断する。

## 8. 完了条件（R&D として）

- 必須評価 6 項目の結果、合否と根拠が揃っていること
- 未検証／未達がそのまま明示されていること
- 採用／不採用判断と後続実装の受入条件が残っていること
- 画質達成や製品統合完了と表現しないこと（ルール§63 に準拠）

## 関連

- `docs/analysis/REPORT_UPSCALE_TECH_FEASIBILITY_2026-07-28.md`
- `docs/analysis/ADVANCED_RENDERING_GAP_2026-08-13.md` §5
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/technical/HOT_PATH_RULES.md`
- `tools/render_pass_audit.py`
