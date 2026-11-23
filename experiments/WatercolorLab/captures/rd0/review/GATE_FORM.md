# RD0 gate form (thresholds intentionally blank)

Per EVALUATION_PLAN.md section 5.0: a reviewer independent of the implementer fills the tolerance column after viewing the RD0 baseline full images and crops, and before any candidate comparison. Unfilled items remain unevaluated.

| item | region | baseline evidence | tolerance (BLANK until fixed) | judge | fix date | rationale |
|---|---|---|---|---|---|---|
| 5.1 効果の正しさ | all / patch / ramp | metrics.json | | | | |
| 5.2 画質と細部 | thin_lines / flat / boundary | rd0_full_sheet, roi_sheet | | | | |
| 5.3 色と alpha | patches / alpha boundary | metrics.json alpha_contract | | | | |
| 5.4 時間安定性 | sequence | metrics.json temporal | | | | |
| 5.5 経路と解像度 | small vs large | metrics.json | | | | |
| 5.6 性能 | offline model | timing in manifest_rd0.json | | | | |
| 5.7 bleed / paper / edge | per-component ROI | metrics.json | | | | |
