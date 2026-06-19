# Continuation Sprint / 2026-05-20

**Date**: 2026-05-20
**Status**: Planned
**Window**: Next 1-2 weeks

このマイルストーンは、いまの backlog が広がりすぎて再開点が見えにくい問題を解消するための短期実装束。

目的は、次の作業を `app validation -> timeline readability -> editor mode stability` の順で固定し、セッションごとに迷わず着手できるようにすること。

---

## Sprint Goal

次の 3 本を、依存が少なく実利が高い順に進める。

1. `Project Health / Problem View Wiring` を先に通す
2. `Timeline Keyframe Editing` の見え方を改善する
3. `Composition Editor Mask / Roto Editing` の入口と mode routing を安定させる

---

## Why This Sprint

- `Project Health` は他作業の前に安全な validation loop を作れる
- `Timeline Keyframe Editing` は日常操作の分かりづらさを直接減らせる
- `Mask / Roto` は深掘り前に entry bridge と state sync を整える価値が高い
- `Cloud AI Phase 6 / 7` は既実装のため、この sprint の新規実装対象から外す

---

## Execution Order

### 1. Project Health / Problem View Wiring

**Source**: [`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](./MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)

- `DiagnosticEngine` と `ProjectDiagnostic` の result path を app 側で統一する
- load / save / render preflight で同じ validation result を使う
- `ArtifactProblemViewWidget` と `ArtifactProjectHealthDashboard` の source を揃える

**Done when**
- 問題検出の入口が 1 本に寄る
- render preflight で error blocking が機能する
- dashboard と problem view が食い違わない

### 2. Timeline Keyframe Editing

**Source**: [`MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`](./MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)

- 選択レイヤーの keyframe を timeline 上で読める状態にする
- lane emphasis と marker visibility を先に整える
- header / summary は後回しにして、まず右ペインの可読性を優先する

**Done when**
- keyframe が埋もれず視認できる
- selection と timeline 表示の対応が分かる
- 追加 UI より先に既存面で編集判断ができる

### 3. Composition Editor Mask / Roto Editing

**Source**: [`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)

- `Mask` tool entry を composition editor 側で安定して開けるようにする
- mode routing を `Modal.Mask` / `Modal.Pen` の責務に沿って整理する
- selection / overlay / edit session の state sync を壊れにくくする

**Done when**
- mask tool に入れる
- mode 切り替えで state が壊れない
- 深い path editing へ進める入口が安定する

---

## Out of Scope

- `Mask / Roto` の深い path editing と full authoring
- graph editor の本格導入
- render backend の大規模再設計
- `DiligentEngine` / DX12 低レベル変更の横展開

---

## First Files

1. `ArtifactCore/include/Diagnostics/DiagnosticEngine.ixx`
2. `ArtifactCore/include/Diagnostics/ProjectDiagnostic.ixx`
3. `Artifact/src/Widgets/ArtifactProblemViewWidget.cppm`
4. `Artifact/src/Widgets/ArtifactProjectHealthDashboard.cppm`
5. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
6. `Artifact/src/Widgets/Timeline/*`
7. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
8. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

---

## Stop Conditions

次の状態に入ったら、この sprint では止めて別 milestone へ分岐する。

- timeline keyframe 表示に widget 責務の再分割が必要になった
- mask / roto entry が render controller の責務再分割まで要求し始めた
- editor mode routing が広い入力系再設計を要求し始めた

---

## Follow-up Milestones

- [`MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md`](./MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md)
- [`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](./MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)
- [`MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`](./MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
