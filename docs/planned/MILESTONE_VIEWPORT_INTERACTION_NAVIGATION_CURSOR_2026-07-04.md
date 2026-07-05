# MILESTONE: Viewport Interaction / Navigation / 3D Cursor - 2026-07-04

**ステータス:** Not Started

作成日: 2026-07-04  
マイルストーンID: M-VP-9  
対象: Composition Editor / 3D Viewport / Overlay / Direct Manipulation  
優先度: High

---

## 目的

C4D の迷いにくい viewport navigation と、Blender の 3D Cursor / Pivot / Snap の柔軟さを、
ArtifactStudio の composition editing に合う一つの操作契約としてまとめる。

単にショートカットを模倣するのではなく、次を達成する。

- orbit / pan / dolly の意味が viewport 間で変わらない
- 選択対象や作業位置へすぐ戻れる
- preview view と render camera を誤って混同しない
- 3D Cursor を配置、pivot、orbit、生成、snap の共通基準点として使える
- 2D composition では不要な 3D 操作を露出しすぎない

---

## 基本方針

### C4D から取り入れるもの

- カーソル下または選択対象を Point of Interest とする navigation
- `Frame Selected` / `Frame All` による即時復帰
- active viewport の明確化
- viewport ごとに独立した display / camera / navigation state
- object undo とは分離した View Undo / Redo
- Default Camera と render camera の明確な区別

### Blender から取り入れるもの

- scene 内に明示的な 3D Cursor を置ける
- 3D Cursor を transform pivot / placement origin / snap target に使える
- selection、world origin、surface、grid、3D Cursor 間を素早く往復できる
- orientation と pivot source を別々に選べる

### ArtifactStudio 固有の調整

- 2D composition では 3D Cursor を `Work Cursor` として XY 平面上で扱える
- camera layer を変更しない preview-only navigation を既定にする
- HUD / navigation cross / cursor は editor-level overlay とし、native viewport の子にしない
- `ArtifactCompositionEditor` が surface routing と navigation session を所有する
- `ArtifactCompositionRenderWidget` は描画、hit test、direct manipulation surface を担当する
- 新規の公開 signal / slot や global singleton を前提にしない

---

## 用語と状態

### View Point of Interest

orbit / dolly の一時的な中心。カーソル下、選択中心、3D Cursor、viewport 中央から決定する。
navigation session の終了後に必要な範囲だけ view state へ保持する。

### 3D Cursor / Work Cursor

scene または composition 上の明示的な作業基準点。

- 3D composition: XYZ position と任意の orientation
- 2D composition: composition plane 上の XY position
- render output には含めない
- selection ではなく viewport tool state として扱う
- project persistence は Phase 3 で決定する

### Object Pivot

layer / object 自身の anchor / pivot。3D Cursor とは別状態とし、
`Pivot Source = Object / Selection / 3D Cursor / Individual` で参照先を切り替える。

### Preview View

render camera を変更しない editor camera state。camera editing を明示的に選んだ場合だけ
project の render camera へ反映する。

---

## スコープ

### In Scope

- viewport navigation の状態分離
- 3D Cursor / Work Cursor の最小操作
- Frame Selected / Frame All / View Undo / Redo
- preview-only navigation
- HUD / cursor / cross の editor overlay 配置

### Out of Scope

- full 3D DCC replacement
- 複数 viewport の高度なレイアウト管理
- グローバルイベントの新設
- object transform system の全面刷新

---

## 次の実装候補

- navigation session を editor 側に集約する
- 3D Cursor を overlay で見えるようにする
- Frame Selected / Frame All を existing action 系に接続する
- preview-only state と render camera state の分離を明示する

