# RD4 multi-dither comparison tables (blank conclusion)

No gate is set; no pass/fail or ranking is recorded.

## Alpha fixtures: descriptor delta and absolute probes

| fixture | candidate | model | descriptor delta | visible max | near max | near >1 | near >10 | nonfinite |
|---|---|---|---:|---:|---:|---:|---:|---:|
| alpha_boundary_small | pp_b_binary | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b_binary | v4 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b_multi4 | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b_multi8 | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | pp_b_multi8 | v4 | 5.96e-08 | 0.7143 | 0.7143 | 0 | 0 | 0 |
| alpha_boundary_small | pp_a | v3 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_small | pp_a | v4 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v3 | 5.96e-08 | 0.6 | 0.4 | 0 | 0 | 0 |
| alpha_boundary_small | pp_c | v4 | 5.96e-08 | 0.6 | 0.4 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b_binary | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b_binary | v4 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b_multi4 | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b_multi8 | v3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | pp_b_multi8 | v4 | 5.96e-08 | 0.7143 | 0.7143 | 0 | 0 | 0 |
| alpha_boundary_large | pp_a | v3 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_large | pp_a | v4 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v3 | 5.96e-08 | 0.6 | 0.4 | 0 | 0 | 0 |
| alpha_boundary_large | pp_c | v4 | 5.96e-08 | 0.6 | 0.4 | 0 | 0 | 0 |

## Opaque v3/v4 max RGB delta

| fixture | candidate | max delta |
|---|---|---:|
| color_ramp_small | pp_b_binary | 0 |
| color_ramp_small | pp_b_multi4 | 0.6667 |
| color_ramp_small | pp_b_multi8 | 0.8571 |
| color_ramp_small | pp_a | 0 |
| color_ramp_small | pp_c | 0 |
| thin_lines_small | pp_b_binary | 0 |
| thin_lines_small | pp_b_multi4 | 0.6667 |
| thin_lines_small | pp_b_multi8 | 0.8571 |
| thin_lines_small | pp_a | 0 |
| thin_lines_small | pp_c | 0 |
| flat_highlight_small | pp_b_binary | 0 |
| flat_highlight_small | pp_b_multi4 | 0.7331 |
| flat_highlight_small | pp_b_multi8 | 0.9426 |
| flat_highlight_small | pp_a | 0 |
| flat_highlight_small | pp_c | 0 |
| color_ramp_large | pp_b_binary | 0 |
| color_ramp_large | pp_b_multi4 | 0.6667 |
| color_ramp_large | pp_b_multi8 | 0.8571 |
| color_ramp_large | pp_a | 0 |
| color_ramp_large | pp_c | 0 |
| thin_lines_large | pp_b_binary | 0 |
| thin_lines_large | pp_b_multi4 | 0.6667 |
| thin_lines_large | pp_b_multi8 | 0.8571 |
| thin_lines_large | pp_a | 0 |
| thin_lines_large | pp_c | 0 |
| flat_highlight_large | pp_b_binary | 0 |
| flat_highlight_large | pp_b_multi4 | 0.7336 |
| flat_highlight_large | pp_b_multi8 | 0.9432 |
| flat_highlight_large | pp_a | 0 |
| flat_highlight_large | pp_c | 0 |

## Temporal global MAE per step (v3 / v4)

| candidate | v3 | v4 |
|---|---|---|
| pp_b_binary | 0.001544,0.001573,0.001727,0.001652,0.001605,0.001560,0.001414 | 0.001544,0.001573,0.001727,0.001652,0.001605,0.001560,0.001414 |
| pp_b_multi4 | 0.001624,0.001489,0.001815,0.001707,0.001542,0.001723,0.001560 | 0.001537,0.001480,0.001560,0.001636,0.001546,0.001577,0.001611 |
| pp_b_multi8 | 0.001617,0.001444,0.001742,0.001540,0.001489,0.001493,0.001361 | 0.001533,0.001570,0.001547,0.001494,0.001531,0.001549,0.001569 |
| pp_a | 0.000436,0.000510,0.000458,0.000521,0.000463,0.000522,0.000464 | 0.000436,0.000510,0.000458,0.000521,0.000463,0.000522,0.000464 |
| pp_c | 0.000151,0.000142,0.000156,0.000138,0.000148,0.000147,0.000156 | 0.000151,0.000142,0.000156,0.000138,0.000148,0.000147,0.000156 |

## Performance

| fixture | candidate | model | seconds | tracemalloc peak bytes |
|---|---|---|---:|---:|
| color_ramp_small | pp_b_binary | v3 | 0.0414 | 26296360 |
| color_ramp_small | pp_b_binary | v4 | 0.0395 | 26296176 |
| color_ramp_small | pp_b_multi4 | v3 | 0.0360 | 26296448 |
| color_ramp_small | pp_b_multi4 | v4 | 0.0402 | 36060176 |
| color_ramp_small | pp_b_multi8 | v3 | 0.0328 | 26296448 |
| color_ramp_small | pp_b_multi8 | v4 | 0.0410 | 36060176 |
| color_ramp_small | pp_a | v3 | 0.0263 | 17696432 |
| color_ramp_small | pp_a | v4 | 0.0263 | 17696160 |
| color_ramp_small | pp_c | v3 | 0.0288 | 27526912 |
| color_ramp_small | pp_c | v4 | 0.0300 | 27526640 |
| color_ramp_large | pp_b_binary | v3 | 0.3602 | 251767848 |
| color_ramp_large | pp_b_binary | v4 | 0.3918 | 251767664 |
| color_ramp_large | pp_b_multi4 | v3 | 0.3811 | 251767936 |
| color_ramp_large | pp_b_multi4 | v4 | 0.4857 | 346073104 |
| color_ramp_large | pp_b_multi8 | v3 | 0.3411 | 251767936 |
| color_ramp_large | pp_b_multi8 | v4 | 0.4604 | 346073104 |
| color_ramp_large | pp_a | v3 | 0.3136 | 169871024 |
| color_ramp_large | pp_a | v4 | 0.3071 | 169870752 |
| color_ramp_large | pp_c | v3 | 0.3023 | 264242944 |
| color_ramp_large | pp_c | v4 | 0.3225 | 264242672 |
| alpha_boundary_small | pp_b_binary | v3 | 0.0334 | 26296360 |
| alpha_boundary_small | pp_b_binary | v4 | 0.0335 | 26296176 |
| alpha_boundary_small | pp_b_multi4 | v3 | 0.0313 | 26296448 |
| alpha_boundary_small | pp_b_multi4 | v4 | 0.0387 | 36060176 |
| alpha_boundary_small | pp_b_multi8 | v3 | 0.0394 | 26296448 |
| alpha_boundary_small | pp_b_multi8 | v4 | 0.0382 | 36060176 |
| alpha_boundary_small | pp_a | v3 | 0.0268 | 17696432 |
| alpha_boundary_small | pp_a | v4 | 0.0286 | 17696160 |
| alpha_boundary_small | pp_c | v3 | 0.0310 | 27526912 |
| alpha_boundary_small | pp_c | v4 | 0.0320 | 27526640 |
