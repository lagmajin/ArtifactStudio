# M-LA-4 Parent Pick-Whip Milestone

作成日: 2026-07-03
ステータス: Draft
対象: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`,
      `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`,
      `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`,
      `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Service/ArtifactLayerService.cppm`,
      `Artifact/src/Undo/*`
位置づけ: `Parent Pick Whip` を layer parenting の操作導線として追加し、既存の `Select Parent Layer` メニューと layer parent データモデルの上に、AE 風の drag-to-parent workflow を載せる。
参照:
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#6)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md`
- `docs/planned/MILESTONE_LAYER_PARENT_VISUAL_2026-04-10.md`
- `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`
- `docs/WIDGET_MAP.md`

---

## 1. 目的

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` では、parenting のデータモデルと `Select Parent` メニューはある一方で、**drag-to-parent の操作クラス・ツールが未発見** と整理されている。

AE では、child layer の pick-whip を親 layer にドラッグして parenting を作る操作が日常的に使われる。これが無いと:

- parent を探して menu から選ぶ必要がある
- 多数 layer で階層を組む時に視線往復が増える
- parenting の作成と確認が別操作になって流れが切れる

本 milestone では、**property linking 用 pick-whip ではなく layer parenting 用 pick-whip** を対象にする。

---

## 2. 現状整理

### 2.1 既存資産

- `ArtifactAbstractLayer` に parent 関連のデータモデルがある
- `ArtifactCompositionEditor.cppm` に `Select Parent Layer` 導線がある
- `MILESTONE_LAYER_PARENT_VISUAL_2026-04-10.md` に parent/child 可視化の方向性がある
- `docs/WIDGET_MAP.md` 上、layer row 操作は `ArtifactLayerPanelWidget` が正規責務

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Parent pick-whip affordance | なし | 直感的に親付けできない |
| Drag preview | なし | どこへ繋がるか見えない |
| Drop validation | なし | self-parent / loop を事前拒否できない |
| Timeline row integration | なし | layer panel で完結しない |
| Undo | parenting 変更の専用粒度が弱い | 1 undo で戻しにくい |
| Unparent shortcut | menu 中心 | 解除が遠い |

### 2.3 スコープ外

- property reference linking 用 pick-whip
- expression resolver
- viewport 上での parent pick-whip
- group layer / material container 独自ルール

---

## 3. 設計の柱

### 3.1 操作面

Parent pick-whip の入口は 2 つ持つ:

1. `ArtifactLayerPanelWidget`
   各 layer row に小さな parent target glyph を置く
2. `ArtifactInspectorWidget`
   Layer summary の parent 行から drag 開始できる

どちらも **drop 先は layer row** に統一する。

### 3.2 ドラッグ挙動

- glyph mouse press で drag 開始
- drag 中は source layer から cursor へ細い接続線を描く
- hover 中の layer row が parent 候補なら highlight
- mouse release で parent 設定

### 3.3 バリデーション

drop 前に次を拒否する:

- self-parent
- descendant への parent 指定による cycle
- locked / selectionLocked 等で parenting 編集不可の layer
- type 上の明確な禁止組み合わせがある場合

拒否時は state を変えず、layer panel の status 文言だけ更新する。

### 3.4 Undo

`SetLayerParentCommand` を追加し、

- before parent id
- after parent id

を保持する。1 undo で 1 parenting 変更を戻す。

### 3.5 解除導線

- pick-whip drag を空白へ release: no-op
- parent glyph の context menu に `Clear Parent`
- `Alt` 押下 drag release で unparent

解除専用の複雑な UI は増やさない。

---

## 4. 実装フェーズ

### Phase 1: Layer panel affordance

- `ArtifactLayerPanelWidget` の row に parent glyph を追加
- hover / pressed state を owner-draw で描画
- row hit test に glyph 領域を追加

**Done criteria:**
- 各 row に parent glyph が出る
- hover 時に狙える場所が分かる

### Phase 2: Drag preview and drop

- drag state を layer panel 内に保持
- source row から target row への preview line
- valid target row highlight
- drop で parent 設定

**Done criteria:**
- drag 中に接続先が視覚的に分かる
- drop で parent 設定が即反映される

### Phase 3: Undo and validation

- `SetLayerParentCommand`
- cycle / self-parent / locked validation
- invalid drop 時の status feedback

**Done criteria:**
- invalid parent が作れない
- 1 undo で直前の parent 変更を戻せる

### Phase 4: Inspector entry and clear-parent

- Inspector の parent 行にも同じ glyph を追加
- `Clear Parent` 導線追加

**Done criteria:**
- Timeline と Inspector のどちらからでも同じ操作で parent を作れる
- unparent が 1 手でできる

---

## 5. ガードレール

- 新規 global signal は増やさない
- `QPainter` 合成や QtCSS に逃げない
- row 操作は `ArtifactLayerPanelWidget` に閉じる
- viewport contract に parenting drag を混ぜない
- `ArtifactWidgets` は触らない

---

## 6. 優先度メモ

4 本の選定項目の中では、実装順は次が自然:

1. Auto-Orient
2. In/Out Slide
3. Parent Pick-Whip

`Keyframe Copy & Paste` は既に `docs/done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md` に完了記録があるため、新規実装対象からは外す。
