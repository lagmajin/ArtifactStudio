# M-SC-2 Shortcut Context Map / Blender-Like Keymap Routing
**作成日:** 2026-04-21  
**目的:** Blender 風の「場所とモードで意味が変わる」ショートカット構造を、Artifact の主要 surface に対して明文化し、`InputOperator` の widget-specific keymap と preset system の上に載せる。

## 背景

現在の `ArtifactCore::InputOperator` は、Blender 風の operator/keymap に近い基盤を持っている。
`KeyMap` は context を持てるし、widget-specific keymap も登録できる。
しかし、実際にどの widget / region / mode を独立 keymap として切るかの約束がまだ曖昧だと、Blender っぽい柔軟性を UI 側で活かしにくい。

この milestone では、**context 解決の優先順位** と **widget / region 単位の分割表** を固定する。

## 設計方針

### 1. 解決優先順位

1. `Modal`
2. `Widget.Mode`
3. `Widget`
4. `Workspace`
5. `Global`

### 2. Context 命名規約

- `Global`
- `Workspace.Composition`
- `Workspace.Timeline`
- `Workspace.Project`
- `Viewport.Composition`
- `Panel.Timeline.Left`
- `Panel.Timeline.Right`
- `Panel.LayerTree`
- `Panel.AssetBrowser`
- `Panel.Inspector`
- `Modal.Transform`
- `Modal.Scrub`
- `Dialog.ShortcutEditor`

### 3. Widget 単位の分割

- `ArtifactCompositionRenderWidget`
- `ArtifactCompositionEditor`
- `ArtifactTimelineWidget`
- `ArtifactLayerPanelWidget`
- `ArtifactAssetBrowser`
- `ArtifactProjectManagerWidget`
- `ArtifactInspectorWidget`
- `ArtifactPlaybackShortcuts`

## Concrete Region Map

### Composition

- `ArtifactCompositionEditor`
  - `Workspace.Composition`
  - `Viewport.Composition`
  - `Overlay.Composition`
  - `Modal.Transform`
  - `Modal.Mask`
  - `Modal.Pen`
  - `Modal.PlaybackScrub`
  - `Modal.ViewportNavigate`
- `ArtifactCompositionRenderWidget`
  - `Viewport.Composition`
  - `Overlay.Composition`
  - `Modal.Transform`

### Composition Region Split

- `ArtifactCompositionEditor`
  - editor shell / playback controls / surface orchestration / toolbar / transport
- `ArtifactCompositionRenderWidget`
  - viewport drawing / overlay drawing / direct manipulation surface
- `ArtifactContentsViewer`
  - compare / inspect / presentation surface
- `ArtifactCompositionAudioMixerWidget`
  - audio lane surface / meter / gain / mute

### Composition Subregions

- `Viewport.Composition`
  - select / move / rotate / scale / fit / pan / zoom
- `Overlay.Composition`
  - guides / HUD / snap hints / transform handles
- `Modal.Transform`
  - drag-based transform session
- `Modal.Mask`
  - mask edit session
- `Modal.Pen`
  - roto / pen edit session
- `Modal.PlaybackScrub`
  - scrub during playback / frame stepping
- `Modal.ViewportNavigate`
  - pan / zoom / camera-like navigation
- `Panel.Composition.Toolbar`
  - command buttons / modes / quick access
- `Panel.Composition.Transport`
  - play / pause / stop / frame step
- `Panel.Composition.Audio`
  - audio mixer / mute / solo / gain

### Composition Representative Bindings

### Workspace.Composition

- `Space` - Play / Pause
- `Home` - Jump to start
- `End` - Jump to end
- `Alt+Shift+S` - Solo selected layer
- `M` - Toggle mask context entry
- `Alt+M` - Toggle matte context entry

### Viewport.Composition

- `G` - Grab / Move
- `R` - Rotate
- `S` - Scale
- `F` - Fit view
- `1` - Zoom 100%
- `Alt+MouseDrag` - Pan / zoom

### Overlay.Composition

