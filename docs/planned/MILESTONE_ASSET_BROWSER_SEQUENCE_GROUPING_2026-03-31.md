# マイルストーン: Asset Browser Sequence Grouping

> 2026-03-31 作成

## 目的

アセットブラウザで連番ファイルを自動グルーピングし、1 つの論理アセットとして扱えるようにする。

例:

- `image_0001.png`
- `image_0002.png`
- `image_0003.png`

を 1 アセットとして表示し、展開すると個別フレームを見られるようにする。

---

## 背景

連番素材は制作現場で頻繁に使うが、単純なフォルダ表示だと

- ファイル数が多くなる
- 意図したシーケンスが見えにくい
- 1 枚ずつ thumbnail を見ても全体が分かりにくい

という問題がある。

---

## 方針

### 原則

1. フォルダ構造は壊さない
2. 自動検出は正規表現ベースで行う
3. 論理アセットとしてまとめつつ、個別フレームへ展開できる
4. 既存の image / video / audio / folder 表示と共存させる
5. 認識できないものは通常ファイルとして残す

### 例

- `foo_0001.png` - `foo_0100.png`
- `shotA.0001.exr` - `shotA.0100.exr`
- `render-v003-0001.tif`

---

## Phase 1: Sequence Detection

### 目的

ファイル名から連番候補を検出する。

### 作業項目

- 正規表現で basename / frame number / padding を解析する
- 拡張子とディレクトリを含めてグルーピング条件を決める
- 連番として成立しないものは単独ファイルのままにする

### 完了条件

- 連番候補を安定して検出できる
- 誤検出が過剰にならない

---

## Phase 2: Sequence Item Model

### 目的

Asset Browser 内で連番を 1 item として扱う。

### 作業項目

- `SequenceAssetItem` のような論理 item を導入する
- 代表フレームの thumbnail を表示する
- 展開時に個別フレーム一覧へ入れる

### 完了条件

- 1 つの sequence が 1 行 / 1 タイルで見える
- 開くと個別フレームへアクセスできる

---

## Phase 3: Workflow Integration

### 目的

import / relink / preview / render queue へ接続する。

### 作業項目

- sequence を composition layer に import できる
- missing / unused / imported 状態を sequence 単位で扱う
- render / relink 時のパス解決を sequence aware にする

### 完了条件

- 連番素材を「1 本の素材」として扱える
- 制作ワークフローが煩雑にならない

