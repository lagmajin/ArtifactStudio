# Lightweight Tracer / Frame Timeline

**Date**: 2026-04-21
**Status**: Completed
**Parent**: `ProfilerPanelWidget`

Lightweight Tracer / Frame Timeline が実装済みのため、この milestone を完了扱いにする。  
`ProfilerPanelWidget` 上で scope / event / audio / trace の各 lane をまとめて読めるようになっている。

## Evidence

- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `docs/planned/MILESTONES_BACKLOG.md`

## Result

- frame snapshot を tracer の seed にできる
- scope / thread / crash の trace が一覧できる
- trace timeline を同一パネルで確認できる
