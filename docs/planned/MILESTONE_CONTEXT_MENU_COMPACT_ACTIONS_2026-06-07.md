# Context Menu Compact Actions Milestone

**作成日:** 2026-06-07  
**ステータス:** 計画中  
**関連コンポーネント:** Context Menu, Layer Panel, Inspector, Composition Editor, Menu Registry

---

## 概要

右クリックメニューを短く、探しやすく、カスタマイズしやすくするためのマイルストーンです。

最初に表示する項目数を抑え、よく使う操作と全機能を分離します。  
目的は「30 個並ぶ長いメニューを読む」のではなく、「よく使う 10 個から始める」ことです。

---

## 背景

現状の右クリックメニューは、機能が増えるほど縦長になり、目的の操作に辿り着きにくくなります。

- 項目が多すぎてスクロールが必要
- カテゴライズが弱い
- 初見で探索しづらい
- どれが頻出操作か分からない

---

## 目標

- 先頭に `Frequent` セクションを置く
- 初期表示は 10 項目以内にする
- 下部に `Show All...` を置く
- ユーザーがよく使う項目をカスタマイズできるようにする
- カテゴリごとの折りたたみを可能にする

---

## メニュー構成

### 1. Frequent

- よく使う項目を最上段に集約する
- 初期状態は少数に絞る
- 直近使用履歴や user pin で並べ替える

### 2. Categories

- transform
- keyframe
- timing
- visibility
- copy / paste
- render / preview

### 3. Advanced / All

- すべてのコマンドを表示する
- カテゴリを維持したまま展開する
- 検索や type-ahead にも繋げられるようにする

---

## Phase 構成

### Phase 1: Frequent / All Split

- context menu を頻出項目と全項目に分ける
- 先頭 10 個以内の初期表示を固定する
- `Show All...` の導線を作る

完了条件:

- メニューを開いた瞬間に目的候補が見える

### Phase 2: Category Grouping

- コマンドをカテゴリ単位で整理する
- 似た操作が塊として見えるようにする
- 長い menu を意味のあるブロックに分ける

完了条件:

- 30 個の羅列ではなくなる

### Phase 3: Customization

- frequent 項目を user が pin / unpin できるようにする
- 直近使用履歴を頻度に反映する
- surface ごとに既定の frequent 集を持てるようにする

完了条件:

- よく使う操作が人ごとに最適化できる

### Phase 4: Searchable All Menu

- all menu に検索や type-ahead を足す
- 項目名だけでなくカテゴリ名でも絞れるようにする
- 深い階層でも辿り着けるようにする

完了条件:

- 長いメニューでも迷いにくい

### Phase 5: Surface-Specific Tuning

- layer / timeline / inspector / composition で頻出項目を変える
- ただし構造は共通に保つ
- surface ごとの違いは `frequent set` だけに寄せる

完了条件:

- 文脈ごとの最適化ができる

---

## リスクと留意点

- 項目を減らしすぎると逆に見つけにくくなる
- カスタマイズを入れすぎると既定値の設計が難しくなる
- コンテキストメニューと command palette の役割が重ならないようにする必要がある

---

## 成功条件

- 右クリックしてすぐ使う操作が見える
- 全機能は失われず、必要なら辿れる
- よく使う項目を人に合わせて調整できる
- 長い menu を読む負担が減る

---

## 関連

- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`
