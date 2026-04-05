# マイルストーン: Asset Browser Navigator Phase 2 Execution

> 2026-04-03 作成

## 目的

`M-UI-21 Asset Browser Navigator / Search / Presentation Surface` の Phase 2 を、search mode / flat results の実行粒度に落とす。

この文書は Asset Browser の「探す・絞る」を確立する初手として、検索状態の表示と右ペインの flat surface をまとめる。

---

## Phase 2 の範囲

### 2-1. Incremental Search

入力中に結果が更新される検索を安定化する。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Asset/AssetDirectoryModel.cppm`

完了条件:

- 入力した瞬間に結果が変わる
- 検索中でも UI が固まらない

### 2-2. Search Mode Surface

検索中は folder 階層を隠し、flat な結果 surface を出す。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- path rail が `検索結果: "..."`
  に切り替わる
- folder tree を残しつつ、右ペインは flat に見える

### 2-3. Search Result Presentation

検索結果の badge / status / presentation を揃える。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Asset/AssetDirectoryModel.cppm`

完了条件:

- source / type / status が検索中も読める
- item count が結果件数と一致する

### 2-4. Search Navigation

検索結果から対象素材へ素早く移動できるようにする。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- 検索から folder double click / source open へ迷わない
- back / forward と search の切り替えが破綻しない

---

## 実装順

1. [x] Incremental Search
2. [x] Search Mode Surface
3. [x] Search Result Presentation
4. [x] Search Navigation

## Status (2026-04-04)
Completed by Gemini CLI.

---

## リスク

- search mode と breadcrumb 表示が混ざると現在位置が読みにくい
- flat result と folder tree の責務が曖昧だと操作が重くなる
- status / badge の表示を増やしすぎると逆に情報過多になる

