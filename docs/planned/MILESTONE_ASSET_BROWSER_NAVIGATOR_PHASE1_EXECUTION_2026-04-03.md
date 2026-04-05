# マイルストーン: Asset Browser Navigator Phase 1 Execution

> 2026-04-03 作成

## 目的

`M-UI-21 Asset Browser Navigator / Search / Presentation Surface` の Phase 1 を、実装順がぶれない粒度に落とす。

この文書は Asset Browser の shell を DCC っぽい navigator として固める初手として、splitter / breadcrumb / history / favorites をまとめる。

---

## Phase 1 の範囲

### 1-1. Splitter Shell

左右 2 ペインの境界を安定させる。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- 左右の比率をドラッグで変えられる
- 左ペインの最小 / 最大幅が破綻しない

### 1-2. Breadcrumb / Path Rail

現在位置をクリック可能なパンくずとして見せる。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- 現在フォルダが分かる
- クリックで上位階層へ戻れる

### 1-3. History Navigation

back / forward の履歴遷移を安定化する。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Asset/AssetDirectoryModel.cppm`

完了条件:

- フォルダ移動の往復ができる
- 履歴の末端状態が分かる

### 1-4. Favorites Anchor

よく使うフォルダへのショートカットを固定する。

対象:

- `Artifact/src/Asset/AssetDirectoryModel.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- Favorites セクションがトップに見える
- 右クリックまたは操作から登録できる

---

## 実装順

1. [x] Splitter Shell
2. [x] Breadcrumb / Path Rail
3. [x] History Navigation
4. [x] Favorites Anchor

## Status (2026-04-04)
Completed by Gemini CLI.

---

## リスク

- breadcrumb と search mode の表示を混同すると path 表示が壊れる
- history が tree navigation と二重化すると current directory が分かりにくくなる
- favorites の保存先を固定しすぎると project 切替時に混乱する

