# MILESTONE: Sandbox Edits

日付: 2026-06-07

**最終更新:** 2026-08-15

## Update 2026-08-15

- 現行コードを再確認したが、一般編集を本番データから分離する `SandboxEdit`／session／snapshot の専用モデル、Enter／Compare／Apply／Discard の一体化した UI／サービスは確認できない。
- 既存の Undo snapshot、Composition の before／after preview、AI Command Sandbox、各種 preview cache は限定目的の仕組みであり、編集全体を隔離する sandbox の証拠にはならない。
- よって Phase 1〜3 は未実装扱いを維持する。特に discard／apply と既存 undo／保存状態の境界、runtime での安全性は未検証。

試しに変更して、比較して、気に入らなければ破棄できる一時編集モードを作る。

## 2026-07-25 実装監査

本番データから分離した編集 snapshot、sandbox mode の開始／終了、before／after 比較、差分ハイライト、Apply／Discard を一体で提供する専用実装は確認できない。既存の各種 preview、Undo snapshot、AI Command Sandbox は別の限定的な仕組みであり、本マイルストーンの一般編集 sandbox の完了証拠にはならない。したがって Phase 1〜3 は未実装として扱い、runtime の安全性も未検証とする。

## Goal

`Enter Sandbox Mode` から編集し、`Compare` で差分を見て、`Apply` / `Discard` で確定または破棄できるようにする。

## Non-Goals

- 既存の編集 UI を丸ごと置き換えない
- 保存済みプロジェクトを自動で壊すような即時反映はしない
- 新規の global signal-slot 経路を増やさない
- Diligent / D3D12 backend や低レベル render path には触らない

## Core Concept

- Sandbox は本番データの複製ではなく、一時的な編集レイヤーとして扱う
- 変更は apply されるまで本番へ反映しない
- 比較対象を明示して、差分が見える状態にする
- 色・モーション・サイズ・配置の試行をまとめて扱えるようにする

## Typical Flow

1. `Enter Sandbox Mode`
2. 色を変更する
3. モーションを変更する
4. サイズを変更する
5. `Compare` で差分を見る
6. `Apply` または `Discard` を選ぶ

## Why It Matters

- 試行錯誤の心理的コストが下がる
- 本番変更前に安心して触れる
- 複数案の比較がしやすくなる
- Motion Tokens や Content Bounds と組み合わせると評価しやすい
- 量産前の微調整を短くできる

## Phase 1: Sandbox State

目的: 一時編集状態を本番と分けて保持する。

- sandbox の開始 / 終了を持つ
- 編集前の snapshot を保持する
- 変更差分だけを追う
- 本番データと混同しない

確認観点:

- discard で元に戻る
- apply で本番へ反映される
- sandbox の途中状態を再表示できる

## Phase 2: Comparison View

目的: 比較の意味を UI と実装で揃える。

- before / after の表示
- 変更箇所のハイライト
- 色・モーション・サイズの差を見やすくする
- 必要なら bounds 情報も比較に使う

確認観点:

- 何が変わったか一目で分かる
- 差分が部分的でも全体の把握ができる
- 比較結果が本番の表示とずれにくい

## Phase 3: Apply / Discard

目的: 一時編集を安全に確定または破棄する。

- apply は必要な差分だけを反映する
- discard は snapshot に戻す
- 途中での破棄が安全に行える
- undo/redo と競合しない形を保つ

確認観点:

- 破棄後に残骸が残らない
- apply 後も編集フローが継続できる
- 既存の undo 履歴を壊さない

## Integration Notes

- Motion Tokens と組み合わせると、動きの候補を比較しやすい
- Content Bounds と組み合わせると、サイズや整列の評価がしやすい
- 最初は範囲限定の sandbox から始めるのが安全
