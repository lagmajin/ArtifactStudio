# Buffer Precision / Alpha Conversion Chain R&D — 画像評価計画

**最終更新:** 2026-11-17
**ステータス:** In Progress
**識別子:** M-FX-PRECISION-RD

## 1. 目的と対象

- **改善したい見た目:** 半透明エッジの黒縁／色にじみ、8bit 入口での HDR クランプ、スムーズグラデーションの banding、透明画素に隠れた RGB の漏れ。
- **対象:** レイヤー入口の sRGB 8bit 量子化と、premultiplied alpha × sRGB の変換順序。具体的には `IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md` の弱点 1・2・3・4 を画像条件として再現し、順序の違いが合成結果に与える影響を測る。
- **対象レンダー経路:** オフラインの数式モデル（Python / NumPy）と合成 fixture。GPU dispatch、スワップチェーン変更、実アプリの readback／upload は後続段階。
- **対象外:** 実装変更、Diligent パイプライン、HDR モニタ出力、OCIO 設定の変更。
- **現状:** 監査では「正しくは unpremultiply → sRGB decode → linear premultiply の順。現状この順序が守られているか未確認（要検証）」とされている。本 R&D はその順序違反の画像影響を先に確定させる。

この文書は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う評価計画であり、実装や製品統合を承認するものではない。

## 2. 比較対象

| 種別 | 内容 | 正解として扱える根拠 |
|---|---|---|
| Reference | linear scene-referred の straight RGBA をそのまま linear 合成した結果 | 定義上の基準。色空間変換と alpha 合成の順序を正しく適用した値 |
| Baseline（現状仮説） | sRGB 8bit 量子化後に premultiplied encoded 値をそのまま linear デコードする連鎖 | 監査弱点 2 の「順序違反」をモデル化したもの。現行実装の断定ではない |
| 候補 | 8bit 量子化込みの正しい順序、float の正しい順序、HDR クランプのみ | 差分の原因を分離するための比較条件 |
| 外部参照 | なし | 実写や他製品画像は無条件に正解としない |

## 3. 評価素材

`experiments/PrecisionLab/build_precision_fixtures.py` が固定 seed で以下を生成する。

| Fixture | 目的 | 失敗しやすい点 |
|---|---|---|
| `alpha_fringe` | 多段 alpha（0 / 0.01 / 0.1 / 0.5 / 0.9 / 1）の非対称色エッジ | 黒縁、色にじみ、alpha 境界の段差 |
| `chromatic_edge` | 高コントラストかつ非対称 RGB の半透明境界 | 順序違反による色相ズレ |
| `transparent_hidden_rgb` | alpha=0 領域に非ゼロ／負値／HDR の RGB | 不可視 RGB の合成後リーク |
| `hdr_ramp` | linear 0〜4 の ramp | 0–1 クランプ、ハイライトの平坦化 |
| `banding_ramp` | linear 0.2〜0.3 の緩やかな ramp | 8bit 量子化による段差 |

各 fixture は `source.npy`（float32 linear straight RGBA）、`source.png`（表示用）、manifest（SHA-256 と条件）を出力する。

## 4. 画像条件

| 項目 | 固定／記録する内容 |
|---|---|
| 入力形式 | float32 RGBA、linear scene-referred、straight alpha（NPY） |
| 解像度 | 256×256 |
| sRGB | OETF / EOTF は標準定義を明示的に使用。8bit は round-to-nearest |
| alpha 契約 | straight を入力規約とする。premultiplied は層バッファ表現としてモデル内で生成する |
| 背景 | 黒 / 18% 灰 / 白 の3種類で合成結果を比較 |
| 露出・表示変換 | 表示用 PNG は sRGB OETF と 0–1 クランプのみ。Artifact の active view transform は未使用 |
| 乱数 seed | 20261117 |
| 出力形式 | 測定用 NPY、表示用 PNG、条件と hash を記録した manifest JSON |

## 5. 必須評価項目の適用判断

| 項目 | 判断 | 根拠／内容 |
|---|---|---|
| 効果の正しさ | 適用 | 変換順序ごとの合成結果が reference に近いこと。float の正しい順序は reference と一致するはず |
| 画質と細部 | 適用 | エッジの黒縁／色にじみ、ramp の banding を等倍と拡大 crop で確認 |
| 色と alpha | 適用（主軸） | straight / premultiplied、透明画素 RGB、背景合成後の縁、alpha 境界の誤差 |
| 時間安定性 | 対象外 | 静的変換のみで時間軸を持たない |
| 経路と解像度 | 一部適用／未検証 | 8bit vs float、複数背景は比較可。GPU backend、preview／最終出力は未検証 |
| 性能 | 未検証 | オフラインモデルでは測定しない |

