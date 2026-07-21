# MILESTONE_COLOR_BACKEND_HARDENING_2026-07-21

**Status:** In Progress (P0 ✅ / P1 Partial / P2 ✅)
**Goal:** カラーコレクションのバックエンドの未定義動作・データ競合・スタブを潰し、UI 層の前に土台を固める。

## 背景

`docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` および成熟度分析レポート (p2/p4/p5, 2026-06-03) において、カラー系バックエンドに以下の問題が指摘されている：

- GradingNode union 操作の UB（3 レポート重複）
- ColorNodeGraph / Saturation のデータ競合
- ColorScienceManager の設定反映 no-op
- GradingEngine の preset 保存/読み込み スタブ
- GPU カーブがプリセット限定

## Scope

| # | 優先度 | 項目 | ファイル | 内容 |
|---|--------|------|----------|------|
| 1 | ✅ P0 | GradingNode operator= UB | `Artifact/src/Color/ArtifactColorGradingEngine.cppm` | union active member の型不一致による未定義動作を修正。operator= で破棄→再構築の順序を修正、デストラクタで active member を明示破棄 |
| 2 | ✅ P0 | ColorNodeGraph data race | `Artifact/src/Color/ArtifactColorNodeGraph.cppm` | `ensureOrder()` の遅延構築キャッシュに `mutable std::mutex orderMutex_` 追加、double-checked locking で保護 |
| 3 | ✅ P0 | Saturation mutex 未使用 | `ArtifactCore/src/Color/Sarturation.cppm` | `setSaturation()` / `saturation()` に `std::lock_guard` 追加 |
| 4 | ✅ P1 | applySettings() no-op | `Artifact/src/Color/ArtifactColorScienceManager.cppm` | `conversionCache_` クリア + `settingsChanged()`/`lutChanged()` signal 発行に変更 |
| 5 | ❌ P1 | GPU Curves 任意ポイント対応 | `ArtifactCore/include/Graphics/Shader/HLSL/ColorCorrectionShaders.ixx` | プリセット選択から任意 LUT テーブルアップロードに拡張 |
| 6 | ✅ P1 | Preset save/load スタブ | `Artifact/src/Color/ArtifactColorGradingEngine.cppm` | GradingEngine の preset JSON 保存/読み込み/list を実装 |
| 7 | ✅ P2 | FloatColor ヒープアロケーション | `ArtifactCore/src/Color/FloatColor.cppm` + `.ixx` | pimpl → inline 4-float メンバに変更。全メソッドをヒープアロケーション不要の inline 実装に |
| 8 | ✅ P2 | ColorPaletteManager 精度 | `Artifact/src/Color/ColorPaletteManager.cppm` | 現在のコードは既に `colorToJsonObject()` で float → JSON double の保存パスを使用。旧来の `HexArgb` は読み込み専用のレガシーパス。問題解消済み |

## Non-goals

- UI 改修（LUT Browser / OCIO / Scopes / Color Correction Rack は別 milestone）
- 新規シグナル&スロットの追加
- ArtifactWidgets / ArtifactCore サブモジュールの単独変更（親リポジトリから bump で扱う）

## 実施順序

1. P0 3 件（即時修正必須）
2. P1 3 件（applySettings → GPU Curves → Preset save/load）
3. P2 2 件（パフォーマンス・精度改善）

## 関連

- `docs/planned/MILESTONE_COLOR_CORRECTION_2026-03-27.md`
- `docs/planned/MILESTONE_COLOR_GRADING_2026-03-29.md`
- `docs/done/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`
- `docs/done/MILESTONE_SOLID_COLOR_EFFECTS_2026-06-27.md`
- `ae_maturity_additional_analysis_p2.md` (#32, #37)
- `ae_maturity_additional_analysis_p4.md` (#13, #24, #25, #28)
- `ae_maturity_additional_analysis_p5.md` (#41, #45)
