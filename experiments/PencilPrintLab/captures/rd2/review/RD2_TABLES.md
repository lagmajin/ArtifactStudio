# RD2 comparison tables (blank conclusion)

No gate is set; no pass/fail or ranking is recorded.

## Alpha fixtures: descriptor delta and absolute probes

| fixture | candidate | model | descriptor delta | visible max | near max | near >1 | near >10 | nonfinite |
|---|---|---|---:|---:|---:|---:|---:|---:|
| alpha_boundary_small | pp_a | v1 | 0 | 0.8656 | 1.159 | 247 | 0 | 0 |
| alpha_boundary_small | pp_a | v2 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b | v1 | 5.96e-08 | 0.6038 | 0.6038 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b | v2 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v1 | 1.49e-08 | 0.6 | 0 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v2 | 1.49e-08 | 0.6 | 0.2 | 0 | 0 | 0 |
| alpha_boundary_large | pp_a | v1 | 0 | 0.8537 | 1.177 | 2186 | 0 | 0 |
| alpha_boundary_large | pp_a | v2 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b | v1 | 5.96e-08 | 0.6038 | 0.6038 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b | v2 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v1 | 5.96e-08 | 0.6667 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v2 | 2.98e-08 | 0.6 | 0.4 | 0 | 0 | 0 |

## Opaque v1/v2 max RGB delta

| fixture | candidate | max delta |
|---|---|---:|
| color_ramp_small | pp_a | 0.2 |
| color_ramp_small | pp_b | 1 |
| color_ramp_small | pp_c | 0.1991 |
| thin_lines_small | pp_a | 0.2038 |
| thin_lines_small | pp_b | 1 |
| thin_lines_small | pp_c | 0.1994 |
| flat_highlight_small | pp_a | 1.255 |
| flat_highlight_small | pp_b | 1.106 |
| flat_highlight_small | pp_c | 1.147 |
| color_ramp_large | pp_a | 0.2 |
| color_ramp_large | pp_b | 1 |
| color_ramp_large | pp_c | 0.1999 |
| thin_lines_large | pp_a | 0.2 |
| thin_lines_large | pp_b | 1 |
| thin_lines_large | pp_c | 0.2 |
| flat_highlight_large | pp_a | 1.471 |
| flat_highlight_large | pp_b | 1.199 |
| flat_highlight_large | pp_c | 1.2 |

## Temporal global MAE per step (v1 / v2)

| candidate | v1 | v2 |
|---|---|---|
| pp_a | 0.000300,0.000320,0.000300,0.000320,0.000301,0.000320,0.000303 | 0.000466,0.000438,0.000470,0.000450,0.000441,0.000455,0.000424 |
| pp_b | 0.000689,0.000729,0.000698,0.000673,0.000723,0.000736,0.000680 | 0.001624,0.001489,0.001815,0.001707,0.001542,0.001723,0.001560 |
| pp_c | 0.000204,0.000175,0.000203,0.000175,0.000204,0.000176,0.000204 | 0.000147,0.000155,0.000155,0.000142,0.000152,0.000145,0.000148 |

## Performance

| fixture | candidate | model | seconds | tracemalloc peak bytes |
|---|---|---|---:|---:|
| color_ramp_small | pp_a | v1 | 0.0313 | 24087264 |
| color_ramp_small | pp_a | v2 | 0.0295 | 17861480 |
| color_ramp_small | pp_b | v1 | 0.0500 | 24087264 |
| color_ramp_small | pp_b | v2 | 0.0305 | 22284544 |
| color_ramp_small | pp_c | v1 | 0.0228 | 17762448 |
| color_ramp_small | pp_c | v2 | 0.0330 | 22350448 |
| color_ramp_large | pp_a | v1 | 0.4451 | 231213792 |
| color_ramp_large | pp_a | v2 | 0.3051 | 171445096 |
| color_ramp_large | pp_b | v1 | 0.5153 | 231213792 |
| color_ramp_large | pp_b | v2 | 0.4078 | 213911808 |
| color_ramp_large | pp_c | v1 | 0.2795 | 169937040 |
| color_ramp_large | pp_c | v2 | 0.3449 | 213977712 |
| alpha_boundary_small | pp_a | v1 | 0.0267 | 24087264 |
| alpha_boundary_small | pp_a | v2 | 0.0280 | 17861480 |
| alpha_boundary_small | pp_b | v1 | 0.0390 | 24087264 |
| alpha_boundary_small | pp_b | v2 | 0.0287 | 22284544 |
| alpha_boundary_small | pp_c | v1 | 0.0208 | 17762448 |
| alpha_boundary_small | pp_c | v2 | 0.0304 | 22350448 |
