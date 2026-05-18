# App Surface Cohesion - Phase 4 Execution

**Date**: 2026-05-17

**Source**: [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)

---

## Phase 4 Goal

Project / Asset / Timeline / Composition / Debugger の表面文法を最後に合わせ、画面を移動しても `current -> recent -> selection -> status -> next action` の読み順が崩れないようにする。

---

## Scope

### In

- cross-surface wording finish
- summary strip ordering
- status / empty action の役割分離
- Phase 1-3 の進捗整理

### Out

- new global signal/slot
- QtCSS based restyle
- render backend changes
- heavy validation pass

---

## Progress Note - 2026-05-17

- `ArtifactAssetBrowser`: Library Hub の行順を `Current -> Recent -> Selection -> Favorites/Sources/Status` へ寄せ、Recent の重複表示を減らした
- `ArtifactProjectManagerWidget`: project health と選択詳細を `Status` 起点の文言へ寄せ、`Current` との混線を減らした
- `ArtifactTimelineWidget`: keyframe status の `Now` 表記を `Current` に寄せ、空の recent を `Recent: -` に統一した
- `ArtifactCompositionEditor`: Phase 1-3 の info overlay 文法を維持し、render/controller 境界には触れない
- `AppDebuggerWidget`: diagnostics 側は `Goal / Now / Warning / Next` を維持し、surface 側の `Status` と混ぜない

---

## Validation Note

- Code edits are limited to UI wording / label sizing / summary ordering.
- Build and tests were not run in this pass.
- `git diff --check` should be used for this surface slice before handoff.

