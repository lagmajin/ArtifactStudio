# Automation Helpers (2026-03-27)

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
