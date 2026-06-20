# Render Preflight / Output Safety Check

**Date**: 2026-06-02
**Status**: Completed
**Source**: [`../planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`](../planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md)
**Phase 1 Memo**: [`../planned/MILESTONE_RENDER_PREFLIGHT_PHASE1_EXECUTION_2026-06-02.md`](../planned/MILESTONE_RENDER_PREFLIGHT_PHASE1_EXECUTION_2026-06-02.md)

## Summary

`render preflight` は、既存の `ArtifactRenderQueueService` と render output dialog の診断経路で、静的検査と既存 validation 結果の再利用を中心に進める方針で整理された。

## Current Ground Truth

- render queue preflight は `ArtifactRenderQueueService::preflightRenderQueueAt()` / `preflightAllRenderQueues()` で提供される
- render output dialog は frame rate / alpha / format の preflight 表示を持つ
- `Problem View` と `AppDebugger` は同じ診断語彙に寄せる前提で整理された
- Warning は案内、Error はブロック対象という役割分担が固定されている

## Done Criteria

- render 前に止めるべきものが見える
- warning と error が同じ文法で読める
- queue / export dialog / debugger / problem view で結果が食い違わない

## Related Files

- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

