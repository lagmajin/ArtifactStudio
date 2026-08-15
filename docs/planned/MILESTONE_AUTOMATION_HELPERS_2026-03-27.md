# Automation Helpers (2026-03-27)

**最終更新:** 2026-08-15

### 2026-08-15 follow-up

- Command Palette の JSON MRU 復元で空 ID と重複 ID を除外し、再起動後の順位情報を正規化するようにした。
**判定:** Command Palette／Recipe／WorkspaceAutomation の基盤は実装済み。外部向け macro／全 workflow の runtime 受入れは未完了。

## Goal

制作中の定型作業を少しずつ自動化できる土台を作る。

## Scope

- command palette / quick action
- batch rename / batch relink / batch export
- preset save / recall
- simple script hook / macro entry
- repetitive workflow のテンプレート化

## DoD

- 繰り返し操作を手で連打しなくてよい
- preset と command が再利用できる
- 後から automation を増やしやすい

## Notes

`Feature Expansion` の Phase 0 と各機能をつなぐ補助ワークストリーム。
# 2026-07-10 Progress

- Composition Editor Command Palette に `Repeat Last Action` を追加
- 直前の repeatable action を名前付き Recipe としてセッション内保存・再実行可能にした
- Batch / Paste Special / Published Controls / Responsive Preview / Auto Precompose を同じ入口へ統合した
- palette labelを View / Tool / Selection / Batch / Safety 等の検索可能なcategory prefixへ整理した
- `artifact.parameter-recipe.v1` descriptorを導入し、actionId + parametersをQSettingsへJSON永続化する縦スライスを追加
- 初期対応はBatch RenameのbaseNameで、再起動後も`[Persistent]` Recipeとして再実行できる
- command labelごとのusage countと直近12件をQSettingsへ保存する
- usage count + recency boostでPalette項目をstable sortし、日常的な操作を自動的に上位表示する
- Paletteから任意commandをPin / Unpinでき、favorite boostをusage rankingより優先する
- usage countとrecent historyだけを初期化するReset actionを追加（PinとRecipeは保持）

## 2026-08-15 現行コード照合

`WorkspaceAutomation`、`CommandIRExecutor`、AI tool bridge、interactive shell、Render Queue API を確認した。レイヤー／コンポジション／エフェクト／キーフレーム／アセット／再生／レンダーキューの操作が共通の automation surface に登録され、Command Palette の Recipe、pin、usage／recent 永続化も実装されている。batch rename／project item 移動、render queue 編集・開始・状態取得の API も確認できる。

一方、DoD の「繰り返し操作を再利用できる」は API／descriptor の静的存在だけでは保証できない。Undo 境界、失敗時の部分適用、再起動後の Recipe の全 action、外部 script／macro の長時間運用、UI と AI／CLI の結果整合は runtime 未検証。今回はビルド・テストを実行していない。
