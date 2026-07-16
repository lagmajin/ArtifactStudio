# analysis
- When investigating what features are missing or incomplete in the Artifact project, do NOT trust doc/analysis files (\`docs/analysis/\`, \`docs/planned/\`) at face value — many are outdated. Always verify claims by searching the actual source code (.cppm/.ixx) for real implementations before reporting status. The user explicitly corrected trusting outdated analysis docs over source code. Confidence: 0.75

# Taste (Continuously Learned by [CommandCode][cmd])

[cmd]: https://commandcode.ai/


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

# project-architecture
- For new functionality in `ArtifactPr`, prefer an internal/centralized event system architecture (the style used in `Artifact/`) rather than ad-hoc Qt signal-slot wiring between widgets. Surface components should publish events to a central bus/service that other layers subscribe to. Confidence: 0.60
- When given a choice between a "faithful/comprehensive" (本格的) port/implementation approach vs a simplified/quick approach, prefer the comprehensive option. The user explicitly chose full tile-based DOF (7-pass Jimenez) over a simplified prepass+main alternative when asked. Confidence: 0.70

