# RD6 comparison tables (blank conclusion)

No gate is set; no pass/fail or ranking is recorded.

## Alpha fixtures: descriptor delta and absolute probes

| fixture | candidate | model | descriptor delta | visible max | near max | near >10 channels |
|---|---|---|---:|---:|---:|---:|
| alpha_boundary_small | wc_a | v3 | 0 | 0.663 | 0.6417 | 0 |
| alpha_boundary_small | wc_a | v4 | 0 | 0.663 | 0.642 | 0 |
| alpha_boundary_small | wc_b | v3 | 0 | 0.7007 | 0.6837 | 0 |
| alpha_boundary_small | wc_b | v4 | 0 | 0.7007 | 0.6837 | 0 |
| alpha_boundary_small | wc_c | v3 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | wc_c | v4 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | only_bleed | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_bleed | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_paper | v3 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_paper | v4 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_edge | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_edge | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | wc_a | v3 | 0 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_a | v4 | 0 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_b | v3 | 0 | 0.7008 | 0.7153 | 0 |
| alpha_boundary_large | wc_b | v4 | 0 | 0.7008 | 0.7153 | 0 |
| alpha_boundary_large | wc_c | v3 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | wc_c | v4 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | only_bleed | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_bleed | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_paper | v3 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_paper | v4 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_edge | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_edge | v4 | 0 | 0.6038 | 0.6038 | 0 |

## Opaque v3/v4 max RGB delta

| fixture | candidate | max delta |
|---|---|---:|
| color_ramp_small | wc_a | 0.2919 |
| color_ramp_small | wc_b | 0 |
| color_ramp_small | wc_c | 0 |
| color_ramp_small | only_bleed | 0.514 |
| color_ramp_small | only_paper | 0 |
| color_ramp_small | only_edge | 0 |
| thin_lines_small | wc_a | 0.4167 |
| thin_lines_small | wc_b | 0 |
| thin_lines_small | wc_c | 0 |
| thin_lines_small | only_bleed | 0.5982 |
| thin_lines_small | only_paper | 0 |
| thin_lines_small | only_edge | 0 |
| flat_highlight_small | wc_a | 0.1856 |
| flat_highlight_small | wc_b | 0 |
| flat_highlight_small | wc_c | 0 |
| flat_highlight_small | only_bleed | 0.3085 |
| flat_highlight_small | only_paper | 0 |
| flat_highlight_small | only_edge | 0 |
| color_ramp_large | wc_a | 0.2911 |
| color_ramp_large | wc_b | 0 |
| color_ramp_large | wc_c | 0 |
| color_ramp_large | only_bleed | 0.5159 |
| color_ramp_large | only_paper | 0 |
| color_ramp_large | only_edge | 0 |
| thin_lines_large | wc_a | 0.2917 |
| thin_lines_large | wc_b | 0 |
| thin_lines_large | wc_c | 0 |
| thin_lines_large | only_bleed | 0.5319 |
| thin_lines_large | only_paper | 0 |
| thin_lines_large | only_edge | 0 |
| flat_highlight_large | wc_a | 0.1854 |
| flat_highlight_large | wc_b | 0 |
| flat_highlight_large | wc_c | 0 |
| flat_highlight_large | only_bleed | 0.3086 |
| flat_highlight_large | only_paper | 0 |
| flat_highlight_large | only_edge | 0 |

## Temporal: global MAE per step (v3 / v4)

| candidate | v3 | v4 |
|---|---|---|
| wc_a | 0.001040,0.001041,0.001041,0.001041,0.001041,0.001041,0.001041 | 0.001029,0.001029,0.001029,0.001029,0.001029,0.001029,0.001029 |
| wc_b | 0.001395,0.001403,0.001395,0.001403,0.001395,0.001403,0.001395 | 0.001395,0.001403,0.001395,0.001403,0.001395,0.001403,0.001395 |
| wc_c | 0.001018,0.001018,0.001018,0.001019,0.001019,0.001019,0.001019 | 0.001018,0.001018,0.001018,0.001019,0.001019,0.001019,0.001019 |

## Performance (offline reference model; tracemalloc = Python heap only)

| fixture | candidate | model | seconds | tracemalloc peak bytes | finite output |
|---|---|---|---:|---:|---|
| color_ramp_small | only_bleed | v3 | 0.2876 | 23709376 | True |
| color_ramp_small | only_bleed | v4 | 0.0221 | 26905576 | True |
| color_ramp_small | wc_b | v3 | 0.0432 | 28020792 | True |
| color_ramp_small | wc_b | v4 | 0.0442 | 28020792 | True |
| color_ramp_large | only_bleed | v3 | 3.1430 | 226774744 | True |
| color_ramp_large | only_bleed | v4 | 0.2695 | 257985512 | True |
| color_ramp_large | wc_b | v3 | 0.3849 | 268963896 | True |
| color_ramp_large | wc_b | v4 | 0.4076 | 268963896 | True |
| alpha_boundary_small | only_bleed | v3 | 0.2328 | 23709400 | True |
| alpha_boundary_small | only_bleed | v4 | 0.0193 | 26905576 | True |
| alpha_boundary_small | wc_b | v3 | 0.0297 | 28020792 | True |
| alpha_boundary_small | wc_b | v4 | 0.0301 | 28020792 | True |
