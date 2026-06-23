# Export / Review / Share (2026-03-27)

## Goal

書き出し結果をそのまま確認・共有しやすくする。

## Scope

- render result の quick review
- open output folder
- copy path / reveal / share
- image / video / audio / animated image の結果確認
- review 用の軽量 viewer 連携

## DoD

- export 後に結果を即確認できる
- 作業フォルダを手で探さなくてよい
- review 用の再生/参照導線が共通化される

## Notes

`Contents Viewer` と `Render Queue` の間を埋めるワークストリーム。

---

## Phase 1: Result Surface

### 目的

書き出し結果に、まず触れる場所を作る。

### 作業項目

- open output folder
- copy path
- reveal in explorer
- recent export result の一覧
- 成功 / 失敗 / 中断 の状態表示

### 完了条件

- 出力先を手で探さなくてよい
- 直近の結果がすぐ開ける

---

## Phase 2: Quick Review Handoff

### 目的

出力結果をそのまま確認できるようにする。

### 作業項目

- image / video / audio / animated image を Contents Viewer へ渡す
- final result を優先して開く
- render queue から review までの遷移を短くする

### 完了条件

- 書き出し直後に確認に移れる
- viewer のモード選択に迷わない

---

## Phase 3: Share / Compare Surface

### 目的

確認した結果を共有しやすくする。

### 作業項目

- share action
- compare view への送出
- bookmark / note の入口
- side-by-side review の起点

### 完了条件

- review から共有までの導線が短い
- compare へすぐ飛べる

---

## Phase 4: Diagnostics / Failure Context

### 目的

書き出し失敗時の理由を、その場で読めるようにする。

### 作業項目

- failure reason 表示
- missing source / codec / write failure の区別
- retry / rerender / open log の入口

### 完了条件

- 失敗しても次の行動が分かる
- render queue の診断と review の文言が揃う

---

## Phase 5: Automation Hooks

### 目的

よくある後処理を定型化する。

### 作業項目

- export 完了後 auto-open
- auto-review
- auto-copy path
- preset ごとの post action

### 完了条件

- 毎回同じ後処理を手で繰り返さなくてよい

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5
