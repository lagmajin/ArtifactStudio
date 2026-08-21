# マイルストーン: Color Pipeline Parity Hardening

**最終更新:** 2026-08-21
**ステータス:** Not Started
**優先度:** High
**関連:** `docs/planned/MILESTONE_COLOR_BACKEND_HARDENING_2026-07-21.md`, `docs/planned/MILESTONE_COLOR_ALPHA_CONTRACT_UNIFICATION_2026-07-18.md`, `docs/analysis/COLOR_PIPELINE_AUDIT_2026-08-02.md`, `docs/analysis/IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md`, `docs/analysis/COMPOSITION_EFFECT_FORMAT_PATH_MEMO_2026-07-13.md`

## 目的

カラーマネジメント監査(2026-08-21)で判明した CPU/GPU パリティ破綻・生成色の空間不整合・書き出し色タグ欠落を解消し、「preview / GPU / CPU / export 間で同一ピクセル結果」を成立させる。数値コア(伝達関数・ガマット行列)は既に業界標準水準のため、本マイルストーンは経路統合が主題。

## 即時対応済み(2026-08-21)

- **Subtract ブレンドの誤マップ解消**: QPainter に Subtract は存在しないため、`SoftwareRender::qPainterCompositionMode()` を新設し Difference 代用を SourceOver フォールバックへ変更。CPU 書き出しパス(`ArtifactRenderQueueService.cppm`)とソフトプレビュー(`ArtifactSoftwareRenderInspectors.cppm`)の2箇所の重複実装を共有ヘルパーに統合。float/CV 経路(`blendChannel`)は正確な減算を維持。
- **LiftGammaGain のチャンネル順修正**: CPU コアを `colorDescriptor().channelOrder` 参照に変更(BGRA/RGBA 両対応)。GPU はアップロード時に `makeGpuImageUploadBuffer()` で BGRA→RGBA 正規化し、HLSL を `px.rgb` 直接参照に変更、readback descriptor を RGBA 固定に。
- **ColorWheels のチャンネル順修正**: CPU コアは RGBA 前提の `ColorWheelsProcessor::process` を利用するため、BGRA バッファでは行単位でスワップ→グレーディング→復元する `applyColorWheelsCore()` を新設。GPU は LiftGammaGain と同じ正規化アップロード+RGBA readback。
- **Curves の GPU 経路修正**: アップロード無正規化(BGRA を RGBA テクスチャとして誤アップロード)と readback 後の source descriptor 再設定(`setColorDescriptor`)を修正。これにより CPU(整数インデックス)/GPU(線形補間)の LUT 評価差以外の R↔B 反転が解消。

## 残存課題(優先度順)

### P1 — 生成色の working space 変換(最重要の構造問題)

| # | 項目 | 現状 |
|---|---|---|
| 1 | text/shape/solid/particle の生成色が QColor 生値(sRGB エンコード)のままレンダラへ流れる | 画像レイヤーのみ `applyInputInterpretation()` で working space 変換済み。working space = ACEScg の場合、生成グラフィックスだけ sRGB 数値が混入 |

方針: レイヤー描画境界で生成色を「sRGB エンコード → リニア → working primaries」変換してから GPU へ渡す。逆方向(表示・保存時)も明示関数で行う。AGENTS.md の「QImage 化・GPU アップロードは必ず明示関数を通す」方針と整合。

### P2 — 内蔵 OCIO プリセットの実体化

| # | 項目 | 現状 |
|---|---|---|
| 2 | `createACESConfig()` 等がメタデータ列挙のみで変換定義を持たない | 既定状態では全変換が静的行列+ガンマフォールバック。「デフォルト ACES」はトーンマップなし近似 |
| 3 | `ArtifactColorScienceManager::convertColor()` が完全ハードコード | OCIO を経由しない並行経路 |

方針: 公式 ACES OCIO config を同梱して `CreateFromFile` するか、内蔵プリセットに Programmatic transform を定義する。`convertColor()` は OCIO 経路へ統合または廃止。

