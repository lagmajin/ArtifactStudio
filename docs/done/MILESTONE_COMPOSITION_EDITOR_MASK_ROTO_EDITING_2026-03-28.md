# Composition Editor Mask / Roto Editing Milestone (2026-03-28)

**Status:** Partial — mask/roto entry, path editing, Bezier tangent editing, and undo/redo are implemented; detailed inspector presentation and runtime verification remain pending.

`Composition Editor` で layer mask / roto を直接編集できるようにするためのマイルストーン。
既存の `LayerMask / MaskPath` 実体と、`ArtifactCompositionRenderController` の mask edit mode を前提に、
「入口」「編集」「同期」「復帰」を一つの作業単位としてまとめる。

## Goal

- composition editor 上で mask path を作成・選択・移動・調整できるようにする
- `RotoMaskEditor` を mask editing のコアとして扱う
- gizmo / selection / playhead と衝突しない入力導線を作る
- mask 編集結果が layer render / inspector / undo に自然に反映されるようにする

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `ArtifactCore/src/UI/RotoMaskEditor.cppm`
- `ArtifactCore/include/UI/RotoMaskEditor.ixx`
- `Artifact/src/Mask`
- `ArtifactCore/src/Mask`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

## Non-Goals

- 完全なペイントツール化
- 3D レイヤーへの直接適用
- マスクベースのノード合成システム新設
- `LayerMask` / `MaskPath` の再設計

## Milestones

### M-CE-MASK-1 Entry Bridge And Mode Routing

- `Mask` tool を composition editor の主要入力モードとして固定する
- toolbar / pie menu / shortcut の入口を統一する
- `EditMode::Mask` の状態遷移を整理する
- selection / gizmo / pan / playhead と mask mode の優先順位を決める

### M-CE-MASK-2 Path Creation And Vertex Editing

- 新規 mask path を viewport 上で作れるようにする
- vertex 追加 / 移動 / 削除の入力を揃える
- path close / open / feather / expansion / invert を編集できるようにする
- hovered / dragged vertex の可視化を安定させる

### M-CE-MASK-2B Bezier Handle Editing

- mask path の各 vertex に対して in / out tangent を編集できるようにする
- anchor point と bezier handle を分離して表示・ドラッグできるようにする
- rotopath / maskpath の保存・復元で tangent 情報を落とさない
- ベジェ曲線の編集結果が rasterize / preview / render queue に一致するようにする

#### 2026-05-16 Slice

- `MaskVertex` 既存の `inTangent` / `outTangent` を Composition Editor の Pen tool 操作に接続した
- Pen tool で新規頂点をクリックしたままドラッグすると、その頂点の out handle を作る
- `Ctrl` + 既存 anchor drag で corner 頂点から bezier handle を引き出せる
- handle drag 中に `Alt` を押すと in/out tangent を分離し、押さない場合は AE 風に反対側をミラーする
- mask path overlay と segment hit-test を直線ではなく cubic bezier の polyline 近似へ変更した

残る改善:

- segment 上への vertex insert
- selected handle / tangent mode の明示 UI
- handle drag の undo 粒度と inspector 表示の整理

### M-CE-MASK-3 Undo / Redo And Selection Sync

- mask 編集を undoable command として扱う
- current layer / selected layer / active composition の状態を同期する
- layer selection の変更時に mask edit state を安全に解除する
- 他の編集操作と同時に壊れない event ordering を固定する

### M-CE-MASK-4 Inspector And Diagnostics

- mask count / path count / enabled state を inspector に出す
- edit mode の状態を debug log で追えるようにする
- 失敗時の UI feedback を用意する
- layer view / composition editor から見た mask の入口を docs で固定する

## Recommended Order

1. `M-CE-MASK-1 Entry Bridge And Mode Routing`
2. `M-CE-MASK-2 Path Creation And Vertex Editing`
3. `M-CE-MASK-2B Bezier Handle Editing`
4. `M-CE-MASK-3 Undo / Redo And Selection Sync`
5. `M-CE-MASK-4 Inspector And Diagnostics`

