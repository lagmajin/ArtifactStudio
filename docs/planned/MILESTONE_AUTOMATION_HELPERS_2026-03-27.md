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
