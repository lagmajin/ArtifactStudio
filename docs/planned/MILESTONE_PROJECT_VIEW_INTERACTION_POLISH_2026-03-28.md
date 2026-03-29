# Project View Interaction Polish

> 2026-03-28 作成

## 目的

Project View を「一覧を見る場所」から「選択して次の操作へ進む場所」に寄せる。

既存の Project View は、検索、rename、delete、relink、drag & drop まで揃っている。
このマイルストーンでは、それらを選択中心の UI にまとめ、操作の入口を短くする。

---

## 背景

現在の Project View は機能量は十分に近いが、次の操作がやや散っている。

- 選択中アイテムの状態確認が上部に出ていない
- open / reveal / rename / delete / relink が散在している
- composition の double-click と open の意味が少し弱い
- keyboard と button の操作感がそろいきっていない

---

## 方針

1. 選択したら、何ができるかがすぐ見える
2. open / reveal / rename / delete / relink を近い位置に置く
3. composition は Project View からそのまま開ける
4. footage は Viewer と Explorer の両方へ自然に飛べる
5. 既存の search / filter / relink / delete は壊さず、入口だけ整える

---

## Phase 1: Selection Center

### 目的

選択中 item の状態と、今の filter 条件を見える化する。

### 作業項目

- selection summary の追加
- item type / state / path の表示
- selection change 時の quick action state 更新
- search / type / unused filter の状態表示

### 完了条件

- 選択を変えると summary が即更新される
- 何を選んだかと、何ができるかが見える

---

## Phase 2: Quick Actions

### 目的

よく使う操作を、右クリックに寄せすぎずワンクリックで触れるようにする。

### 作業項目

- `Open`
- `Reveal`
- `Rename`
- `Delete`
- `Relink`
- `Copy Path`

### 完了条件

- 選択 item に対して主要操作が 1 列で触れる
- footages と compositions の扱いが自然につながる

---

## Phase 3: Open / Double-Click Alignment

### 目的

double-click と open ボタンの意味をそろえる。

### 作業項目

- composition double-click で current composition を切り替える
- composition を開いたら composition viewer を前面に出す
- folder double-click で expand/collapse する
- footage double-click は contents viewer 導線に残す

### 完了条件

- double-click が item type ごとに期待通りの操作になる
- open ボタンが double-click と齟齬を起こさない

---

## Phase 4: Keyboard / Context Polish

### 目的

キーボード操作と context menu を、同じ意味の操作として揃える。

### 作業項目

- `Enter` / `F2` / `Delete` / `R` の挙動整理
- 既存 context menu 項目の並び直し
- 選択数が多いときの batch 操作の見せ方整理

### 完了条件

- keyboard と mouse の操作感がずれない
- context menu に頼らなくても主要操作が進む

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

---

## Current Status

2026-03-28 時点で、Project View は search / rename / delete / relink / drag & drop が既にある。
このマイルストーンは、それらを選択中心に束ねるための UI polish を目的とする。

