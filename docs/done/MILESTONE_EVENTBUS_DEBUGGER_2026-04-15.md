# EventBus Debugger

**Date**: 2026-04-15
**Status**: Completed
**Parent**: `EventBusDebugger` / `EventBusDebuggerWidget`

EventBus Debugger が実装済みのため、この milestone を完了扱いにする。  
publish hook、type registry、log / subscriber / frequency snapshot の監視導線が揃っている。

## Evidence

- `docs/planned/MILESTONES_BACKLOG.md`
- `ArtifactCore/include/Event/EventBusDebugger.ixx`
- `Artifact/src/Widgets/Diagnostics/EventBusDebuggerWidget.cppm`

## Result

- fire log を見られる
- subscriber snapshot を見られる
- frequency snapshot を見られる
- attach / detach で debugger を接続できる