### P3 — 書き出しの色タグ

| # | 項目 | 現状 |
|---|---|---|
| 4 | ICC プロファイル指定がサイレント無視 | `applyICCProfile()` が attribute 設定せず return true |
| 5 | FFmpeg 出力に `-color_primaries/-color_trc/-colorspace` なし | HEVC HDR 用タグは実装済み、SDR/汎用パイプは無タグ |
| 6 | float EXR 出力が 8bit QImage 経由の二重量子化 | `frameBuffer.toQImage()` → `/255.0f` で float 化 |

方針: P1 の float パイプライン整備後に EXR 直書き(float buffer → OIIO)へ接続。ICC/NCLX は OIIO attribute 設定を実装。

### P4 — 計算空間の統一

| # | 項目 | 現状 |
|---|---|---|
| 7 | エフェクトが sRGB エンコード値のまま計算 | canonical 契約(Linear/Premultiplied)は定義済みだが未適用。GPU ブレンドはリニア、ソフトウェアブレンドはエンコード値 |
| 8 | Curves の LUT 評価差(CPU 整数/GPU 補間) | CPU `processPixel` を線形補間に揃えるのが最小修正 |
| 9 | 8bit サンドイッチ(GPU RTV RGBA8_UNORM_SRGB、QImage 境界) | 中間 float 化の段階的拡大は IMAGE_BUFFER_PRECISION_AUDIT の分析に従う |

### P5 — 検証基盤

| # | 項目 | 現状 |
|---|---|---|
| 10 | 変換コアの unit test 皆無 | sRGB/PQ/HLG/行列の既知ベクトルテストを gtest で固定(`MILESTONE_AUTOMATED_TESTING_FOUNDATION` と連動) |
| 11 | preview/GPU/CPU/export の pixel parity ハーネス運用 | 比較ハーネスは存在するが判定記録ゼロ |

## 実施フェーズ

### Phase 1 — 生成色の空間明示(P1)

- 描画境界ヘルパー(例: `toWorkingSpaceFloatColor(QColor)`)を新設し、text/shape/solid/particle の色設定箇所から呼ぶ。
- 表示・QImage 互換境界での逆変換も明示関数として対称に用意する。
- 既存プロジェクトの見た目が変わるため、working space が sRGB/Linear の既定構成では挙動不変であることを確認してから ACEScg 構成で検証する。

### Phase 2 — OCIO 実体化(P2)

- 公式 ACES config 同梱とロード経路を追加する。
- `convertColor()` の統合または削除、`ColorSpace` 型の三重化整理、`ColorSpace.ixx` の AP0/AP1 コメント誤記修正。

### Phase 3 — 書き出し(P3)

- EXR float 直書き経路(multi-channel export の float buffer を流用)。
- OIIO attribute による ICC 埋め込み、FFmpeg カラータグ付与。

### Phase 4 — 空間統一・検証(P4/P5)

- エフェクト前後の decode/encode を契約に沿って適用(影響範囲が大きいため effect 単位で段階適用)。
- gtest による変換コアの固定テスト、parity ハーネスでの代表素材判定記録。

## 対象外

- HDR モニター出力(viewport HDR swapchain)
- tetrahedral LUT 補間
- 本物の ACES RRT+ODT 実装(簡易近似のまま)
- macOS/Linux カラーマネジメント

## リスクと確認方法

- **P1 は既存プロジェクトの見た目を変える**: 生成色が working space 変換を通るようになるため、ACEScg 構成のプロジェクトで色が変化する。移行フラグまたはバージョン境界の扱いを実装前に決める。
- **チャンネル順修正の影響確認**: 今回の修正で LiftGammaGain/ColorWheels/Curves の GPU 経路は BGRA バッファで初めて正しい色になる(以前は反転)。ユーザーが反転結果を前提に調整値を合わせている可能性があるため、リリースノートで明示する。
- **ビルド・テスト実行**: AGENTS.md 制約によりユーザー指示が必要。特に HLSL 変更(LiftGammaGain)は GPU 実機での確認が必要。
