# MILESTONE ROADMAP CURRENT

**Date**: 2026-06-03  
**Scope**: `Artifact/`, `ArtifactCore/`, `docs/planned/`  
**Inputs**: `docs/analysis/COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md`, `docs/analysis/WIDGET_GAP_ANALYSIS_2026-06-03.md`, `docs/planned/MILESTONES_BACKLOG.md`, `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`

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
- `Project Health / Problem View Wiring` は、load / save / render preflight の共通診断面を整える役
- 共通の出力語彙は `severity / category / message / description / fixHint` を軸にする
- `Debug Render Harness` は Critical Render 側の regression surface として使い、`Problem View` は同じ結果ソースを読む
- 先に観測性を固めることで、原因修正と UI 表示の両方を同じ判定基準で回せる

---

## Priority A: まず進めるべきもの

### 1. Critical Render / Media Stability Program

Status:

- `M-CE-CRIT-1` is in progress
- `M-CE-CRIT-2` is an existing regression surface, not a new implementation target

対象:

- `docs/planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`
- `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md` (existing harness surface; use for capture/report and regression checks)
- `docs/planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`

理由:

- 「何も描画されない」系の不具合を先に潰すと、以後の作業の信頼性が上がる
- 最小再現と診断導線を固めると、他の UI 改善の検証コストも下がる

進め方:

1. 最小再現ケースを 1 つに絞る
2. capture / trace / report の出力語彙を揃える
3. 原因修正より先に観測性を固める

### 2. Project Health / Problem View Wiring

対象:

- `docs/planned/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md`
- `docs/planned/MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md`
- `docs/planned/MILESTONE_CONTINUATION_SPRINT_2026-05-20.md`

理由:

- app 側の診断を実務で使える形に寄せると、今後の回帰確認が速くなる
- `diagnostic` と `problem view` の見え方を揃えるのは、重い機能追加よりも効果が高い

運用メモ:

- この項目は `Critical Render / Media Stability Program` と並走させる
- render preflight の gate と Problem View の source を揃えることで、同じ failure を別 surface で同じ言葉で読めるようにする

### 3. Timeline Right Pane Keyframe Edit Refinement

対象:

- `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_PHASE1_EXECUTION_2026-05-12.md`

理由:

- right-pane の state readability は、制作時の迷いを直接減らす
- 既存の action / model / property path を揃えるだけで体験差が出る

---

## Priority B: 1 セッションで切りやすいもの

### 4. Visual Density Monitor

対象:

- `docs/planned/MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md`
- `docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`
- `docs/planned/MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md`

理由:

- 既存の debug / overlay / app debugger surface に乗せやすい
- 「詰まりすぎている画面」を定量化する案は、設計上の価値が高い

### 5. Timeline Audio Waveform

対象:

- `docs/planned/MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md`

扱い:

- analysis 上では実装済み寄りだったため、優先度は下げる
- 着手するなら「未接続の差分確認」か「ドキュメント更新」のほうが速い

### 6. Continuation Sprint の小粒タスク

対象:

- `docs/planned/MILESTONE_CONTINUATION_SPRINT_2026-05-20.md`
- `docs/planned/MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md`

候補:

- `Project Health / Problem View Wiring`
- `Timeline Keyframe Editing`
- `Composition Editor Mask / Roto Editing`

---

## Priority C: いまは再着手しない候補

以下は analysis 上で「実装済み」「ほぼ実装済み」「legacy 名称の混乱が主因」に寄っていたため、今は再実装より整理優先にする。

- `Motion Path overlay`
- `Effect Hitbox View`
- `Render Debounce`
- `Pen Tool / Mask editing`
- `Text box resize`
- `Audio Waveform on Timeline`

---

## 依存が重いので後回し

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

## Recommended Order

1. `Critical Render / Media Stability Program` と `Project Health / Problem View Wiring` を並走
2. `Debug Render Harness` の既存 surface を使って `Critical Render / Media Stability Program` を回す
3. `Timeline Right Pane Keyframe Edit Refinement`
4. `Visual Density Monitor`

---

## One-Session Default

もし「いまから 1 回の作業で前進させる」なら、次の順が安全です。

1. `docs/planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md` と `docs/planned/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md` を並べて読む
2. `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md` を読む
3. `docs/analysis/COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md` を突き合わせる
4. `docs/analysis/WIDGET_GAP_ANALYSIS_2026-06-03.md` で既知の実装済み領域を除外する
5. 実装 or 文書更新に入る

---

## Notes

- この roadmap は、機能一覧ではなく「今どれから触るか」のためのもの
- 新しい大型機能を増やす前に、既存の診断と責務境界を揃えるほうが全体効率が高い
- 迷ったら `ArtifactTimelineTrackPainterView` を正規名として扱い、`TimelineTrackView` は legacy reference とみなす
