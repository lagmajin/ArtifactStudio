# Phase 3: Useful HUD / Context Overlay / Probe

> 2026-04-21 作成

## Goal

選択中の要素に対して、操作に有用な補助情報をまとめて出す。

## Scope

- layer name
- blend mode
- mask count / matte presence
- ROI / frame / timecode
- current frame / decode state
- pixel / region probe

## Notes

- HUD は selection 時だけ出す
- probe は軽量にする
- diagnostics と重複する情報は再利用する

## Success Criteria

- 選択レイヤーの状態が即読める
- マスク / マット / video の状態が分かる
- region probe で問題箇所を追いやすい
