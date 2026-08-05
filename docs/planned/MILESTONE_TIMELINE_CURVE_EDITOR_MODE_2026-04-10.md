> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md](MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md)

# Timeline Curve Editor Mode

## Goal

Add a curve editor mode to `ArtifactTimelineWidget` so the same surface can switch between:

- the normal timeline / layer orchestration view
- a curve-focused editor for animated properties and keyframes

The intended trigger is `Tab` while the timeline has focus.
`U` is reserved for flat keyframe filtering.

## UX Intent

- keep the playhead, selection, and zoom context when switching modes
- preserve the user's current time position
- make the transition feel like a mode change, not a new window
- keep timeline editing available as the default path
- show curve editing only when the current selection can actually expose animated properties
- do not overload `U` for mode switching; that key belongs to flat visibility filtering

## Scope

- mode state and toggle routing inside `ArtifactTimelineWidget`
- a curve editor surface that reuses the current time context
- selection-aware property curve presentation
- keyboard focus and shortcut handling for `Tab`
- undo-friendly curve edits for keyframe navigation and value changes
- documentation alignment with `docs/WIDGET_MAP.md`

## Phases

### Phase 1

- define the mode state and toggle entry points
- wire keyboard handling for `Tab`
- preserve playhead and selection when switching modes

### Phase 2

- introduce the curve editor surface and basic curve rendering
- connect selected animated properties to visible curves
- add curve navigation, zoom, and pan behavior

### Phase 3

- integrate curve edits with the existing keyframe / property system
- add regression tests for shortcut routing and state preservation
- expose the mode to AI-assisted workflows if useful

---

## 2026-07-25 現状確認

Phase 1〜3 の主要実装を確認した。`ArtifactTimelineWidget` は `QStackedWidget` の timeline painter page と curve editor page を持ち、Tab／グローバルスイッチで切り替え、playhead・選択・現在のプロパティ文脈を共有する。Curve Editor には Value / Speed グラフ切替、ズーム／パン、Bezier 描画、キー移動・削除・挿入、複数選択、数値入力があり、Timeline widget 側の snapshot command 経由でプロパティ書き戻しと Undo/Redo を行う。表示モードは `timelineGraphEditorMode` 等の設定へ保存される。

未完了・未確認:

- Speed グラフ編集の書き戻し（現状は read-only）
- Tab 切替時の全選択・playhead・scroll 文脈の runtime 回帰
- shortcut routing と複数キー編集の実機確認
- AI workflow への専用公開導線

したがって「Curve Editor mode と Value 編集は実装済み、Speed 編集と runtime 回帰確認が残る」と整理する。

