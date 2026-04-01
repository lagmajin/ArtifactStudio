# マイルストーン: Deep Compositing Support

> 2026-03-31 作成

## 目的

OpenEXR ベースの deep compositing / deep EXR の基盤を作る。

現状の flat RGBA compositing とは別に、

- depth sample
- per-pixel multi-sample
- holdout / matte
- deep merge
- deep read/write

を扱える経路を定義する。

---

## 現状

今の実装は、

- EXR sequence の入出力
- HDR / float image
- flat RGBA compositing

までは持っているが、deep sample を保持するデータ構造はない。

そのため Deep Compositing は「EXR が書ける」だけでは成立せず、内部表現から別になる。

---

## 方針

### 原則

1. flat compositing と deep compositing を混ぜない
2. 既存の EXR sequence 出力を壊さない
3. deep sample は内部構造として明示する
4. deep merge と holdout の責務を分ける
5. 最初は deep write / read の基盤までに留める

### 想定する型

- `DeepSample`
  - `depth`
  - `color`
  - `alpha / coverage`

- `DeepPixel`
  - 複数 sample を持つ

- `DeepImageBuffer`
  - ピクセルごとの sample list
  - bbox / tile / stride 相当の管理

---

## Phase 1: Deep Buffer Model

### 目的

deep compositing の内部表現を定義する。

### 作業項目

- `DeepSample` を定義する
- `DeepPixel` を定義する
- `DeepImageBuffer` を定義する
- flat buffer との変換経路を作る

### 完了条件

- deep sample を失わずに保持できる
- flat RGBA との往復が可能

---

## Phase 2: Deep EXR IO

### 目的

OpenEXR で deep read/write できるようにする。

### 作業項目

- deep EXR 読み込みを実装する
- deep EXR 書き出しを実装する
- channel / sample / depth の対応を整理する
- fallback として flat EXR 出力も残す

### 完了条件

- deep EXR を読み込める
- deep EXR を書き出せる
- 通常 EXR との互換導線が壊れない

---

## Phase 3: Deep Merge / Holdout

### 目的

deep compositing の中核である sample merge を実装する。

### 作業項目

- over / under merge
- holdout matte
- depth order merge
- sample pruning

### 完了条件

- 透明物体や奥行きを持つ重なりを deep で扱える
- flat compositing へ落とす前に merge ルールを定義できる

---

## Phase 4: UI / Workflow

### 目的

deep compositing を制作導線に乗せる。

### 作業項目

- deep buffer の preview
- deep EXR import/export UI
- deep 有効/無効の mode 切り替え

### 完了条件

- deep compositing を通常の render と区別して扱える
- ユーザーが deep データの有無を理解できる

---

## Non-Goals

- 既存 flat compositing の全置換
- いきなり full Nuke/Fusion 相当の deep node graph
- deep を必要としない通常レンダーの複雑化