## 6. 画像統計による評価

領域は全体に加え、**alpha 境界帯**（0 < alpha < 1）、**透明領域**（alpha = 0）、**高輝度領域**（linear > 1）で分けて測定する。

| 測定 | 用途 |
|---|---|
| 画素値分布 | RGB／alpha の最小・最大・分位点、非有限値、クリップ率 |
| 参照との差分 | MAE／RMSE／最大値／p99／閾値超過率、差分画像 |
| 色／alpha | 背景合成後の縁の色差、透明画素 RGB のリーク量 |
| 構造 | ramp の離散レベル数と最大隣接段差、エッジ幅 |
| 効果固有 | 順序違反による黒縁の強度（背景ごとの差分） |

数値だけでは合格としない。全体画像と crop を同じ表示条件で並べ、目視レビューを併記する。

## 7. 評価と合否

- 各 gate の閾値は R1 の baseline 取得後に、実測分布と目視レビューから設定し、候補の最終比較より前に freeze する。
- 想定する gate（未確定）: 透明領域のリーク、alpha 境界帯の MAE、HDR クランプ率、banding の離散レベル数、非有限値 0。
- 変更時は理由・変更前後・承認者・影響する結果を記録して比較をやり直す。
- 重大欠陥（例: 透明画素 RGB の可視リーク、非有限値、著しい黒縁）は合格扱いにしない。

## 8. 実行条件

| 項目 | 内容 |
|---|---|
| 実行環境 | Python 3 / NumPy / OpenCV（バージョンを manifest に記録） |
| GPU・backend | 本段階では不使用。D3D12／Vulkan、実アプリ capture は未実施として記録 |
| 時間・メモリ予算 | 未定義。実装段階で設定 |
| ビルド・テスト | この R&D のオフラインスクリプトはビルドを伴わない。実アプリのビルドは明示指示まで行わない |

## 9. 進行段階と成果物

### R0 — 評価計画と fixture（本段階）
- 本計画書、README、fixture 生成スクリプト。**Exit:** 第三者が再現できること。

### R1 — Baseline 取得と gate 確定
- 各変換連鎖の統計と contact sheet を取得し、gate を freeze する。**Exit:** 条件と閾値が記録される。

### R2 — 原因分離と候補比較
- 8bit 量子化、変換順序、HDR クランプの寄与を分离して比較する。**Exit:** 採用／不採用の根拠がレビュー可能。

### R3 — 実装計画と受入契約
- 実アプリの経路（layer→layerFloat、readback、表示）へ割り当て、視覚・数値 gate を定義する。**Exit:** 実装 milestone を切れる状態。

## 10. 証跡と完了条件

入力・参照・出力への参照、取得手順、設定、環境、実行日、画像全体と crop、測定結果、判定者、既知の制限を保存する。未達や未検証はそのまま明示し、画質達成や製品統合完了と表現しない。R&D 完了は実装承認を意味しない。

## 11. 未確認・リスク

- 現行実装が実際にどの変換順序を使うかは未検証。baseline は監査に基づく仮説であり、断定ではない。
- 監査自体がソース静的確認に基づき、build／test／runtime 画像比較は未実施。
- premultiplied の encoded 表現、符号付き／HDR 値の扱いなど、契約上未確定の点が多い。

## 12. 次の作業

1. ✅ fixture 生成・解析・再現性確認を実施（§13）。
2. ✅ gate を freeze し、R2 の寄与分離を実施（§14）。
3. ✅ 実アプリ経路の静的追跡と capture adapter 設計を実施（§15）。実装・実行は未実施。

## 13. R0 / R1 実行結果（2026-11-17）

- 実行: `python experiments/PrecisionLab/build_precision_fixtures.py` と `python experiments/PrecisionLab/analyze_precision_chain.py`
- 出力: `experiments/PrecisionLab/fixtures/precision-rd1/`（5 fixture／10 ファイル）、`experiments/PrecisionLab/captures/chain-rd1/chain-metrics.json` と contact sheet
- 再現性: 別ディレクトリへ再実行し、fixture id とファイル名をキーにした SHA-256 集合が完全一致（差分 0）

