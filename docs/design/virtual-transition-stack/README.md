# Virtual Transition / Transition Stack — Project

**最終更新:** 2026-11-20
**ステータス:** In Progress

![Saved Virtual Transition / Transition Stack reference](virtual-transition-stack-feature-spec.png)

This folder preserves the original user-provided reference image. Its SHA-256 is `7D6D0902494C7B8E1C3EEDBDC576A7B0E6FFD089AD9CDFCC2C203613EAEAFF5E`.

## Project files

- Product requirements, implementation decisions, phases, and acceptance checks: [MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md](../../planned/MILESTONE_VIRTUAL_TRANSITION_STACK_2026-11-20.md)
- Contract test target: `ArtifactVirtualTransitionStackContractTest` in `tests/Artifact/CMakeLists.txt`
- Model, timeline, and GPU kernel contract sources: `tests/Artifact/ArtifactVirtualTransitionStackContractTest.cpp` and `tests/Artifact/ArtifactVirtualTransitionGpuCrossfadeTest.cpp`

The contract sources currently cover transition serialization and source identity, frame evaluation and cached source indices, simultaneous evaluation of disjoint transitions into a caller-reused workspace, compact timeline marker rendering, and the GPU crossfade kernel's pixel output at progress 0, 0.5, and 1. The Preview and Render Queue opacity paths now consume all active opacity-only transitions. Tests still do not prove GPU group composition, insertion into the composition stack, source draw suppression, or preview/final-render parity; those remain implementation and acceptance work in the milestone.

The repository's `AGENTS.md` currently prohibits running builds, tests, or CMake without an explicit user instruction. The test target and sources are registered, but their compilation and execution remain unverified.
