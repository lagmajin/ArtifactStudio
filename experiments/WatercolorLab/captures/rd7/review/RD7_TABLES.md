# RD7 v5 comparison tables (blank conclusion)

v5 params: {'k': 11, 'sigma_space': 1.0, 'sigma_color': 0.6}. No gate is set; no pass/fail or ranking is recorded.

## Alpha fixtures: descriptor delta and absolute probes

| fixture | candidate | model | descriptor delta | visible max | near max | near >10 channels |
|---|---|---|---:|---:|---:|---:|
| alpha_boundary_small | wc_a | v3 | 0 | 0.663 | 0.6417 | 0 |
| alpha_boundary_small | wc_a | v4 | 0 | 0.663 | 0.642 | 0 |
| alpha_boundary_small | wc_a | v5 | 0 | 0.663 | 0.6411 | 0 |
| alpha_boundary_small | wc_b | v3 | 0 | 0.7007 | 0.6837 | 0 |
| alpha_boundary_small | wc_b | v4 | 0 | 0.7007 | 0.6837 | 0 |
| alpha_boundary_small | wc_b | v5 | 0 | 0.7007 | 0.6837 | 0 |
| alpha_boundary_small | wc_c | v3 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | wc_c | v4 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | wc_c | v5 | 0 | 0.6743 | 0.6674 | 0 |
| alpha_boundary_small | only_bleed | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_bleed | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_bleed | v5 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_paper | v3 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_paper | v4 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_paper | v5 | 0 | 0.6618 | 0.6574 | 0 |
| alpha_boundary_small | only_edge | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_edge | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_small | only_edge | v5 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | wc_a | v3 | 0 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_a | v4 | 0 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_a | v5 | 0 | 0.6631 | 0.6439 | 0 |
| alpha_boundary_large | wc_b | v3 | 0 | 0.7008 | 0.7153 | 0 |
| alpha_boundary_large | wc_b | v4 | 0 | 0.7008 | 0.7153 | 0 |
| alpha_boundary_large | wc_b | v5 | 0 | 0.7008 | 0.7153 | 0 |
| alpha_boundary_large | wc_c | v3 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | wc_c | v4 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | wc_c | v5 | 0 | 0.6999 | 0.6662 | 0 |
| alpha_boundary_large | only_bleed | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_bleed | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_bleed | v5 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_paper | v3 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_paper | v4 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_paper | v5 | 0 | 0.6617 | 0.6577 | 0 |
| alpha_boundary_large | only_edge | v3 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_edge | v4 | 0 | 0.6038 | 0.6038 | 0 |
| alpha_boundary_large | only_edge | v5 | 0 | 0.6038 | 0.6038 | 0 |

## Opaque max RGB delta

| fixture | candidate | v4-v5 | v3-v5 |
|---|---|---:|---:|
| color_ramp_small | wc_a | 0.1377 | 0.2122 |
| color_ramp_small | wc_b | 0 | 0 |
| color_ramp_small | wc_c | 0 | 0 |
| color_ramp_small | only_bleed | 0.2338 | 0.3798 |
| color_ramp_small | only_paper | 0 | 0 |
| color_ramp_small | only_edge | 0 | 0 |
| thin_lines_small | wc_a | 0.1721 | 0.3461 |
| thin_lines_small | wc_b | 0 | 0 |
| thin_lines_small | wc_c | 0 | 0 |
| thin_lines_small | only_bleed | 0.3069 | 0.4842 |
| thin_lines_small | only_paper | 0 | 0 |
| thin_lines_small | only_edge | 0 | 0 |
| flat_highlight_small | wc_a | 0.06694 | 0.1439 |
| flat_highlight_small | wc_b | 0 | 0 |
| flat_highlight_small | wc_c | 0 | 0 |
| flat_highlight_small | only_bleed | 0.1091 | 0.2401 |
| flat_highlight_small | only_paper | 0 | 0 |
| flat_highlight_small | only_edge | 0 | 0 |
| color_ramp_large | wc_a | 0.1373 | 0.2116 |
| color_ramp_large | wc_b | 0 | 0 |
| color_ramp_large | wc_c | 0 | 0 |
| color_ramp_large | only_bleed | 0.2342 | 0.3815 |
| color_ramp_large | only_paper | 0 | 0 |
| color_ramp_large | only_edge | 0 | 0 |
| thin_lines_large | wc_a | 0.1602 | 0.2493 |
| thin_lines_large | wc_b | 0 | 0 |
| thin_lines_large | wc_c | 0 | 0 |
| thin_lines_large | only_bleed | 0.266 | 0.4457 |
| thin_lines_large | only_paper | 0 | 0 |
| thin_lines_large | only_edge | 0 | 0 |
| flat_highlight_large | wc_a | 0.0668 | 0.1439 |
| flat_highlight_large | wc_b | 0 | 0 |
| flat_highlight_large | wc_c | 0 | 0 |
| flat_highlight_large | only_bleed | 0.1091 | 0.2402 |
| flat_highlight_large | only_paper | 0 | 0 |
| flat_highlight_large | only_edge | 0 | 0 |

