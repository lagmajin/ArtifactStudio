# Taste (Continuously Learned by [CommandCode][cmd])

[cmd]: https://commandcode.ai/

# cppm-modules
- In C++20 module files (.cppm/.ixx), do NOT put `#include` directives in the module purview (after `module X;`). All includes must go in the Global Module Fragment (GMF) between `module;` and `export module X;`. The only exception is TBB includes which should be placed in GMF with all other includes, NOT in the purview. Confidence: 0.80

# project-workflow
- Do not run build, tests, or formatting commands for the ArtifactStudio project unless the user explicitly requests it. Confidence: 0.85
- Do not commit or push to the parent/child git repos (Artifact/, ArtifactCore/, ArtifactWidgets/) unless the user explicitly asks; just stop at the file-edit step. Confidence: 0.85
- Before creating a new feature/widget/file, verify by exploration that an equivalent implementation does not already exist (under any name/alias) in the workspace; only proceed if the check is explicit. Confidence: 0.60
- When `git status` shows modifications to files outside the current task scope, treat them as the user's in-progress work; run `git diff` to understand intent but do NOT revert, edit, or commit those changes. Only touch files directly required by the current task. Confidence: 0.85

# project-code-style
- Do not introduce new QtCSS or `QObject::setStyleSheet(...)` calls in new code; use QPalette, owner-draw, QProxyStyle, or existing theme tokens instead. Confidence: 0.80
- Do not introduce new public `QImage` usage except at IO/compat boundaries; prefer GPU/buffer types. Confidence: 0.80
- Avoid adding new `QObject` signal/slot connections (especially global/centralized event wiring) in new code; reuse existing event paths/services. New public signals/slots need design review. Confidence: 0.80

# project-submodule-boundary
- Do not modify, commit, or push the `ArtifactWidgets`, `libs/...`, or `third_party/*` submodules unless the user explicitly asks; treat them as read-only/external. New UI widgets are first placed under `Artifact/` and only promoted to `ArtifactWidgets/` later. Confidence: 0.85

