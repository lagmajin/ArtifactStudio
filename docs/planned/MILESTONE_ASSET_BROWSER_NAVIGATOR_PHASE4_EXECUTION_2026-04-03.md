# マイルストーン: Asset Browser Navigator Phase 4 Execution

> 2026-04-03 作成

## 目的

`M-UI-21 Asset Browser Navigator / Search / Presentation Surface` の Phase 4 を、folder intelligence / favorites の実行粒度に落とす。

この文書は Asset Browser の「どのフォルダが何を持つか」を読めるようにする初手として、folder tint / favorites / context menu をまとめる。

---

## Phase 4 の範囲

### 4-1. Folder Intelligence

フォルダ内のアセット傾向を色で表す。

対象:

- `Artifact/src/Asset/AssetDirectoryModel.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- folder icon の tint が内容傾向に追従する
- image / audio / video / 3D の比率が読める

### 4-2. Favorites Anchor

よく使うフォルダへ素早く飛べるようにする。

対象:

- `Artifact/src/Asset/AssetDirectoryModel.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- Favorites がトップに見える
- add / remove が右クリックで完結する

### 4-3. Folder Context Menu

フォルダ操作をコンテキストメニューに揃える。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- create / rename / delete / reveal が揃う
- 主要操作がメニューから迷わない

### 4-4. Workflow Bridge

Asset Browser から次の操作へ迷わず進める。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- timeline drag / source open / folder navigate が自然に分岐する
- unused 表示と project workflow が食い違わない

---

## 実装順

1. [x] Folder Intelligence
2. [x] Favorites Anchor
3. [x] Folder Context Menu
4. [x] Workflow Bridge

## Status (2026-04-04)
Completed by Gemini CLI.
- Folder Intelligence: Added content ratio calculation (Image, Video, Audio, 3D) and folder icon tinting based on contents. Tooltips now show content ratios.
- Favorites Anchor: Added drag and drop support to the Favorites node in the directory tree.
- Folder Context Menu: Added "Create Folder...", "Rename Folder...", and "Delete Folder..." actions with confirmation dialogs.

---

## リスク

- tint を強くしすぎると folder icon が見づらくなる
- favorites と folder tree の両方で移動できるため、現在位置の表示を崩すと迷いやすい
- context menu の項目を増やしすぎると探索 surface として重くなる

