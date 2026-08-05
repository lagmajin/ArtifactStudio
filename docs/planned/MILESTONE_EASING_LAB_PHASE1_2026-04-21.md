> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_EASING_LAB.md](MILESTONE_EASING_LAB.md)

# EasingLab - Phase 1: Core Easing Math and Presets

Date: 2026-04-21

## Purpose

Lock down the easing vocabulary and calculation helpers before building the dialog UI.

This phase is intentionally headless. It defines the candidates, the shared formulas, and the data shape that the preview layer and apply path will reuse.

## Scope

- `EasingType` definition
- `EasingCandidate` catalog
- shared easing evaluation helpers
- preset naming and display labels
- mapping to existing `QEasingCurve::Type` / interpolation concepts where useful

## Out Of Scope

- preview widgets
- dialog layout
- timeline integration
- undo/apply wiring
- bezier editing

## Execution Steps

### 1. Define the candidate model

- add `EasingType` if no direct equivalent exists in the current animation layer
- define a lightweight `EasingCandidate` record with display name and type
- keep the candidate list stable and small for the first slice

### 2. Implement easing math

- add `EasingCurveUtil`
- cover linear, ease-in, ease-out, ease-in-out, back, and expo
- keep the API pure and deterministic
- clamp inputs to the canonical `[0, 1]` range

### 3. Map to existing animation concepts

- align the new preset catalog with existing `InterpolationType` / `QEasingCurve::Type` semantics where possible
- document which presets are direct mappings and which are preview-only aliases

## Definition Of Done

- the codebase has a reusable easing math helper
- the candidate catalog is defined in one place
- the formulas are deterministic and ready for preview UI reuse

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. build the preview widget
2. tile multiple candidates in a dialog
3. wire synced scrubbing to the preview surface

---

## Static audit follow-up (2026-07-25)

`ArtifactCore/src/Animation/EasingCurveUtil.cppm` を確認した。候補モデル、安定した catalog、入力 clamp、easing 評価、既存 `InterpolationType` への mapping が一箇所にまとまっている。

| Definition of Done | 現状 | 判定 |
|---|---|---|
| reusable easing math helper | `evaluateEasing()` と `clampUnit()` が UI 非依存の Core にある | 実装済み |
| single candidate catalog | `defaultEasingCandidates()` が初期候補を提供する | 実装済み |
| deterministic formulas | Linear / Ease / Back / Expo 等の純粋な評価関数がある | 実装済み（静的確認） |
| interpolation compatibility | `easingTypeToInterpolation()` が既存 enum に対応付ける | 実装済み |
| headless boundary | EasingLab widget は Core helper を import して利用し、Core 側に UI 依存を持ち込んでいない | 実装済み |

**判定**: Phase 1 はソース上完了。数値境界・各 preset の runtime 動作確認は未実施のため、検証状態は別途保留する。
