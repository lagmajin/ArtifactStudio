# communication-integrity
- Always read actual source code before answering — never rely solely on documents, speculation, or unchecked assumptions. If something is unverified or unclear, explicitly state 「未確認」rather than guessing or presenting speculation as fact. This applies to ALL responses, not just feature-gap investigations. The user issued this directive after being told that MFR was "not implemented" (which led to a full milestone being written), when in fact MFR was already implemented with farm distribution + checkpoint recovery — the assistant had trusted its own assumption rather than reading the source first. Confidence: 0.95

# analysis
See [analysis/taste.md](analysis/taste.md)
# Taste (Continuously Learned by [CommandCode][cmd])

[cmd]: https://commandcode.ai/


# cost-benefit-prioritization
- When deciding which types in the codebase to extend with operators/utilities, prioritize based on cross-codebase usage footprint, not just whether the type is a color/vector/etc. Types used only as local variables in a single translation unit (e.g. HSVColor/HSLColor used only inside ColorConversion) are low value for operator expansion. Types heavily used across multiple subsystems (e.g. DynVec2/3 used throughout AnimationDynamics) get higher priority. The user accepted this cost-benefit analysis — "value of wide cross-cutting impact" — without objection. Confidence: 0.70

# cppm-modules
- In C++20 module files (.cppm/.ixx), do NOT put `#include` directives in the module purview (after `module X;`). All includes must go in the Global Module Fragment (GMF) between `module;` and `export module X;`. The only exception is TBB includes which should be placed in GMF with all other includes, NOT in the purview. Confidence: 0.80

# project-workflow
See [project-workflow/taste.md](project-workflow/taste.md)

# project-code-style
- Do not introduce new QtCSS or `QObject::setStyleSheet(...)` calls in new code; use QPalette, owner-draw, QProxyStyle, or existing theme tokens instead. Confidence: 0.80
- Do not introduce new public `QImage` usage except at IO/compat boundaries; prefer GPU/buffer types. Confidence: 0.80
- Avoid adding new `QObject` signal/slot connections (especially global/centralized event wiring) in new code; reuse existing event paths/services. New public signals/slots need design review. Confidence: 0.80

# project-submodule-boundary
- Do not modify, commit, or push the `ArtifactWidgets`, `libs/...`, or `third_party/*` submodules unless the user explicitly asks; treat them as read-only/external. New UI widgets are first placed under `Artifact/` and only promoted to `ArtifactWidgets/` later. Confidence: 0.85
- Treat `Artifact_dev_review/` as a read-only sandbox copy; do not edit files inside it. Only modify the `Artifact/` directory for code changes. Confidence: 0.70

# ux-design-philosophy
- Prefers innovative UX interactions with "wow factor" over standard drag-and-drop / conventional patterns. When asked for feature proposals (especially D&D/UI interactions), the user expects the assistant to brainstorm creative, surprising ideas that make users say 「そんなことできるの？」(you can do that?!), rather than straightforward conventional implementations. The user explicitly rejected a standard "drag effect from palette to layer" milestone as too plain/obvious (「地味だ」) and asked for more revolutionary interaction designs. Confidence: 0.80

# project-architecture
See [project-architecture/taste.md](project-architecture/taste.md)

# task-preference
- When presented with a choice between stability/bug-fix work and new feature development, prefers new feature creation (新機能作成). Confidence: 0.55
