# Virtual Transition / Transition Stack — Project

**最終更新:** 2026-11-21
**ステータス:** In Progress

![Saved Virtual Transition / Transition Stack reference](virtual-transition-stack-feature-spec.png)

This folder preserves the original user-provided reference image. Its SHA-256 is `7D6D0902494C7B8E1C3EEDBDC576A7B0E6FFD089AD9CDFCC2C203613EAEAFF5E`.

## Project files

- Product requirements, implementation decisions, phases, and acceptance checks: [MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md](../../planned/MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md)
- Contract test target: `ArtifactVirtualTransitionStackContractTest` in `tests/Artifact/CMakeLists.txt`
- Model, timeline, and GPU kernel contract sources: `tests/Artifact/ArtifactVirtualTransitionStackContractTest.cpp` and `tests/Artifact/ArtifactVirtualTransitionGpuCrossfadeTest.cpp`
- Render integration design proposal (2026-11-20): [RENDER_INTEGRATION_DESIGN_2026-11-20.md](RENDER_INTEGRATION_DESIGN_2026-11-20.md) — A/B temporary composite, draw suppression, insertion ordering, Undo, and parity plan. Design only; no production code changed.

The contract sources cover transition serialization and source identity, frame evaluation and cached source indices, simultaneous evaluation of disjoint transitions into a caller-reused workspace, compact timeline marker rendering, marker hover hit testing, draw-plan ordering, and the GPU crossfade kernel's pixel output at progress 0, 0.5, and 1. The Preview and Render Queue opacity paths now consume all active opacity-only transitions. Tests still do not prove GPU group composition, source draw suppression, or preview/final-render parity; those remain implementation and acceptance work in the milestone.

Builds and tests were executed on 2026-11-21 with explicit user permission using the existing `out/build/x64-Debug-Agent` tree. The target builds. With the two QWidget offscreen tests excluded, 8 of 8 model, draw-plan, and GPU kernel cases pass; the legacy source-identity index expectation was corrected from runtime evidence. Full CTest does not pass: an earlier run timed out at 30 seconds in `TimelineShowsCompactMarkerUntilTransitionIsSelected`; the latest run exits as failed after 3.76 seconds at that test's start without a diagnostic, and the isolated marker and hover UI tests stop responding. The visible timeline UI is therefore not yet verified. The build initially hit Ninja 1.12.1's stale C++ module dyndep assertion; cleaning the existing generated build outputs allowed the target to build. No Artifact submodule sources were changed.