18% 灰背景の full MAE（naive8 / correct8）:

| Fixture | naive8 | correct8 | 備考 |
|---|---|---|---|
| alpha_fringe | 0.04762 | 0.00280 | alpha 境界帯は 0.07186 / 0.00356 |
| chromatic_edge | 0.02621 | 0.00094 | alpha 境界帯は 0.06939 / 0.00131 |
| transparent_hidden_rgb | 0.00008 | 0.00008 | 順序差なし |
| hdr_ramp | 0.76495 | 0.76495 | クリップ率 0.6732。順序ではなくクランプが支配 |
| banding_ramp | 0.00100 | 0.00100 | 26 段階、最大隣接ステップ 1/255 |

- 目視: `alpha_fringe` と `chromatic_edge` で、naive8 は部分 alpha の帯が暗く／彩度が落ちており、reference と correct8 は一致。`hdr_ramp` は 3 パネルともクランプで同見え。
- 負の結果: alpha=0／0.01 下の隠れ RGB は、このモデルでは順序違反でも可視リークしなかった。部分 alpha の色ずれの主因は隠れ RGB ではなく変換順序である。
- 解釈: 8bit 量子化単独の誤差は 0.001 程度、順序違反は 0.02〜0.07 と大きい。HDR クランプはこれらと独立した支配的な誤差。

### 合否 gate（2026-11-17 freeze、承認者: ユーザーの「じゃあお願い」）

R2 の最終比較前に確定した。各 gate は「合格」「不合格」「未検証」を記録する。

| Gate | 条件 | 根拠 |
|---|---|---|
| alpha 境界 MAE | 正しい順序で ≤ 0.01 | 実測 0.00356 / 0.00131。naive8 は 0.07 前後で不合格 |
| 非有限値 | 0 | 全 fixture で 0 |
| 隠れ RGB リーク | 透明領域の差分を記録し、可視リークがないこと | 実測はほぼ 0。閾値は目視と合わせて設定 |
| HDR クランプ | クリップ率を記録し、順序ゲートとは分離して判定 | 順序を直しても改善しないため |
| banding | 離散レベル数と最大ステップを記録 | universal な合格閾値は設定しない |
| 目視 | 部分 alpha に黒縁・色ずれがないこと | contact sheet で確認 |

gate は R2 比較前に freeze 済み。変更する場合は理由・変更前後・承認者・影響する結果を記録して比較をやり直す。判定者はユーザー。

## 14. R2 寄与分離（2026-11-17）

- 実行: `python experiments/PrecisionLab/separate_contributions.py`
- 出力: `experiments/PrecisionLab/captures/contributions-rd2/contributions.json`
- 手法: `reference → clamped`（HDR クランプ）`→ naive_float`（変換順序）`→ naive8`（8bit 量子化）の段階差を計算。MAE は厳密に加算的ではないため、残差は誤差予算ではなく診断値として扱う。

18% 灰背景の段階別 MAE:

| Fixture | 領域 | clamp | order | quantization |
|---|---|---|---|---|
| alpha_fringe | full | 0.00148 | 0.04549 | 0.00080 |
| alpha_fringe | alpha_edge | 0.00225 | 0.06932 | 0.00051 |
| chromatic_edge | alpha_edge | 0.00000 | 0.06943 | 0.00061 |
| transparent_hidden_rgb | full | 0.00000 | 0.00000 | 0.00008 |
| hdr_ramp | full | 0.76451 | 0.00000 | 0.00043 |
| hdr_ramp | hdr | 1.01935 | 0.00000 | 0.00021 |
| banding_ramp | full | 0.00000 | 0.00000 | 0.00100 |

- 判定: G1（alpha 境界 MAE ≤ 0.01）pass、G2（非有限値 0）pass、G3（透明領域リーク）pass。G4／G5 は記録のみ、G6 は R1 目視済みで最終判定はユーザー。
- 結論: 部分 alpha の色ずれは変換順序が支配（約 0.069）、HDR ramp はクランプが支配（約 0.765）、banding は 8bit 量子化が支配（0.001）。3 つの誤差は独立に扱うべきで、1 つの閾値で合格判定してはならない。
- 未達: 実アプリの layer→layerFloat 経路 capture、CPU/GPU parity、性能、知覚上の許容差は未検証。R&D 完了や実装承認を意味しない。

