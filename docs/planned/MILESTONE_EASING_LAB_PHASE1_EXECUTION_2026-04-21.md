# EasingLab - Phase 1 Execution

Date: 2026-04-21

## Purpose

Create the easing math and candidate catalog as a shared core that the UI can consume.

## Current Anchors

- `ArtifactCore/include/Geometry/Interpolate.ixx`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`
- `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`

## Work Items

### 1. Add a shared easing helper

- create `EasingCurveUtil` in `ArtifactCore`
- expose functions for the first preset set
- keep it independent from any widget code

### 2. Define the preset catalog

- declare the initial six candidates
- give each candidate a stable display name
- keep the catalog ordered for preview tiling

### 3. Add compatibility notes

- note how the new presets relate to current interpolation modes
- note which existing `InterpolationType` values can be directly applied

## Done When

- there is a single place to evaluate easing curves for preview
- the preset list is available to the dialog layer
- no UI code is required to use the helper

---

## Static audit follow-up (2026-07-25)

現行ソース上、実行メモの 3 作業項目は完了している。

| Work item | 現状 | 判定 |
|---|---|---|
| shared easing helper | `EasingCurveUtil` が Core module として UI から独立して存在 | 実装済み |
| initial six candidates | `defaultEasingCandidates()` が Linear / Ease In / Ease Out / Ease In Out / Back / Expo を順序付きで返す | 実装済み |
| compatibility notes | easing type と既存 interpolation の mapping helper、preview 表示ラベルがある | 実装済み |

**判定**: Execution の Done 条件はソース上達成。ビルド・数値境界・実行時適用の検証は未実施。
