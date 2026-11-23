# RD3 comparison tables (blank conclusion)

No gate is set; no pass/fail or ranking is recorded.

## Alpha fixtures: descriptor delta and absolute probes

| fixture | candidate | model | descriptor delta | visible max | near max | near >1 | near >10 | nonfinite |
|---|---|---|---:|---:|---:|---:|---:|---:|
| alpha_boundary_small | pp_a | v1 | 0 | 0.8656 | 1.159 | 247 | 0 | 0 |
| alpha_boundary_small | pp_a | v2 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_small | pp_a | v3 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b | v1 | 5.96e-08 | 0.6038 | 0.6038 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b | v2 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v1 | 1.49e-08 | 0.6 | 0 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v2 | 1.49e-08 | 0.6 | 0.2 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v3 | 5.96e-08 | 0.6 | 0.4 | 0 | 0 | 0 |
| alpha_boundary_large | pp_a | v1 | 0 | 0.8537 | 1.177 | 2186 | 0 | 0 |
| alpha_boundary_large | pp_a | v2 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_large | pp_a | v3 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b | v1 | 5.96e-08 | 0.6038 | 0.6038 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b | v2 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v1 | 5.96e-08 | 0.6667 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v2 | 2.98e-08 | 0.6 | 0.4 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v3 | 5.96e-08 | 0.6 | 0.4 | 0 | 0 | 0 |

## Opaque max RGB deltas

| fixture | candidate | v1-v2 | v1-v3 | v2-v3 |
|---|---|---:|---:|---:|
| color_ramp_small | pp_a | 0.2 | 0.2 | 0 |
| color_ramp_small | pp_b | 1 | 1 | 0 |
| color_ramp_small | pp_c | 0.1991 | 0.1991 | 0 |
| thin_lines_small | pp_a | 0.2038 | 0.2038 | 0 |
| thin_lines_small | pp_b | 1 | 1 | 0 |
| thin_lines_small | pp_c | 0.1994 | 0.1994 | 0 |
| flat_highlight_small | pp_a | 1.255 | 0.3303 | 1.169 |
| flat_highlight_small | pp_b | 1.106 | 1 | 1.106 |
| flat_highlight_small | pp_c | 1.147 | 0.3025 | 1.076 |
| color_ramp_large | pp_a | 0.2 | 0.2 | 0 |
| color_ramp_large | pp_b | 1 | 1 | 0 |
| color_ramp_large | pp_c | 0.1999 | 0.1999 | 0 |
| thin_lines_large | pp_a | 0.2 | 0.2 | 0 |
| thin_lines_large | pp_b | 1 | 1 | 0 |
| thin_lines_large | pp_c | 0.2 | 0.2 | 0 |
| flat_highlight_large | pp_a | 1.471 | 0.3208 | 1.201 |
| flat_highlight_large | pp_b | 1.199 | 1 | 1.199 |
| flat_highlight_large | pp_c | 1.2 | 0.319 | 1.2 |

## Temporal global MAE per step

| candidate | v1 | v2 | v3 |
|---|---|---|---|
| pp_a | 0.000300,0.000320,0.000300,0.000320,0.000301,0.000320,0.000303 | 0.000466,0.000438,0.000470,0.000450,0.000441,0.000455,0.000424 | 0.000466,0.000438,0.000470,0.000450,0.000441,0.000455,0.000424 |
| pp_b | 0.000689,0.000729,0.000698,0.000673,0.000723,0.000736,0.000680 | 0.001624,0.001489,0.001815,0.001707,0.001542,0.001723,0.001560 | 0.001624,0.001489,0.001815,0.001707,0.001542,0.001723,0.001560 |
| pp_c | 0.000204,0.000175,0.000203,0.000175,0.000204,0.000176,0.000204 | 0.000147,0.000155,0.000155,0.000142,0.000152,0.000145,0.000148 | 0.000147,0.000155,0.000155,0.000142,0.000152,0.000145,0.000148 |

## Performance

| fixture | candidate | model | seconds | tracemalloc peak bytes |
|---|---|---|---:|---:|
| color_ramp_small | pp_a | v1 | 0.0280 | 24087264 |
| color_ramp_small | pp_a | v2 | 0.0247 | 17861480 |
| color_ramp_small | pp_a | v3 | 0.0241 | 17696184 |
| color_ramp_small | pp_b | v1 | 0.0377 | 24087264 |
| color_ramp_small | pp_b | v2 | 0.0298 | 22284544 |
| color_ramp_small | pp_b | v3 | 0.0304 | 26296176 |
| color_ramp_small | pp_c | v1 | 0.0196 | 17762448 |
| color_ramp_small | pp_c | v2 | 0.0265 | 22350448 |
| color_ramp_small | pp_c | v3 | 0.0266 | 27526640 |
| color_ramp_large | pp_a | v1 | 0.2829 | 231213792 |
| color_ramp_large | pp_a | v2 | 0.2495 | 171445096 |
| color_ramp_large | pp_a | v3 | 0.2397 | 169870752 |
| color_ramp_large | pp_b | v1 | 0.3656 | 231213792 |
| color_ramp_large | pp_b | v2 | 0.2853 | 213911808 |
| color_ramp_large | pp_b | v3 | 0.3018 | 251767664 |
| color_ramp_large | pp_c | v1 | 0.1956 | 169937040 |
| color_ramp_large | pp_c | v2 | 0.2599 | 213977712 |
| color_ramp_large | pp_c | v3 | 0.2692 | 264242672 |
| alpha_boundary_small | pp_a | v1 | 0.0279 | 24087264 |
| alpha_boundary_small | pp_a | v2 | 0.0250 | 17861480 |
| alpha_boundary_small | pp_a | v3 | 0.0242 | 17696160 |
| alpha_boundary_small | pp_b | v1 | 0.0351 | 24087264 |
| alpha_boundary_small | pp_b | v2 | 0.0277 | 22284544 |
| alpha_boundary_small | pp_b | v3 | 0.0291 | 26296176 |
| alpha_boundary_small | pp_c | v1 | 0.0191 | 17762448 |
| alpha_boundary_small | pp_c | v2 | 0.0254 | 22350448 |
| alpha_boundary_small | pp_c | v3 | 0.0265 | 27526640 |
