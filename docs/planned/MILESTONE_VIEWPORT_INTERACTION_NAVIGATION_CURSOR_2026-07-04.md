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
camera layer の transform を変更する。

---

## 操作契約

### Navigation baseline

| 操作 | 既定動作 |
|---|---|
| `Alt + Left Drag` | Point of Interest を中心に orbit |
| `Alt + Middle Drag` または `Middle Drag` | pan |
| `Alt + Right Drag` | cursor position を基準に dolly |
| Wheel | cursor position を基準に段階 zoom / dolly |
| `S` | Frame Selected |
| `H` | Frame All / Frame Geometry |
| `Ctrl+Shift+Z` | View Undo |
| `Ctrl+Shift+Y` | View Redo |

既存 shortcut と衝突する場合は Shortcut Registry の現状を確認してから割当を確定する。
key event を各 widget に重複実装しない。

### 3D Cursor baseline

| 操作 | 既定動作 |
|---|---|
| Cursor tool + click | surface / composition plane に配置 |
| `Shift + S` 相当の command palette | Cursor to Selection / Origin / Grid、Selection to Cursor |
| Pivot selector | Object / Selection / 3D Cursor / Individual |
| Orbit source selector | Auto POI / Selection / 3D Cursor / View Center |
| Create / Paste | 選択中の placement source に生成 |

Blender と同じキーへ固定することより、command 名と状態が明確であることを優先する。

---

## フェーズ

### Phase 1: Navigation Contract

- [ ] preview-only view state と camera layer state を分離する
- [ ] orbit / pan / dolly の入力文法を Composition viewport で統一する
- [ ] cursor-under-point / selection center から Point of Interest を決定する
- [ ] navigation 開始時に短時間だけ navigation cross を表示する
- [ ] `Frame Selected` / `Frame All` を 2D / 3D の共通 command にする
- [ ] active viewport を細い枠または既存 theme token で示す

完了条件:

- view navigation だけでは render camera の transform が変わらない
- orbit 開始時に対象が不自然に飛ばない
- 選択対象を失っても 1 command で復帰できる

### Phase 2: View History / HUD

- [ ] viewport ごとの View Undo / Redo ring を追加する
- [ ] orbit / pan / dolly / frame / camera switch を view history に記録する
- [ ] object transform undo stack とは分離する
- [ ] 左上に projection / view name、中央上に active camera を表示する
- [ ] Default Camera / render camera / preview-only を判別できる表示にする
- [ ] multi-view では active pane だけが shortcut command を受ける

完了条件:

- 誤った navigation を object undo に触れず戻せる
- multi-view のどの pane を操作しているか視覚的に分かる
- HUD が viewport content や transform handle を過度に覆わない

### Phase 3: 3D Cursor / Work Cursor

- [ ] viewport tool state に cursor position / orientation を定義する
- [ ] 2D plane、3D grid、visible surface への配置方法を分ける
- [ ] depth hit が取れない場合の grid / active plane fallback を定義する
- [ ] cursor overlay と hit target を実装する
- [ ] Cursor to Selection / World Origin / Grid を command 化する
- [ ] Selection to Cursor を undoable transform operation として実装する
- [ ] project / composition / session のどこへ保存するかを責務確認して決定する

完了条件:

- cursor を置いても現在選択が解除されない
- 2D と 3D で配置位置が予測可能
- cursor 自体は render result に現れない

### Phase 4: Pivot / Orientation / Creation

- [ ] pivot source を Object / Selection / 3D Cursor / Individual から選択できる
- [ ] transform orientation を World / Local / View / Cursor から選択できる
- [ ] 既存 anchor / pivot editing と 3D Cursor を混同しない
- [ ] object / layer creation の placement source に 3D Cursor を追加する
- [ ] duplicate / paste placement が command ごとに不一致にならないよう統一する
- [ ] direct manipulation 中に pivot source と orientation を小さく表示する

完了条件:

- 3D Cursor を中心に複数選択を回転 / scale できる
- object pivot を編集しても 3D Cursor は移動しない
- 生成位置が現在の placement source と一致する

### Phase 5: Snap / Surface Interaction

- [ ] cursor / selection 双方で grid、vertex、edge、surface、center snap を共通利用する
- [ ] snap candidate と確定点を overlay で区別する
- [ ] occluded surface と visible surface の優先規則を定義する
- [ ] modifier key は既存 transform / accessibility 設定との衝突を確認する
- [ ] shape、field、camera target など固有 handle へ同じ snap contract を展開する

完了条件:

- cursor placement と transform manipulation で snap 精度が一致する
- snap 対象が視覚的に説明される
- surface miss や depth ambiguity で cursor が遠方へ飛ばない

---

## 状態所有と責務

| 責務 | owner 候補 |
|---|---|
| navigation session / active pane routing | `ArtifactCompositionEditor` |
| viewport drawing / cursor overlay / hit test | `ArtifactCompositionRenderWidget` |
| pan / zoom / projection calculation | `ArtifactCompositionRenderController` / existing viewport transform path |
| selection center | existing layer selection manager |
| object anchor / pivot | existing transform model |
| shortcut resolution | existing Shortcut Registry / command path |
| project persistence | existing project or composition serialization owner |

実装開始前に現行コードで owner を再確認する。古い文書にある
`ViewportBookmarkManager` singleton や新規 EventBus multicast は、そのまま採用しない。

---

## 非目標

- Blender の edit mode や mesh component editing 全体の再現
- C4D の camera system の完全複製
- 3D Cursor を layer / renderable object として追加すること
- viewport navigation のために render camera を暗黙更新すること
- 新規 QtCSS、`QColorDialog`、`QImage`、`QPainter` composition path の追加
- Diligent backend / DX12 low-level path の広範な変更

---

## 検証シナリオ

1. 2D layer を選択して `S` を実行し、選択 bounds が安定して収まる
2. 3D object 上から orbit を開始し、同じ Point of Interest の周囲を回る
3. preview view を動かしても active render camera の transform が変わらない
4. view を数回動かし、View Undo / Redo だけで視点を往復できる
5. 3D Cursor を surface に置き、複数選択を cursor pivot で回転できる
6. 2D composition で Work Cursor を置き、layer をその位置へ移動できる
7. multi-view で各 pane の view history / cursor visibility / camera 表示が混線しない
8. cursor placement miss 時に origin や極端な depth へ飛ばない

---

## 推奨実装順

1. Phase 1: Navigation Contract
2. Phase 2: View History / HUD
3. Phase 3: 3D Cursor / Work Cursor
4. Phase 4: Pivot / Orientation / Creation
5. Phase 5: Snap / Surface Interaction

Phase 1-2 で C4D 的な触りやすさを先に成立させ、Phase 3-5 で Blender 的な
基準点ワークフローを段階的に追加する。

---

## 関連文書

- `docs/WIDGET_MAP.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`
- `docs/planned/MILESTONE_MAYA_VIEWPORT_OPERATIONS_2026-03-25.md`
- `docs/planned/MILESTONE_MULTI_VIEWPORT_LAYOUT_2026-06-01.md`
- `docs/planned/MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md`
- `docs/planned/MILESTONE_VIEWPORT_CANVAS_ROTATION_2026-06-27.md`

---

## 未確認事項

- 現在の shortcut conflict と command routing
- depth picking / surface hit API の現状
- viewport camera state と render camera state の実際の分離状況
- multi-view prototype の active pane state と persistence
- 3D Cursor を project / composition / session のどこへ保存するのが適切か

ビルド、テスト、runtime verification は実施していない。