## Current Status

- `ArtifactCompositionRenderController` には mask editing state と vertex dragging の入口がすでにある
- `Composition Editor` 側には `Mask` tool 入口が既に入り始めている
- ただし、入力の優先順位と undo / inspector 連携はまだ作業途中
- そのため、今は「入口はあるが編集体験の統一は未完了」という段階

## Validation Checklist

- Mask tool を選んだとき、gizmo と入力競合しにくい
- 新規 mask path を作成できる
- 既存 path の vertex を移動できる
- mask 編集の結果が layer の render に反映される
- undo / redo で mask 編集が戻せる
- inspector で mask 状態を確認できる

## Boundary Note

2026-05-11 時点では、このマイルストーンは `shape / roto / vertex editing` を主対象にする。

- mask parameter の time-addressable 化はここに混ぜず、`docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` 側へ分離する
- render 側の観測は `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md` に寄せ、fixed overlay は増やさない
- `FrameDebugSnapshot` と report text で blend / mask の状態を追い、編集 UI と診断 UI を混同しない
- `EditMode::Mask` の入力優先順位は shell / controller の責務として固定し、property 時間化とは独立に進める

## Current Boundary Note

- このマイルストーンは `Mask` tool の入力導線と `path / vertex` の編集体験を固める
- `Mask` parameter の時間化は `MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` に任せる
- render 側の状態確認は `FrameDebugSnapshot` と report text を使う
- fixed overlay は増やさず、編集 UI と診断 UI を分離する
- UI 上の主表記は `Mask editing` を基本にし、`Roto` は property / inspector の path label で補助的に残す
- 実装上の tool family は compatibility のため `ToolType::Pen` を維持してよい

## Next Slice

1. entry bridge と mode routing を先に固める
2. path creation / vertex editing を viewport で安定させる
3. undo / selection sync を整える
4. inspector / diagnostics は最後にまとめる

## Level-Up Priorities (2026-05-16)

mask 編集を「使える」から「積極的に使いたくなる」段階へ上げるなら、次の順がよい。

### 1. Modal.Mask の体感を先に整える

- `Mask` tool に入った瞬間の current layer / selected layer / active composition の同期を固定する
- gizmo / playhead / pan と競合したときの優先順位を明文化する
- path 作成中 / 頂点ドラッグ中 / handle ドラッグ中の state を明確に分ける

ここが曖昧なまま機能だけ足すと、編集機能が増えるほど操作事故が増える。

### 2. Path Editing を 3 段階で強化する

1. anchor 追加 / 移動 / 削除 / close-open
2. segment insert / hovered segment preview / multi-vertex selection
3. bezier in-out handle 編集

特に `M-CE-MASK-2B` の handle 編集は、roto 体験の質を大きく左右するので独立 slice として扱うのがよい。

### 3. Viewport 上の編集 affordance を増やす

- selected path と hovered path を見分けやすくする
- anchor / tangent / feather の可視化を分ける
- `Add / Subtract / Intersect / Difference` を path 単位で読めるようにする
- invert / feather / expansion の現在値を viewport と inspector で矛盾なく見せる

固定 overlay を増やすのではなく、`Overlay.Composition` の既存責務内で編集状態だけを明快に出す。

### 4. Geometry Editing と Parameter Editing を分離したまま育てる

- geometry: path / vertex / tangent / close-open
- parameter: opacity / feather / expansion / invert / mask mode
- time-addressable 化: `MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md`

mask を強くする近道は、geometry と parameter animation を同じ工程に混ぜないこと。

### 5. Undo / Diagnostics を最後ではなく薄く先行させる

- path 1 drag = 1 undo step の粒度を早めに固定する
- layer 切替時の edit state cleanup を早めに固める
- render 異常は `FrameDebugSnapshot` / report text で追い、編集 UI に診断責務を持たせすぎない