- `Shift+Space` - Toggle HUD
- `Ctrl+Alt+G` - Toggle guides
- `Ctrl+Alt+S` - Toggle snap hints
- `Tab` - Cycle overlay focus

### Panel.Composition.Toolbar

- `Ctrl+1` - Switch to selection mode
- `Ctrl+2` - Switch to transform mode
- `Ctrl+3` - Switch to mask mode
- `Ctrl+4` - Switch to pen mode

### Panel.Composition.Transport

- `Space` - Play / Pause
- `J` - Reverse play
- `K` - Stop / hold
- `L` - Play forward

### Panel.Composition.Audio

- `M` - Toggle mute
- `S` - Toggle solo
- `Up/Down` - Gain adjust

### Timeline

- `ArtifactTimelineWidget`
  - `Workspace.Timeline`
  - `Panel.Timeline.Left`
  - `Panel.Timeline.Right`
  - `Modal.Scrub`
  - `Modal.KeyframeEdit`
- `ArtifactLayerPanelWidget`
  - `Panel.Timeline.Left`
  - `Panel.LayerTree`
  - `Modal.Mask`
  - `Modal.Matte`
- `ArtifactTimelineNavigatorWidget`
  - `Panel.Timeline.Navigator`
  - `Modal.NavigatorDrag`
- `ArtifactTimelineScrubBar`
  - `Panel.Timeline.Scrub`
  - `Modal.Scrub`
- `ArtifactWorkAreaControlWidget`
  - `Panel.Timeline.WorkArea`
  - `Modal.WorkAreaDrag`
- `TimelineTrackView`
  - `Panel.Timeline.Right`
  - `Workspace.Timeline`
  - `Modal.KeyframeEdit`

### Timeline Region Split

- `ArtifactTimelineWidget`
  - orchestration / context router / playhead sync
- `ArtifactLayerPanelWidget`
  - left tree / row operations / hide / shy / mask / matte
- `ArtifactTimelineNavigatorWidget`
  - visible range / zoom window / scrub viewport navigation
- `ArtifactTimelineScrubBar`
  - RAM preview cache range / cache occupancy
- `ArtifactWorkAreaControlWidget`
  - in/out range editing / work area span
- `TimelineTrackView`
  - track content / keyframe lane / right-side editing surface

### Project / Asset / Inspector

- `ArtifactAssetBrowser`
  - `Panel.AssetBrowser`
  - `Modal.Rename`
  - `Modal.Import`
- `ArtifactProjectManagerWidget`
  - `Workspace.Project`
  - `Panel.ProjectTree`
- `ArtifactInspectorWidget`
  - `Panel.Inspector`
  - `Modal.PropertyEdit`
  - `Modal.EffectEdit`

### Playback

- `ArtifactPlaybackShortcuts`
  - `Workspace.Playback`
  - `Modal.PlaybackScrub`

## Representative Bindings

### Global

- `Ctrl+Z` - Undo
- `Ctrl+Shift+Z` - Redo
- `Ctrl+S` - Save
- `Ctrl+Shift+S` - Save As
- `Ctrl+O` - Open
- `Ctrl+P` - Preferences / Settings

### Workspace.Composition

- `Space` - Play / Pause
- `Home` - Jump to start
- `End` - Jump to end
- `Alt+Shift+S` - Solo selected layer
- `M` - Toggle mask context entry
- `Alt+M` - Toggle matte context entry

### Viewport.Composition

- `G` - Grab / Move
- `R` - Rotate
- `S` - Scale
- `F` - Fit view
- `1` - Zoom 100%
- `Alt+MouseDrag` - Pan / zoom

### Workspace.Timeline

- `Space` - Play / Pause
- `J` - Reverse play
- `K` - Stop / hold
- `L` - Play forward
- `I` - Set in point
- `O` - Set out point
- `Ctrl+D` - Duplicate selected keyframe / clip

### Panel.Timeline.Left

