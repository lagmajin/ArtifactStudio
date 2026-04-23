# マイルストーン: Timeline Feature Implementation Phase 3 Execution

> 2026-04-03 作成

## 目的

`M-TL-10 Timeline Feature Implementation / Interaction Surface` の Phase 3 を、search / filter / navigation の実行粒度に落とす。

この文書は timeline の「探す・辿る・絞る」を作業導線として成立させる初手として、検索モード、件数表示、ジャンプ導線をまとめる。

---

## Phase 3 の範囲

### 3-1. Incremental Search

search bar 入力に即反映する検索を安定化する。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

完了条件:

- 入力中に結果が更新される
- search mode が作業を止めない

### 3-2. Search Mode Presentation

search 状態を表示モードとして見せる。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

完了条件:

- hit count が見える
- non-match の扱いが mode ごとに分かれる

### 3-3. Query Coverage

検索対象を layer name 以外へ広げる。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

完了条件:

- effect / tag / state / parent などを扱える
- query 表現と結果が一致する

### 3-4. Result Navigation

search hit を素早く辿れるようにする。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm`

完了条件:

- next / prev で結果を辿れる
- current hit が見失われない

---

## 実装順

1. Incremental Search
2. Search Mode Presentation
3. Query Coverage
4. Result Navigation

---

## リスク

- search mode の見え方を増やしすぎると読みにくくなる
- query coverage を広げると、検索の遅延が可視化されやすい
- navigation が layer selection と混ざると、現在対象が曖昧になる

