**最終更新:** 2026-09-12

# 汎用GPU常駐 設計メモ（Halftoneパイロット）

## 1. 背景・現状（ソース確認済み）

GPU実行経路は3種類ある。

| 経路 | 実体 | 常駐 | パラメータ | 新規追加コスト |
|---|---|---|---|---|
| (a) 旧式単体GPU | 各effectの `*GPUImpl::applyGPU`（upload→compute→staging readback→`WaitForIdle()`） | ×（1effect毎に往復） | 各自のCB | 中（executor自前） |
| (b) creative compute cache | `runCreativeCompute(label, hlsl)` ＋ labelキーキャッシュ（`Artifact/src/Effect/ArtifactCreativeEffects.cppm:3725-3879`） | ×（(a)と同じ往復） | ×（input/outputのみ、entry固定`main`） | 小（HLSL文字列＋label） |
| (c) 常駐Kind | `GpuSpatialEffectKind` ＋ `RenderPipeline::applySpatialEffect` | ○ | float8＋mask | 大（Kind・Shader・Params・分岐の4箇所） |

(b)は既に「HLSLを渡すだけでGPU化できる汎用機構」だが、常駐でない・パラメータがない。
(c)は常駐だが、1effect追加に4箇所の編集が要る。

## 2. 提案：共有シェーダレジストリ＋汎用常駐分岐

(b)の汎用性を(c)の常駐に持ち込む。新設ではなく(b)の拡張方向。

- `Artifact.Effect.Abstract` に共有レジストリを置く。key（uint32＝effect ID文字列のFNV-1a）→ `{shaderBody, entryPoint, resource}`。
- 登録はeffectのctor（描画より必ず先に走る）。スレッド安全なfunction-local static map。
- `GpuSpatialEffectKind::Generic` を追加。ノードは `genericKey`＋既存 `parameters[8]` のみを運ぶ。rendererは具象effect型を見ない境界を維持。
- `RenderPipeline` は key→`{executor, paramsCB, source}` の汎用PSOキャッシュを持ち、固定契約でバインドする。
  - `g_InputTexture(t0)`＋`g_OutputTexture(u0)`（Filter）／`g_OutputTexture(u0)` のみ（Generator）。
  - `ResidentGenericParams(b0)`：`g_P0..g_P7`（effect）＋`g_Width/g_Height/g_Time/g_Frame`（標準）。48byte。
  - rendererは標準prelude＋bodyを結合してコンパイルする。
- 失敗（未登録・コンパイル失敗・バインド失敗）は `false`→層ごとCPUフォールバック（fail-closed、現行と同じ）。
- 既存9Kindは高速パスとして残す。`Kind::Halftone/RasterGlow` は専用ポート用の予約枠。

## 3. パイロット：登録済みHalftone

- 対象は登録済み `ArtifactHalftoneEffect`（Service `halftone`／`builtin.halftone`）。`Rasterizer::HalftoneEffect` はimportゼロ・Service登録なしのデッドコードのため対象外。
- bodyは `kHalftoneComputeHlsl`（`ArtifactCreativeEffects.cppm:3893`）をそのまま登録。CPU／creative経路は残置し、3経路の画素比較を可能にする。
- 登録済みHalftoneは無パラメータ（dotSize=8固定）のため、paramsはゼロ埋め。パラメータ露出は別案件。

## 4. 対象外（常駐化しないもの、2026-09-12判定）

- `ResidualGlow`：`cv::Mat history` のフレーム履歴を持つ状態持ちeffect。常駐チェーンは単一フレーム前提のため対象外。CPUのまま（`supportsGPU` 既定false）。
- 時間依存・状態持ち一般（Echo/Feedback/FrameAccumulation等）：同理由で対象外。履歴が必要ならパイプライン側の履歴機能として別途設計する。
- コスト超過パラメータ：`appendGpuSpatialNodes` で `false` を返してCPUに残す（LiquidGlow radius>8 が先例）。

## 5. 将来（Phase 2以降、本メモの範囲外）
- `runCreativeCompute` のキャッシュを共有レジストリへ移行し、(b)と汎用常駐を一本化。
- `g_Time/g_Frame` の配線（現行は0埋め）。進行性effectの常駐対象化はここが前提。
- mask/mixの汎用エピローグ。入るとmask付きも常駐に残せる。
- PSOウォームアップ（初回コンパイルの吸収）。

## 5. リスク・制約

- HLSL可搬性：DiligentがVulkanへ変換する。SM6専用・wave命令は禁止の規律が必要。
- エラー帰属：コンパイル失敗時はeffect key＋コンパイラ出力をログに出す。
- 定数バッファ固定長：8float＋標準に収まらないeffectは分割か対象外。
- **body側の約束（2026-09-12 Glow実証で確定）：旧式のような独自 `cbuffer X : register(b0)` をbodyに書いてはならない。preludeの `ResidentGenericParams(b0)` とregister衝突する。パラメータは `g_P0..g_P7`、標準値は `g_Width/g_Height/g_Time/g_Frame` を使う。入出力viewはbody側で宣言する（Filter：`g_InputTexture(t0)`＋`g_OutputTexture(u0)`、Generator：`g_OutputTexture(u0)` のみ）。**
- D3D12/Diligent低レベルには触れない。変更はeffect契約・RenderPipelineのcompute分岐・effectヘッダに限定。

## 6. 受入基準（ビルド許可後）

1. Halftone単体層が常駐パスを選択すること（`buildGpuRasterEffectPlan` 成功）。
2. CPU／creative GPU／常駐GPUの3者画素差分がtolerance内（境界1px級の可能性に留意）。
3. 未登録key・コンパイル失敗時にCPUフォールバックすること。
4. 他層・他effectの既存経路に変化なし。
