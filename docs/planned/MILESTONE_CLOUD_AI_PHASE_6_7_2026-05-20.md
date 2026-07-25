# Cloud AI Phase 6 & 7: Export API & Timeline Operations

**Date:** 2026-05-20
**Status:** In Progress
**Priority:** 90/100 (Tier 1 from Roadmap)

## Overview
This milestone covers two critical API expansions for the Cloud AI assistant integration:
1. **Phase 6 - Export API**: Allows AI to export the current composition to a specified file format (MP4, PNG Sequence).
2. **Phase 7 - Timeline Operations**: Allows AI to manipulate timeline playback, seek, and work area trims.

## Scope & Implementation Plan

### Phase 6: Export API
- Create `ArtifactCore/include/Automation/ExportServiceAPI.ixx`
  - Defines export format options (resolution, framerate, bit rate, output path).
- Update `WorkspaceAutomation.cppm`
  - Introduce `exportComposition(options)` which blocks the caller until the background render queue completes the job.

### Phase 7: Timeline Operations
- Update `WorkspaceAutomation.cppm` to hook into the PlaybackService or TimelineService.
  - Implement `seekTimeline(double timeInSeconds)`
  - Implement `playTimeline()` / `pauseTimeline()`
  - Implement `setWorkArea(int64_t startFrame, int64_t endFrame)`

## Acceptance Criteria
- AI can trigger an export and reliably receive a response when the file is successfully written to disk.
- AI can move the playhead and set the work area boundaries.
- No memory leaks or thread blocks that freeze the main UI during export.

## 2026-07-25 実装監査

WorkspaceAutomation／CommandIRExecutor に `exportComposition`／`exportCurrentComposition`、render queue 登録・完了待ち・timeout/cancel、playback／seek／work area 操作の入口があり、AI tool bridge から呼べることを確認した。一方、指定ファイルが実際に正常生成されたことの厳密な返却、長時間 export 中の UI 非ブロック、memory leak、seek／work area の実機動作、失敗時の normalized response は runtime で確認できない。したがって Phase 6／7 は API 部分実装済み・受け入れ未完了・runtime未検証とする。