## 15. R3 静的追跡と capture adapter 設計（2026-11-17）

### 静的追跡結果

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp:13389-13398` が `layerSRV` → `layerFloatUAV` へ `convertLayerToFloat` を呼ぶ。背景 seed 経路も `:12374-12387` で同じ。
- `Artifact/src/Render/ArtifactIRenderer.cpp:8273-8302` は `LayerBlendPipeline::convertLayerToFloat` へ委譲。
- 実体の HLSL は `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:106-137`。
  - `alpha = saturate(src.a)`
  - `straight = alpha > 1e-6 ? src.rgb / alpha : 0`
  - `OutTex = float4(saturate(straight), alpha).bgra`
  - **明示的な sRGB decode なし、linear での再 premultiply なし、straight で保存**
- sRGB decode は `layer` の sRGB SRV（`Artifact/src/Render/ArtifactRenderLayerPipeline.cpp:3611, 1249-1251`）に暗黙依存。`Texture2D::operator[]` / `Load` がバックエンドで decode するかはリポジトリ内に証拠がなく**未確定**。
- CPU 側 `ArtifactCore/include/Image/SurfacePixelConversion.ixx:240-268` は unpremultiply → sRGB decode → premultiply の正しい順序を実行している。
- 未解決の前提: `layer` の premultiply が encoded 空間か linear 空間か。encoded なら監査の順序が正しく、linear なら decode → unpremultiply が正しい。ソースから確定しない。

### 判定

- 明示的な数式は監査の正しい順序と一致しない（decode と再 premultiply が欠落し、straight で保存）。
- 実効値は sRGB SRV の Load セマンティクスと premultiply 領域に依存し、静的ソースからは断定できない。
- GPU と CPU の契約が異なるため、premultiplied sRGB エッジの parity はソースだけでは証明されない。

### capture adapter 設計

- 最小の継ぎ目は既存テスト `tests/Artifact/ClonerGpuRenderContractTest.cpp` へ `TEST` を 1 件追加すること。production code 変更なし、新規 CMake target なし。headless renderer と float readback の既存パターンを再利用する。
- 構成: 4×1 の `RGBA8_UNORM_SRGB` SRV（非対称 premultiplied 値）→ `createLayerBlendPipeline()` → `convertLayerToFloat()` → `RGBA32_FLOAT` UAV → `readbackTextureViewToBufferAsync()` で生 float を取得 → CPU 参照（unpremultiply → decode → premultiply）と比較。
- ドラフト: `experiments/PrecisionLab/capture_adapter_draft.cpp`（未コンパイル、CMake 未登録）。`ImageUploadBuffer` のアクセス API と Core モジュールの import 名は実装時に確認が必要。
- capture テストは `tests/Artifact/ClonerGpuRenderContractTest.cpp` に `LayerToFloatConversionCapture` として追加済み（CMake 変更なし）。

### ビルド検証状況（2026-11-17）

- 既存ビルドツリー `out/build/x64-Debug-Agent` で検証を試みた。
- 解消した環境問題: `ScopeComputer.ixx` の古い BMI（再生成で C2511 解消、ソース内容は変更せず）、`rc.exe` の PATH 不足（Windows SDK bin を追加）。
- 未解消の既存問題: `Animation.Transform3D` の import で `stop_token` の fatal C1116、および `Graphics.Shader.Set` の BMI 不足（C2230/C1199）。いずれも今回の変更とは無関係。
- したがって capture テストは**コンパイル・実行ともに未検証**。変換順序の実測値はまだ得られていない。

### 未達

- 実 GPU における sRGB SRV Load の挙動、D3D12／Vulkan parity、実アプリのレイヤー合成結果、性能、製品判断は未検証。R&D 完了や実装承認を意味しない。

## 関連文書

- [画像エフェクト R&D 画像評価ルール](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)
- [画像バッファ精度 監査メモ](../analysis/IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md)
- [Color Pipeline Parity Hardening](MILESTONE_COLOR_PIPELINE_PARITY_HARDENING_2026-08-21.md)
- [画像チャンネル順の一貫性監査](../analysis/COLOR_CHANNEL_ORDER_CONSISTENCY_AUDIT_2026-10-16.md)
