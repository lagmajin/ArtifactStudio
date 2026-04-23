# Milestone: Asset Browser Left Pane Hub

> 2026-04-23 作成

`ArtifactAssetBrowser` の左側を、単なるフォルダ一覧ではなく「素材ハブ」として整理するマイルストーン。

見た目の寂しさを埋めるだけでなく、何があるか・何が危ないか・次に何をすればいいかを左側で読めるようにする。

---

## 背景

現状の `Asset Browser` 左ペインは、検索や一覧の補助としては機能していても、画面としては情報密度が薄く見えやすい。

特に以下が不足しやすい。

- いまのフォルダ / 素材群の短い要約
- 最近使った素材への素早い導線
- お気に入りや仮想カテゴリの入り口
- Missing / Relink 必要素材の注意表示
- ドラッグ&ドロップや quick import の受け皿

このため、左側を「ただの余白」ではなく「探索の入口」として再設計する。

---

## 目的

- 左ペインの情報密度を上げる
- ユーザーが今見ている素材の文脈を一目で読めるようにする
- `Recent` / `Favorites` / `Missing` / `Import` を左側から触れるようにする
- Asset Browser の探索導線を Project View と区別しながら強くする
- 既存の owner-draw / state chip / thumbnail 導線を活かす

---

## 非目的

- 右ペインの全面再設計
- Qt 標準 UI のまま大きく複雑化すること
- 見た目だけのデコレーションを増やすこと
- Project View と Asset Browser の責務を混ぜること

---

## 方針

### 1. 左ペインは「素材ハブ」にする

- 現在の場所を短く示す
- 最近触った素材を出す
- お気に入りを出す
- 例外状態を目立たせる
- すぐ import できる導線を置く

### 2. 情報は小さくまとめる

- 長い説明文は避ける
- サマリは 1 行か 2 行にする
- 状態は chip や badge で読ませる
- 余白は「空き」ではなく「区切り」として使う

### 3. 既存の Asset Browser 責務に寄せる

- 探索
- 絞り込み
- 最近使った素材
- お気に入り
- 同期状態
- import / relink の入口

---

## 想定レイアウト

### Top: Workspace Summary

- 現在のフォルダ / コレクション名
- アイテム件数
- Missing 数
- 選択中の種類

### Middle: Navigation / Shortcuts

- Recent
- Favorites
- Imported
- Missing
- Unused

### Bottom: Quick Actions

- Import Files
- Import Folder
- Relink Missing
- Open in Contents Viewer
- Refresh / Rescan

### Supplemental

- 小さい status note
- search / filter の現在値
- Project View との同期 chip

---

## フェーズ

### Phase 1: 左ペインの情報再配置

- 既存の一覧上部 / 下部の空きを再利用する
- サマリ行と短いステータスを追加する
- 状態表示を左ペインに集約する

### Phase 2: クイック導線

- Recent / Favorites / Missing / Import のショートカットを置く
- クリック 1 回でよく使う状態へ移動できるようにする

### Phase 3: 状態の見える化

- Missing / Relink / Unused を左ペインで読めるようにする
- 選択中素材の分類や同期状態を軽く出す

### Phase 4: 体験の仕上げ

- 空状態でも寂しく見えない構成にする
- 表示密度と余白のバランスを整える
- 必要なら owner-draw を強める

---

## 完了条件

- 左ペインを見ただけで「何がある場所か」が分かる
- Recent / Favorites / Missing / Import の入口がある
- 空状態でも単なる空白に見えない
- Asset Browser の探索導線として使いやすい
- Project View と責務が混ざらない

---

## リスク

- 詰め込みすぎると左ペインが逆にうるさくなる
- 状態表示が多すぎると本体の一覧より目立ってしまう
- Project View との同期情報を出しすぎると責務が曖昧になる
- 見た目の改善だけで終わると、実用導線が増えない

---

## 参照

- [`MILESTONE_ASSET_BROWSER_IMPROVEMENT.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md)
- [`MILESTONE_ASSET_BROWSER_NAVIGATOR_SEARCH_PRESENTATION_2026-04-03.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_SEARCH_PRESENTATION_2026-04-03.md)
- [`MILESTONES_BACKLOG.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)

---

## 次の一手

1. 左ペインに残す情報と移動先を決める
2. `Recent` / `Favorites` / `Missing` の優先順位を決める
3. 既存の Asset Browser shell にどの程度組み込むかを決める
