# communication-integrity
- Always read actual source code before answering — never rely solely on documents, speculation, or unchecked assumptions. If something is unverified or unclear, explicitly state 「未確認」rather than guessing or presenting speculation as fact. This applies to ALL responses, not just feature-gap investigations. The user issued this directive after being told that MFR was "not implemented" (which led to a full milestone being written), when in fact MFR was already implemented with farm distribution + checkpoint recovery — the assistant had trusted its own assumption rather than reading the source first. Confidence: 0.95

# analysis
- When investigating what features are missing or incomplete in the Artifact project, do NOT trust doc/analysis files (`docs/analysis/`, `docs/planned/`) at face value — many are outdated. Always verify claims by searching the actual source code (.cppm/.ixx) for real implementations before reporting status. The user has corrected this multiple times: OCIO rated 🔴5% in gap analysis was actually 🟢75% implemented; MFR was written up as a full milestone under the assumption it was absent, but was actually already implemented with farm distribution + checkpoint recovery. The user proactively questions milestone premises when their own knowledge of the codebase contradicts the assistant's doc-based claims. The color system audit (2026-08-02) reinforced this: the user explicitly demanded 「実際のコードを見て」rather than accepting a rough summary. Confidence: 0.95
- When the user explicitly requests a detailed subsystem audit (「大まかにではなく細かく精査してほしい」「実際のコードを見て」), they expect a systematic code-level analysis: read every source file in the subsystem, evaluate per-component with quality scores (🟢/🟡/🟠/🔴), cite actual code snippets as evidence, identify specific issues (code duplication, missing pieces, quality gaps, architectural inconsistencies like dual implementations), and benchmark against industry standards where applicable (e.g. SMPTE ST 2084 constants, DaVinci Resolve feature parity). Save to `docs/analysis/<TOPIC>_AUDIT_YYYY-MM-DD.md`. This is distinct from feature-gap analysis (what's missing vs AE/Nuke), missing-modules analysis (what doesn't exist at all), and cross-app comparison — it's an inward-facing code quality and completeness audit of an existing subsystem. Confidence: 0.70
- After a high-quality subsystem audit is delivered and well-received, the user may initiate a full-codebase sequential audit sweep — "今のようなレポートを順番に全モジュールをしらべてみて" (investigate all modules in order like the report you just did). The user expects the same depth applied module-by-module in dependency order, not a single monolithic document. This is a distinct workflow track from feature-gap analysis and cross-app comparison — an inward-facing code-quality sweep of every existing subsystem. Confidence: 0.65
- When the user asks "is this really enough / sufficient?" (「本当にこれで十分なのか？」) after receiving a milestone or assessment, they are challenging the ambition level of the analysis — not seeking reassurance. The correct response is to re-examine critically against professional/industry-standard tools (e.g. Deadline, Tractor, Smedge, HQueue, AFANASY for render farm), identify gaps the initial analysis missed, and produce a structured gap analysis with priority levels — not to defend the original assessment. The user thinks at the system level and expects the assistant to proactively identify what was overlooked. Confidence: 0.75

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
