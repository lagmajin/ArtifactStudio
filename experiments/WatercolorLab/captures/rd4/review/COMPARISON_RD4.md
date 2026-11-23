# RD4 comparison table (blank conclusion)

Descriptor delta checks input-descriptor consistency; absolute probes check soundness.
No gate is set, so no pass/fail or ranking is recorded here.

| fixture | candidate | model | descriptor delta | visible max | near-transparent max | near >10 channels |
|---|---|---|---:|---:|---:|---:|
| alpha_boundary_small | wc_a | v1 | 0.138 | 0.663 | 0.6417 | 0 |
| alpha_boundary_small | wc_a | v2 | 0 | 0.6631 | 1.615 | 0 |
| alpha_boundary_small | wc_a | v3 | 0 | 0.663 | 0.6417 | 0 |
| alpha_boundary_small | wc_b | v1 | 0.383 | 0.7007 | 0.676 | 0 |
| alpha_boundary_small | wc_b | v2 | 0 | 0.701 | 187.2 | 82 |
| alpha_boundary_small | wc_b | v3 | 0 | 0.7007 | 0.6837 | 0 |
| alpha_boundary_small | wc_c | v1 | 0.199 | 0.6743 | 0.6613 | 0 |
| alpha_boundary_small | wc_c | v2 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | wc_c | v3 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | only_bleed | v1 | 1.19e-07 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_bleed | v2 | 0 | 0.6049 | 27.17 | 4 |
| alpha_boundary_small | only_bleed | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_paper | v1 | 1.19e-07 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_paper | v2 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_paper | v3 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_edge | v1 | 0.134 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_edge | v2 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_edge | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | wc_a | v1 | 0.146 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_a | v2 | 0 | 0.6637 | 13.52 | 4 |
| alpha_boundary_large | wc_a | v3 | 0 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_b | v1 | 0.389 | 0.7008 | 0.6776 | 0 |
| alpha_boundary_large | wc_b | v2 | 0 | 0.701 | 258.2 | 634 |
| alpha_boundary_large | wc_b | v3 | 0 | 0.7008 | 0.7153 | 0 |
| alpha_boundary_large | wc_c | v1 | 0.204 | 0.6999 | 0.6647 | 0 |
| alpha_boundary_large | wc_c | v2 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | wc_c | v3 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | only_bleed | v1 | 1.19e-07 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_bleed | v2 | 0 | 0.6057 | 10.49 | 6 |
| alpha_boundary_large | only_bleed | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_paper | v1 | 1.79e-07 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_paper | v2 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_paper | v3 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_edge | v1 | 0.139 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_edge | v2 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_edge | v3 | 0 | 0.6038 | 0.6038 | 0 |

## Opaque look parity (v1 / v2 / v3 max RGB delta)

| fixture | candidate | v1-v2 | v1-v3 | v2-v3 |
|---|---|---:|---:|---:|
| color_ramp_small | wc_a | 0 | 0.0106 | 0.0106 |
| color_ramp_small | wc_b | 0 | 2.12e-05 | 2.12e-05 |
| color_ramp_small | wc_c | 0 | 0 | 0 |
| color_ramp_small | only_bleed | 0 | 0.0118 | 0.0118 |
| color_ramp_small | only_paper | 0 | 0 | 0 |
| color_ramp_small | only_edge | 0 | 0 | 0 |
| thin_lines_small | wc_a | 0 | 0.000499 | 0.000499 |
| thin_lines_small | wc_b | 0 | 2.95e-05 | 2.95e-05 |
| thin_lines_small | wc_c | 0 | 0 | 0 |
| thin_lines_small | only_bleed | 0 | 0.000477 | 0.000477 |
| thin_lines_small | only_paper | 0 | 0 | 0 |
| thin_lines_small | only_edge | 0 | 0 | 0 |
| flat_highlight_small | wc_a | 0 | 0.00968 | 0.00968 |
| flat_highlight_small | wc_b | 0 | 0 | 0 |
| flat_highlight_small | wc_c | 0 | 0 | 0 |
| flat_highlight_small | only_bleed | 0 | 0.0117 | 0.0117 |
| flat_highlight_small | only_paper | 0 | 0 | 0 |
| flat_highlight_small | only_edge | 0 | 0 | 0 |
| color_ramp_large | wc_a | 0 | 0.0106 | 0.0106 |
| color_ramp_large | wc_b | 0 | 2.91e-05 | 2.91e-05 |
| color_ramp_large | wc_c | 0 | 0 | 0 |
| color_ramp_large | only_bleed | 0 | 0.0117 | 0.0117 |
| color_ramp_large | only_paper | 0 | 0 | 0 |
| color_ramp_large | only_edge | 0 | 0 | 0 |
| thin_lines_large | wc_a | 0 | 0.000512 | 0.000512 |
| thin_lines_large | wc_b | 0 | 2.05e-05 | 2.05e-05 |
| thin_lines_large | wc_c | 0 | 0 | 0 |
| thin_lines_large | only_bleed | 0 | 0.000519 | 0.000519 |
| thin_lines_large | only_paper | 0 | 0 | 0 |
| thin_lines_large | only_edge | 0 | 0 | 0 |
| flat_highlight_large | wc_a | 0 | 0.00932 | 0.00932 |
| flat_highlight_large | wc_b | 0 | 0.000247 | 0.000247 |
| flat_highlight_large | wc_c | 0 | 0 | 0 |
| flat_highlight_large | only_bleed | 0 | 0.00964 | 0.00964 |
| flat_highlight_large | only_paper | 0 | 0 | 0 |
| flat_highlight_large | only_edge | 0 | 0 | 0 |
