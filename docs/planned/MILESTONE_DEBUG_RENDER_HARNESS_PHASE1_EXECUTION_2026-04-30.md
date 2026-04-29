# Debug Render Harness - Phase 1 Execution

**Date**: 2026-04-30  
**Source**: [`MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)

---

## Phase 1 Goal

デバッグ用レンダリングハーネスの責務を固定する。

この段階では描画機能を広げず、`particle-only` / `video-only` / `blend-only` の最小再現に必要な契約だけを先に決める。

---

## Scope

### In

- harness の責務と境界
- scene preset の一覧
- capture する state の最小セット
- Frame Debug への接続点

### Out

- 実描画シーンの網羅
- 別 backend の再実装
- 自動回帰ゲート本体
- UI の大規模刷新

---

## Tasks

### 1. Harness Contract

- 何を本番 renderer から借りるかを固定する
- 何を harness 側で持つかを固定する
- `device/context`, `viewport`, `clear`, `scene preset`, `capture` の境界を明文化する

### 2. Scene Preset List

- `particle-only`
- `video-only`
- `blend-only`
- `overlay-only`
- `mixed-media`

各 preset について、描く対象と描かない対象を明示する。

### 3. Snapshot Fields

- `FrameDebugSnapshot`
- particle draw state
- video decode state
- backend / viewport / RTV state
- last error / skipped reason
- selected preset / scene params

### 4. Frame Debug Link

- `FrameDebugViewWidget` で見える summary を想定する
- `ArtifactDebugConsoleWidget` でコピー可能な形にする
- `AppDebuggerWidget` から辿れる導線を残す

---

## Done Criteria

- harness の責務が 1 文で説明できる
- scene preset の一覧が固定される
- 失敗時に残す項目が決まる
- 次の Phase 2 で最小シーン実装に入れる状態になる

