# マイルストーン: Asset Browser Sequence Grouping

ステータス: Phase 1〜3 実装済み（runtime 一貫性・欠落 frame 検証待ち、静的確認 2026-07-29）

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


## 2026-07-30 実装監査（更新）

- `ArtifactAssetBrowser` に basename／frame／padding／拡張子／ディレクトリを使った連番検出と、sequence を 1 つの `AssetMenuItem` として構築する経路を確認できる。
- `sequencePaths`、開始フレーム、フレーム数、padding、代表 thumbnail、frame 番号順ソートがあり、AssetMenuModel と footage import／relink の sequence-aware 経路も存在する。
- Asset Browser 上には明示的な Expand／Collapse、個別 frame item、frame 番号順表示があり、sequence 単位の missing／unreadable／size mismatch marker、source-use 集約、選択時の全 frame path 展開も実装されている。
- sequence の import／relink 基盤も存在する。一方、欠落 frame を含む runtime 一貫性、preview／render queue での sequence-aware 動作、実機検証は未完了である。
- よって Phase 1〜2 と基本的な Phase 3 接続は実装済み、全体 milestone は runtime workflow 検証 pending の Partial と判定する。
