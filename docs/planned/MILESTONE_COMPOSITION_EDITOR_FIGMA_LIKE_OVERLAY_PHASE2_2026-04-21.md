# Phase 2: Selection Overlay / Handles / Bounds

> 2026-04-21 作成

## Goal

選択レイヤーの外接矩形、中心、回転、サイズを見えるようにする。

## Scope

- selection bounds
- center point
- rotation handle
- size / position / rotation angle HUD

## Notes

- overlay は composition editor の描画経路に統合する
- update 順のズレが出ないよう、別 widget は増やさない

## Success Criteria

- 選択状態が一目で分かる
- 回転と拡縮の基準が見える
- overlay の更新タイミングが安定する
