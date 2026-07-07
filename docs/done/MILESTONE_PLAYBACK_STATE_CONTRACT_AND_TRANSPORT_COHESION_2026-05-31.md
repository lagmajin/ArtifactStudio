# Playback State Contract and Transport Cohesion

**Date**: 2026-05-31
**Status**: Completed
**Parent**: `ArtifactPlaybackService`

`ArtifactPlaybackService` を state authority としたまま、secondary preview と diagnostics surfaces が same playback truth を読むように揃えた。RAM preview の blank 表示に reason を返し、`AppDebuggerWidget` と `ArtifactViewMenu` 側の表現も整合している。

## Evidence

- `Artifact/include/Service/ArtifactPlaybackService.ixx`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`

## Result

- playback wording が service / diagnostics / preview で揃った
- silent blank ではなく reason を出せる
- secondary preview が cache miss / not ready 系を説明できる