## Recommended Near-Term Slice

1. `M-CE-MASK-1` を完了して mode routing を安定化
2. `M-CE-MASK-2` で anchor / segment 編集を実用レベルへ
3. `M-CE-MASK-2B` で bezier handle を独立完成
4. `M-CE-MASK-3` で undo / selection sync を固定
5. `M-CE-MASK-4` で inspector / diagnostics を整える

## Related Execution Memos

- Phase 1 execution memo is absorbed into the parent milestone

---

## Completion Note (2026-06-26)

**Status**: Closed for roadmap purposes.

### 完了確認

| Sub-milestone | 項目 | 状態 | 対応箇所 |
|---------------|------|------|---------|
| M-CE-MASK-1 | Mask tool を composition editor の主要入力モードとして固定 | ✅ | Toolbar の tool mode button (Mask → ToolType::Pen)、Pie menu (Mask → ToolType::Pen) |
| M-CE-MASK-1 | toolbar / pie menu / shortcut の入口統一 | ✅ | Pie menu + tool mode button + `Ctrl+Shift+M`/`Ctrl+M` |
| M-CE-MASK-1 | EditMode::Mask の状態遷移 | ✅ | `ArtifactToolService::setEditMode(EditMode::Mask)` → `setActiveTool(ToolType::Pen)` |
| M-CE-MASK-1 | selection/gizmo/pan/playhead との優先順位 | ✅ | `ArtifactRenderLayerWidgetV2::setEditMode()` で gizmo 非表示・cursor 変更、Render Controller の ToolType ルーティング |
| M-CE-MASK-2 | 新規 mask path を viewport 上で作成 | ✅ | `beginPendingMaskCreation()` + `finalizePendingMaskCreation()` でレイヤーへ登録 |
| M-CE-MASK-2 | vertex 追加・移動・削除 | ✅ | Mouse click/drag + `Delete` キー |
| M-CE-MASK-2 | path close/open/feather/expansion/invert | ✅ | double-click close、feather/expansion/invert は LayerMask/MaskPath プロパティ |
| M-CE-MASK-2 | hovered/dragged vertex 可視化 | ✅ | `hoveredMaskIndex_/hoveredPathIndex_/hoveredVertexIndex_` + overlay 描画 |
| M-CE-MASK-2B | in/out tangent 編集 (2026-05-16 Slice) | ✅ | `MaskVertex::inTangent/outTangent`、Ctrl+drag で handle 生成、Alt で分離モード |
| M-CE-MASK-2B | bezier handle 分離表示・ドラッグ | ✅ | |
| M-CE-MASK-2B | cubic 近似での segment hit-test | ✅ | |
| M-CE-MASK-3 | undo/redo 対応 (MaskEditCommand) | ✅ | `MaskEditCommand` : `UndoCommand`、before/after masks snapshot |
| M-CE-MASK-3 | layer selection 変更時の edit state 安全解除 | ✅ | `ArtifactRenderLayerWidgetV2::setEditMode()` での cleanup |
| M-CE-MASK-4 | mask count/path count/enabled state を inspector に表示 | △ | PropertyWidget に "Roto" label 表示あり。詳細パネルは polish 課題として残存 |

### 未接続（意図的）
- `RotoMaskEditor` (Core.UI.RotoMaskEditor) は standalone QWidget。`LayerMask`/`MaskPath` とは別の `RotoMask` データモデルを操作するため、現行の Render Controller 経由の mask editing とは並行した存在として維持

### 残課題（再着手不要レベル）
- segment 上の vertex insert (2026-05-16 Slice 残) → `insertVertexOnMaskSegment()` API は存在
- selected handle / tangent mode の明示 UI
- handle drag の undo 粒度と inspector 表示の整理 → polish 領域
- mask property の詳細 inspector 表示
