# MILESTONE: Cycle Shortcuts

日付: 2026-06-07

**最終更新:** 2026-08-15
**判定:** 候補回し専用の cycle 実装は未着手。既存の shortcut registry／context help／設定画面のみ基盤として利用可能。

Import Cycle と同じ思想で、キー連打で候補を回しながら編集を進めるキーボード中心のワークフローを作る。

## Goal

`E` で Ease 候補、`M` で Motion preset、`A` で Anchor、`F` で Fit、`L` で Layout の候補を回し、画面上に現在の候補を明示できるようにする。

## Non-Goals

- 既存ショートカット体系を全面置換しない
- 新規の global signal-slot 経路を増やさない
- Diligent / D3D12 backend には触らない
- マウス操作をなくすことを目的にしない

## Core Concept

- 1 キーで 1 種類の候補群を回す
- 候補の現在値を画面に表示する
- 連打で素早く探索し、確定は別操作にする
- Import Cycle の「回して選ぶ」体験を他の編集にも広げる

## Typical Cycles

- `E`: Ease 候補を回す
- `M`: Motion preset を回す
- `A`: Anchor 候補を回す
- `F`: Fit 候補を回す
- `L`: Layout 候補を回す

## Why It Matters

- 候補選択をマウス依存にしない
- 反復試行の速度が上がる
- 設定ダイアログを開かずに探索しやすい
- `WorkspaceMode` や将来の作業文脈切替と組み合わせると候補回しがしやすい

## 2026-08-15 現行コード照合

`ShortcutBindings`、workspace context ごとの shortcut help、設定画面の preset import／export は実装されているが、`E`／`M`／`A`／`F`／`L` の候補セット、current index、preview overlay、commit／revert の cycle state は現行コード上で確認できなかった。Timeline の Ease 操作や既存ツールキーは、この milestone の候補循環 UX とは別機能として扱う。

したがって、旧設計の候補回し機能は未実装、ショートカット基盤だけ部分利用可能という判定に更新する。実装時は `G/R/S/X/Y/Z` の汎用変換操作や既存コンテキストと衝突しない定義が必要であり、今回はビルド・テストを実行していない。

## Phase 1: Cycle Definition

目的: どのキーがどの候補群を回すかを定義する。

- shortcut id
- candidate set
- current index
- display label

確認観点:

- キーと候補群の対応が明確
- 候補群ごとに独立して回せる
- 既存ショートカットと競合しない

## Phase 2: On-Screen Feedback

目的: 現在どの候補を回しているか見えるようにする。

- current candidate overlay
- selected value label
- next / previous hint

確認観点:

- いま何を選んでいるか分かる
- 連打時に迷いにくい
- 確定前の探索がしやすい

## Phase 3: Commit Flow

目的: 候補を回したあとに確定できるようにする。

- apply current candidate
- revert to previous candidate
- optional lock

確認観点:

- 誤操作から戻せる
- 確定手順が分かりやすい
- preview と実データがずれない

## Integration Notes

- `Motion Tokens` と相性が良い
- `Coordinate Profiles` のような候補回しにも応用できる
- `WorkspaceMode` や panel context ごとに候補セットを切り替えやすい
