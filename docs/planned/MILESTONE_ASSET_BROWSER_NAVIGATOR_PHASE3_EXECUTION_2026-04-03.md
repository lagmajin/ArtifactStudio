# マイルストーン: Asset Browser Navigator Phase 3 Execution

> 2026-04-03 作成

## 目的

`M-UI-21 Asset Browser Navigator / Search / Presentation Surface` の Phase 3 を、content presentation の実行粒度に落とす。

この文書は Asset Browser の「見せる・切り替える」を整える初手として、grid / list / thumbnail / status をまとめる。

---

## Phase 3 の範囲

### 3-1. Grid / List Presentation

グリッドとリストを制作向けに見やすく切り替える。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- grid / list の切替が明示的
- list で icon / name / type / size / modified time が読める

### 3-2. Thumbnail Slider

サムネイルサイズを連続制御に寄せる。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- 32px - 128px の連続変化ができる
- 最小で list 表示へ自動遷移する

### 3-3. Status Bar Surface

item / selection / unused の件数を読めるようにする。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

完了条件:

- item count が見える
- selection count が見える
- unused count が見える

### 3-4. Type / Icon Semantics

ファイルタイプの色とアイコンの意味を揃える。

対象:

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Asset/AssetDirectoryModel.cppm`

完了条件:

- video / image / audio / 3D / comp / folder / text の意味が揃う
- thumbnail 未生成時の placeholder が分かる

---

## 実装順

1. [x] Grid / List Presentation
2. [x] Thumbnail Slider
3. [x] Status Bar Surface
4. [x] Type / Icon Semantics

## Status (2026-04-04)
Completed by Gemini CLI.

---

## リスク

- thumbnail を大きくしすぎると一画面の密度が落ちる
- list/grid の見た目差が強すぎると操作が分断される
- status を増やしすぎると search 結果との区別がつきにくい

