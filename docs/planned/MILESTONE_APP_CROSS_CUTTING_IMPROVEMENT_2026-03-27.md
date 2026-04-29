# マイルストーン: Application Cross-Cutting Improvement

> 2026-03-27 作成

## 目的

アプリ全体にまたがる改善を、個別機能の寄せ集めではなく、同じ基準で束ねて進める。

このマイルストーンは、`Feature Expansion` のような機能追加本体でも、`App UX & Core Refinement` のような操作感改善でもなく、その上位にある **横断整備のローリングアップデート枠** として扱う。

具体的には、メニュー、ツールバー、ショートカット、ビュー、選択状態、再生状態、表示状態、diagnostics、asset / project 導線を一つの app surface として揃える。

---

## Scope

- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- `Artifact/src/Widgets/Menu/*.cppm`
- `Artifact/src/Widgets/ArtifactToolBar.cppm`
- `Artifact/src/Widgets/ArtifactStatusBar.cpp`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/ArtifactAssetBrowser.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactContentsViewer.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## Non-Goals

- 単一機能の深掘りだけを目的にしない
- render backend の低レベル刷新をここで行わない
- core API の全面再設計をここで終わらせない

---

## Background

このリポジトリでは、機能追加が進むほど、同じ概念が別ウィジェットで別の名前や責務として現れやすい。

例:

- `File / Composition / Edit / View / Layer / Render / Help` の menu と app service の接続
- `ArtifactToolBar` の command surface 化
- `ApplicationSettingDialog` と app 本体の設定反映
- `Contents Viewer` と `Layer Solo View` と `Asset Browser` の inspection 見え方
- `Timeline` の selection / current layer / playback state の同期
- `Audio` の meter / clip / mute / solo の横断表示
- `Text` / `Mask` / `Keyframe` / `Camera` / `Precomp` の編集導線

これらを各マイルストーンの副作用として揃えるのではなく、**横断改善として共通基準を持つ** のがこの文書の役割。

---

## Phase 1: Command / Surface Unification

- 目的:
  - menu / toolbar / shortcut / context command を同じ app command surface に寄せる

- 作業項目:
  - `File / Composition / Edit / View / Layer / Render / Help` の action ownership を整理
  - `ArtifactMainWindow` から各 service への command routing を統一
  - enabled / disabled state を command state と同期
  - shortcut と menu の二重実装を減らす

- 完了条件:
  - 主要 action が app service の正本を持つ
  - menu と shortcut の挙動差が減る

## Phase 1 Status

- menu / toolbar / shortcut の一部はすでに command surface 化が進行中
- composition editor は viewport overlay / context menu / pie menu を Diligent 直描きへ寄せている
- camera / precomp / checkerboard / grid のような surface state は app settings か controller に寄せる流れが始まっている

## Phase 2: Cross-View State Sync

- 目的:
  - 複数ビューの selection / current / active / solo / lock を共通基準に揃える

- 作業項目:
  - timeline / asset browser / contents viewer / layer solo view の selection 同期
  - `current composition` / `current layer` / `active layer` の整理
  - playback state と view state の見え方を統一
  - missing / unloaded / muted / clipped などの state presentation を揃える

- 完了条件:
  - どのビューでも「今何を見ているか」が分かる
  - 状態名がウィジェットごとにバラけない

## Phase 2 Status

- selection / current composition / active camera / precomp navigation の見え方が少しずつ揃ってきている
- nested composition や viewport overlay の表示導線は実装を進めやすい状態
- ここからは「見える化」だけでなく「どの panel が正本か」を固定するのが主眼

## Phase 3: Diagnostics / Feedback Consistency

- 目的:
  - エラー、警告、可視状態、負荷状態を共通の見え方にする

- 作業項目:
  - status bar / console / inline chips / badges の役割分担
  - audio clip / render failure / missing asset / decode failure の表現整理
  - performance hint / cache hit / fallback path の共通ログ方針
  - 進行中タスクの feedback を操作元に返す

- 完了条件:
  - 異常時の見え方が widget ごとに不一致にならない
  - console / status / inline 表示の役割が整理される

## Phase 3 Status

- frame debug / diagnostics surface は拡張中
- render / playback / decode の異常を同じ語彙で扱う準備ができつつある
- ここは後回しにもなりやすいが、app 全体の連携が進むほど効いてくる

## Phase 4: Workflow Bridges

- 目的:
  - 一つの操作が project / asset / timeline / render / playback に自然に橋渡しされるようにする

- 作業項目:
  - Asset Browser から layer / composition / render への導線
  - Contents Viewer から source / final / compare への導線
  - Timeline から keyframe / layer group / audio state への導線
  - Render queue から output preset / diagnostics / retry への導線

- 完了条件:
  - 単体機能ではなくワークフローとして操作できる

## Phase 4 Status

- asset / project / composition / timeline の導線はすでに部分的に接続済み
- precompose や camera / nested comp の導線は workflow bridge の一部として扱える
- この phase は「機能追加の最後」ではなく、横断導線を埋める継続タスクとして扱う

---

## Current Working Set

この時点で実際に進んでいる横断改善のまとまりは次の通り。

- composition editor viewport
  - command palette / context menu / pie menu の viewport overlay 化
  - camera frustum の表示切替
  - selection overlay / ghost overlay / snap guide の統合
  - Maya 風の viewport navigation 追加
- timeline / layer surface
  - `Precomp Layer` の見え方整理
  - nested composition への遷移導線
  - timeline row badge / selection state の整合
- app settings / shared view state
  - checkerboard size の app setting 化
  - composition grid settings の app setting 化
  - editor 起動時の settings 適用
- command routing / surface ownership
  - menu / toolbar / shortcut / viewport command の責務を揃える途中
  - service 経由の state ownership に寄せる途中

この作業群は、単独機能として完了させるよりも、アプリ全体の state と surface を少しずつ揃える rolling update として進める。

---

## Rolling Update Focus

このマイルストーンは一気に終わらせるものではなく、次の順で少しずつ更新する。

1. command / surface の ownership を揃える
2. menu / toolbar / shortcut / settings の state source を一本化する
3. cross-view の selection / current / active state を揃える
4. diagnostics と workflow bridge を足して、迷いにくい app surface にする

## Next Slice Suggestions

次に切りやすいのは次の順。

1. `Menu / Toolbar / Shortcut` の command ownership をさらに揃える
2. `Composition editor` の overlay と `timeline` の selected/current state を同期する
3. `Precomp / Camera / Grid / Checkerboard` の view state を settings 側へ整理する
4. `Diagnostics` と `workflow bridge` を app surface に戻す

---

## Recommended Order

1. Phase 1: Command / Surface Unification
2. Phase 2: Cross-View State Sync
3. Phase 3: Diagnostics / Feedback Consistency
4. Phase 4: Workflow Bridges

---

## Related Milestones

- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `Artifact/docs/MILESTONE_APP_UX_AND_CORE_REFINEMENT_2026-03-17.md`
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`
- `docs/planned/MILESTONE_APPLICATION_SETTINGS_APP_INTEGRATION_2026-04-19.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md`
- `docs/planned/MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md`
- `docs/planned/MILESTONE_OPERATION_FEEL_REFINEMENT_2026-03-25.md`