- `Delete` - Delete selected layer / row
- `F2` - Rename
- `H` - Toggle shy
- `M` - Open masks section
- `Alt+M` - Open mattes section
- `Ctrl+M` - Add marker / metadata action

### Panel.AssetBrowser

- `Enter` - Open selected asset
- `F2` - Rename
- `Delete` - Delete
- `Ctrl+N` - New folder
- `Ctrl+F` - Search
- `Alt+Left` / `Alt+Right` - Navigate folder history

### Panel.Inspector

- `Ctrl+F` - Search property
- `Tab` - Next editable field
- `Shift+Tab` - Previous editable field
- `Delete` - Reset / clear selected property
- `Ctrl+D` - Duplicate effect / parameter block

### Modal.Transform

- `Esc` - Cancel
- `Enter` - Commit
- `X/Y/Z` - Axis constraint
- `Shift` - Fine adjustment

### Modal.Scrub

- `Esc` - Cancel scrub
- `Enter` - Commit scrub
- `Left` / `Right` - Step frame
- `Shift+Left` / `Shift+Right` - Larger step

## Phase

### Phase 1: Context Model Freeze
- `InputOperator` の context 解決順を文書化する
- `KeyMap::context()` の命名規約を固定する
- `Global / Workspace / Widget / Modal` の優先順位を明示する

### Phase 2: Widget / Region Registration
- 主要 widget を region 単位で keymap 登録できるようにする
- `ArtifactCompositionRenderWidget` と `ArtifactTimelineWidget` を最優先で登録する
- `ArtifactLayerPanelWidget` / `ArtifactAssetBrowser` / `ArtifactInspectorWidget` を続ける
- `ArtifactTimelineNavigatorWidget` / `ArtifactTimelineScrubBar` / `ArtifactWorkAreaControlWidget` / `TimelineTrackView` を timeline subregion として分離する

### Phase 3: Preset / Editor Integration
- `Blender` / `Default` / `Custom` / `Workspace` の preset を context 単位で保存できるようにする
- `ArtifactShortcutEditorDialog` から context 別の keymap を編集できるようにする
- 競合表示と revert を用意する

### Phase 4: Shortcut Surface / UX Polish
- `ArtifactShortcutEditorDialog` の画面構成を確定する
- `Global / Workspace / Widget / Modal` の切り替えを UI 上で追えるようにする
- 代表ショートカットの検索、カテゴリ絞り込み、競合警告の見せ方を固定する
- `WIDGET_MAP` と shortcut editor の表示名を同期する

## 実装順

1. `ArtifactCompositionRenderWidget`
2. `ArtifactTimelineWidget`
3. `ArtifactLayerPanelWidget`
4. `ArtifactAssetBrowser`
5. `ArtifactInspectorWidget`
6. `ArtifactPlaybackShortcuts`
7. `ArtifactShortcutEditorDialog`

## Shortcut Editor Surface

### Left

- preset list
- context tree
- category filter
- conflict filter

### Center

- action list
- shortcut table
- binding editor
- reset / revert controls

### Right

- selected shortcut detail
- current context info
- overlap / conflict hints
- usage notes

## 期待する効果

- 同じキーでも、場所とモードで意味を変えられる
- `G / R / S` や `Delete` が widget ごとに違う意味を持てる
- Blender 風の「操作は画面ではなく context に属する」感覚を再現しやすくなる

## Risks

- context 名が増えすぎると UI が見づらくなる
- widget と region の責務が曖昧だと preset が散らばる
- `Global` に逃がしすぎると Blender っぽさが薄れる

## 参照

- `ArtifactCore/include/UI/InputOperator.ixx`
- `ArtifactCore/src/UI/InputOperator.cppm`
- `ArtifactCore/include/UI/ShortcutBindings.ixx`
- `ArtifactCore/src/UI/ShortcutBindings.cppm`
- `docs/technical/BLENDER_STYLE_SHORTCUT_SYSTEM_ANALYSIS_2026-03-28.md`
- `docs/planned/MILESTONE_SHORTCUT_CUSTOMIZATION_2026-04-10.md`
