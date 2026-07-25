# Phase 4: Diagnostics Integration

> 2026-04-21 作成

## 目的

[`docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md) の Phase 4 を、既存 diagnostics surface への統合としてまとめる。

---

## 方針

1. 既存 surface を壊さない
2. summary は text からでも読めるようにする
3. 新 UI は既存 app debugger から辿れるようにする
4. signal/slot の追加は最小限にする

---

## 実装タスク

### 1. 既存 diagnostics surface に summary を載せる

候補ファイル:

- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

やること:

- frame graph summary を表示する
- resource inspector への入口を置く
- diff 判定を text で見せる

### 2. App Debugger から辿れる導線を作る

候補ファイル:

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

やること:

- debugger surface から開ける
- 既存 dock に追加する

---

## File Tickets

### P4-T1 UI Summary Hooks

対象:

- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

完了条件:

- pipeline / resource / diff の summary が既存 UI から見える

### P4-T2 App Entry Points

対象:

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

完了条件:

- App Debugger から導線がある

---

## Static audit follow-up (2026-07-25)

Profiler、Debug Console、Frame Debug、Harness の現行導線を確認した。ビルド・実機 UI は未実施。

| Ticket | 現状 | 判定 |
|---|---|---|
| P4-T1 UI Summary Hooks | FramePipeline/Resource/StateDiff/FrameDebug の各 summary と Debug Console/Profiler の copy/filter/report が存在する。 | 実装済み／表示確認待ち |
| P4-T2 App Entry Points | App Debugger、Frame Debug、Profiler、Harness の surface は存在する。全ての pipeline/resource/diff 面を App Debugger から一貫して開く導線は未確認。 | 部分実装 |
| Cross-surface consistency | warning/error/fallback/diff の個別表示はある。共通の summary wording と責務境界の完全統一は未確認。 | 部分実装／統合待ち |

### Phase 4 判定

既存 diagnostics surface への summary 統合は進んでいるが、App Debugger の一貫した entry point と surface 横断 wording の確認が残る。Phase 4 は「部分実装／統合確認待ち」とする。