## Temporal: global MAE per step (v3 / v4 / v5)

| candidate | v3 | v4 | v5 |
|---|---|---|---|
| wc_a | 0.001040,0.001041,0.001041,0.001041,0.001041,0.001041,0.001041 | 0.001029,0.001029,0.001029,0.001029,0.001029,0.001029,0.001029 | 0.001075,0.001075,0.001075,0.001075,0.001075,0.001075,0.001075 |
| wc_b | 0.001395,0.001403,0.001395,0.001403,0.001395,0.001403,0.001395 | 0.001395,0.001403,0.001395,0.001403,0.001395,0.001403,0.001395 | 0.001395,0.001403,0.001395,0.001403,0.001395,0.001403,0.001395 |
| wc_c | 0.001018,0.001018,0.001018,0.001019,0.001019,0.001019,0.001019 | 0.001018,0.001018,0.001018,0.001019,0.001019,0.001019,0.001019 | 0.001018,0.001018,0.001018,0.001019,0.001019,0.001019,0.001019 |

## Performance (offline reference model; tracemalloc = Python heap only)

| fixture | candidate | model | seconds | tracemalloc peak bytes | finite output |
|---|---|---|---:|---:|---|
| color_ramp_small | only_bleed | v3 | 0.3300 | 23709376 | True |
| color_ramp_small | only_bleed | v4 | 0.0248 | 26905576 | True |
| color_ramp_small | only_bleed | v5 | 0.0231 | 26905576 | True |
| color_ramp_small | wc_b | v3 | 0.0431 | 28020792 | True |
| color_ramp_small | wc_b | v4 | 0.0428 | 28020792 | True |
| color_ramp_small | wc_b | v5 | 0.0450 | 28020792 | True |
| color_ramp_large | only_bleed | v3 | 2.9049 | 226774744 | True |
| color_ramp_large | only_bleed | v4 | 0.2076 | 257985512 | True |
| color_ramp_large | only_bleed | v5 | 0.1915 | 257985512 | True |
| color_ramp_large | wc_b | v3 | 0.3322 | 268963896 | True |
| color_ramp_large | wc_b | v4 | 0.3332 | 268963920 | True |
| color_ramp_large | wc_b | v5 | 0.3306 | 268963920 | True |
| alpha_boundary_small | only_bleed | v3 | 0.2304 | 23709400 | True |
| alpha_boundary_small | only_bleed | v4 | 0.0185 | 26905576 | True |
| alpha_boundary_small | only_bleed | v5 | 0.0186 | 26905576 | True |
| alpha_boundary_small | wc_b | v3 | 0.0298 | 28020792 | True |
| alpha_boundary_small | wc_b | v4 | 0.0310 | 28020816 | True |
| alpha_boundary_small | wc_b | v5 | 0.0306 | 28020816 | True |
