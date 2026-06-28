# MILESTONE ROADMAP CURRENT

**Date**: 2026-06-03  
**Scope**: `Artifact/`, `ArtifactCore/`, `docs/planned/`  
**Inputs**: `docs/done/COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md`, `docs/analysis/WIDGET_GAP_ANALYSIS_2026-06-03.md`, `docs/planned/MILESTONES_BACKLOG.md`, `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`

---

## 目的

この roadmap は、いまの `ArtifactStudio` で「次に何をやると一番前に進むか」を固定するための短い実行表です。

狙いは 3 つです。

- 既に終わっているものを再着手しない
- 依存が重いものと、すぐ切れるものを分ける
- 1 セッションで進める順番を迷わないようにする

---

## 現状メモ

2026-06-03 の分析で、次の点は少なくとも文書上は再確認できた。

- `ArtifactTimelineTrackPainterView` は right-side editing surface として扱うのが自然
- `TimelineTrackView` は legacy / compatibility 名として扱うほうが混乱が少ない
- `Motion Path overlay`、`Effect Hitbox View`、`Render Debounce`、`Pen Tool / Mask editing`、`Text box resize` は、少なくとも分析時点ではコード上に実装痕跡がある
- `Audio Waveform on Timeline` も analysis 上では実装済み扱いだったため、再着手よりは確認・整理候補として扱う

---

## Parallel Track: Critical Stability + Project Health

この 2 本は別々に進めるより、診断語彙を揃えながら並走させるほうが効率がよい。

- `Critical Render / Media Stability Program` は、particle / video の失敗を smoke gate に落とす役
- `Project Health / Problem View Wiring` は、load / save / render preflight の共通診断面を整える役。文書上は完了済みだが参照価値が高い
- 共通の出力語彙は `severity / category / message / description / fixHint` を軸にする
- `Debug Render Harness` は Critical Render 側の regression surface として使い、`Problem View` は同じ結果ソースを読む
- 先に観測性を固めることで、原因修正と UI 表示の両方を同じ判定基準で回せる

---

## Priority A: まず進めるべき未完了もの

### 1. Critical Render / Media Stability Program

Status:

- `M-CE-CRIT-1` is in progress
- `M-CE-CRIT-2` is an existing regression surface, not a new implementation target

対象:

- `docs/done/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`
- `docs/done/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md` (existing harness surface; use for capture/report and regression checks)
- `docs/planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`

理由:

- 「何も描画されない」系の不具合を先に潰すと、以後の作業の信頼性が上がる
- 最小再現と診断導線を固めると、他の UI 改善の検証コストも下がる

進め方:

1. 最小再現ケースを 1 つに絞る
2. capture / trace / report の出力語彙を揃える
3. 原因修正より先に観測性を固める

### 2. Continuation Sprint の小粒タスク

対象:

- `docs/planned/MILESTONE_CONTINUATION_SPRINT_2026-05-20.md`
- `docs/planned/MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md`

候補:

- `Composition Editor Mask / Roto Editing`

### 3. Command IR / Automation Foundation

対象:

- `docs/planned/MILESTONE_COMMAND_IR_AUTOMATION_FOUNDATION_2026-06-28.md`

理由:

- AI / MCP / Python の mutation 面を low-level API から切り離す土台になる
- 既存の `AI Command Sandbox` と `McpBridge` を command-oriented に整理しやすくなる
- 先に安全な Command IR を決めると、後続の DSL や macro も同じ経路に載せられる

進め方:

1. Primitive command vocabulary を固定する
2. validate / execute / undo の result shape を決める
3. resolver / executor / macro の責務を分ける

---

## Priority B: いまは再着手しない候補

以下は analysis 上で「実装済み」「ほぼ実装済み」「legacy 名称の混乱が主因」に寄っていたため、今は再実装より整理優先にする。

- `Motion Path overlay`
- `Effect Hitbox View`
- `Render Debounce`
- `Pen Tool / Mask editing`
- `Text box resize`
- `Audio Waveform on Timeline`

---

## Priority C: 依存が重いので後回し

- `MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md`
- `MILESTONE_PARTICLE_LAYER_3D_MIGRATION_2026-03-25.md`
- `MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md`
- `MILESTONE_3D_GIZMO_IMPLEMENTATION_2026-03-25.md`
- `Render Farm orchestration`
- `Render Queue checkpoint / retry`
- `Puppet Tool`
- `Auto-Orient`
- `Motion Sketch`

---

## Reference Only

以下は完了済みまたは文書上の参照枠なので、実作業の優先一覧からは外してある。

- `docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`
- `docs/done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md`
- `docs/done/MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md`
- `docs/done/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md`
- `docs/done/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- `docs/done/MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md`
- `docs/done/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`
- `docs/done/MILESTONE_TIMELINE_KEYFRAME_EDITING_PHASE1_EXECUTION_2026-05-12.md`

---

## Recommended Order

1. `Critical Render / Media Stability Program` と `Project Health / Problem View Wiring` を並走
2. `Debug Render Harness` の既存 surface を使って `Critical Render / Media Stability Program` を回す
3. `Command IR / Automation Foundation`
4. `Composition Editor Mask / Roto Editing`
5. `Continuation Sprint` の小粒タスク

---

## One-Session Default

もし「いまから 1 回の作業で前進させる」なら、`Composition Editor Mask / Roto Editing` か `Continuation Sprint` の小粒タスクへ進むのが自然です。

1. `docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md` を参照する
2. `docs/planned/MILESTONE_CONTINUATION_SPRINT_2026-05-20.md` の残り候補を確認する
3. `docs/planned/MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md` を参照する

---

## Notes

- この roadmap は、機能一覧ではなく「今どれから触るか」のためのもの
- 新しい大型機能を増やす前に、既存の診断と責務境界を揃えるほうが全体効率が高い
- 迷ったら `ArtifactTimelineTrackPainterView` を正規名として扱い、`TimelineTrackView` は legacy reference とみなす
