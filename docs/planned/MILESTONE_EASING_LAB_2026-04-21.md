# MILESTONE: EasingLab - Easing Comparison Tool

> 2026-04-21 Created

## Objective

Create a specialized UI ("EasingLab") that allows simultaneous comparison of multiple easing candidates for a selected keyframe segment, enabling quick visual decision-making and application of animation curves.

---

## Background

Currently, the animation system primarily supports linear interpolation or requires manual curve adjustment in the graph editor. For motion graphics, finding the "right feel" for a transition often involves repetitive trial-and-error. EasingLab speeds up this workflow by providing a side-by-side preview of common easing presets.

---

## Phase 1: Basic Comparison & Application

### Goals
- [ ] Implement `EasingLabDialog` for side-by-side comparison.
- [ ] Define `EasingCandidate` and presets (Linear, EaseIn, EaseOut, EaseInOut, Back, Expo).
- [ ] Create `EasingPreviewWidget` for lightweight animation previews.
- [ ] Synchronized scrubbing across all candidates.
- [ ] One-click application of easing to the original keyframe segment.

### Non-Goals (Out of Scope for initial implementation)
- Complex multi-segment editing.
- AI-based recommendations.
- Interactive Bezier editing within the lab (presets only).
- Permanent layer cloning (use preview instances/lightweight data).

---

## Technical Design

### Data Structures

```cpp
enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Back,
    Expo
};

struct EasingCandidate {
    QString name;
    EasingType type;
};
```

### Responsibility Boundaries

- **EasingLabDialog**: Orchestrates selection, preview tiling, and application.
- **EasingPreviewWidget**: Handles rendering of a single candidate's preview (shape movement + curve visualization).
- **EasingCurveUtil**: Shared logic for calculating eased values based on standard formulas.
- **applyEasingToSelectedSegment()**: Utility function to modify the actual keyframe data and trigger Undo.

---

## Tasks

### 1. Core Logic & Presets
- [ ] Implement `EasingCurveUtil` with standard easing functions.
- [ ] Define `EasingCandidate` list.

### 2. UI Components
- [ ] Create `EasingPreviewWidget` (QPainter drawing).
- [ ] Implement `EasingLabDialog` layout (grid).
- [ ] Top scrub slider for synchronized preview.

### 3. Integration
- [ ] Integration with `ArtifactTimelineWidget` or `PropertyEditor` to identify selected keyframes.
- [ ] Apply logic using the existing Command/Undo system.

---

## Success Criteria
- EasingLab can be opened from the Timeline/Inspector.
- At least 6 easing candidates are visible with live (scrubbable) previews.
- Clicking "Apply" correctly updates the selected keyframe's easing in the actual composition.

---

## Links

- [Phase 1](MILESTONE_EASING_LAB_PHASE1_2026-04-21.md)
- [Phase 2](MILESTONE_EASING_LAB_PHASE2_2026-04-21.md)
- [Phase 3](MILESTONE_EASING_LAB_PHASE3_2026-04-21.md)
