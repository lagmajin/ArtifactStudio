# RD5 dither sweep tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Alpha fixtures

| fixture | candidate | descriptor delta | visible max | near max | near >1 | near >10 | nonfinite |
|---|---|---:|---:|---:|---:|---:|---:|
| alpha_boundary_small | binary | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | multi3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_small | multi4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 | 0 |
| alpha_boundary_small | multi5 | 5.96e-08 | 0.75 | 0.75 | 0 | 0 | 0 |
| alpha_boundary_small | multi6 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_small | multi8 | 5.96e-08 | 0.7143 | 0.7143 | 0 | 0 | 0 |
| alpha_boundary_large | binary | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | multi3 | 0 | 1 | 1 | 0 | 0 | 0 |
| alpha_boundary_large | multi4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 | 0 |
| alpha_boundary_large | multi5 | 5.96e-08 | 0.75 | 0.75 | 0 | 0 | 0 |
| alpha_boundary_large | multi6 | 5.96e-08 | 0.8 | 0.8 | 0 | 0 | 0 |
| alpha_boundary_large | multi8 | 5.96e-08 | 0.7143 | 0.7143 | 0 | 0 | 0 |

## Opaque difference from binary baseline

| fixture | candidate | max delta | MAE |
|---|---|---:|---:|
| color_ramp_small | binary | 0 | 0.000000 |
| color_ramp_small | multi3 | 1 | 0.203538 |
| color_ramp_small | multi4 | 0.6667 | 0.248843 |
| color_ramp_small | multi5 | 0.75 | 0.272863 |
| color_ramp_small | multi6 | 0.8 | 0.287279 |
| color_ramp_small | multi8 | 0.8571 | 0.281249 |
| thin_lines_small | binary | 0 | 0.000000 |
| thin_lines_small | multi3 | 1 | 0.231977 |
| thin_lines_small | multi4 | 1 | 0.308672 |
| thin_lines_small | multi5 | 0.75 | 0.345957 |
| thin_lines_small | multi6 | 0.8 | 0.317454 |
| thin_lines_small | multi8 | 0.8571 | 0.328240 |
| flat_highlight_small | binary | 0 | 0.000000 |
| flat_highlight_small | multi3 | 1.232 | 0.032131 |
| flat_highlight_small | multi4 | 0.9143 | 0.148271 |
| flat_highlight_small | multi5 | 0.5498 | 0.198803 |
| flat_highlight_small | multi6 | 0.8797 | 0.208979 |
| flat_highlight_small | multi8 | 0.9426 | 0.221713 |
| color_ramp_large | binary | 0 | 0.000000 |
| color_ramp_large | multi3 | 1 | 0.206459 |
| color_ramp_large | multi4 | 0.6667 | 0.251074 |
| color_ramp_large | multi5 | 0.75 | 0.275545 |
| color_ramp_large | multi6 | 0.8 | 0.290116 |
| color_ramp_large | multi8 | 0.8571 | 0.283944 |
| thin_lines_large | binary | 0 | 0.000000 |
| thin_lines_large | multi3 | 1 | 0.234213 |
| thin_lines_large | multi4 | 1 | 0.311684 |
| thin_lines_large | multi5 | 0.75 | 0.349494 |
| thin_lines_large | multi6 | 0.8 | 0.320411 |
| thin_lines_large | multi8 | 0.8571 | 0.331155 |
| flat_highlight_large | binary | 0 | 0.000000 |
| flat_highlight_large | multi3 | 1.273 | 0.032558 |
| flat_highlight_large | multi4 | 0.9481 | 0.150715 |
| flat_highlight_large | multi5 | 0.5502 | 0.201718 |
| flat_highlight_large | multi6 | 0.8804 | 0.210824 |
| flat_highlight_large | multi8 | 0.9432 | 0.223858 |

## Temporal global MAE per step

| candidate | steps |
|---|---|
| binary | 0.001544,0.001573,0.001727,0.001652,0.001605,0.001560,0.001414 |
| multi3 | 0.001473,0.001570,0.001673,0.001619,0.001411,0.001583,0.001574 |
| multi4 | 0.001442,0.001541,0.001596,0.001577,0.001409,0.001617,0.001558 |
| multi5 | 0.001543,0.001517,0.001600,0.001636,0.001496,0.001510,0.001603 |
| multi6 | 0.001532,0.001631,0.001569,0.001568,0.001584,0.001557,0.001549 |
| multi8 | 0.001533,0.001570,0.001547,0.001494,0.001531,0.001549,0.001569 |

## Performance (3 runs)

| fixture | candidate | median s | min s | max s | tracemalloc peak max |
|---|---|---:|---:|---:|---:|
| color_ramp_small | binary | 0.0391 | 0.0366 | 0.0396 | 26296208 |
| color_ramp_small | multi3 | 0.0417 | 0.0383 | 0.0436 | 36060808 |
| color_ramp_small | multi4 | 0.0461 | 0.0379 | 0.0476 | 36060776 |
| color_ramp_small | multi5 | 0.0391 | 0.0386 | 0.0395 | 36060208 |
| color_ramp_small | multi6 | 0.0402 | 0.0380 | 0.0408 | 36060208 |
| color_ramp_small | multi8 | 0.0364 | 0.0362 | 0.0368 | 36060208 |
| color_ramp_large | binary | 0.3045 | 0.3042 | 0.3381 | 251767696 |
| color_ramp_large | multi3 | 0.3736 | 0.3711 | 0.3781 | 346073136 |
| color_ramp_large | multi4 | 0.3699 | 0.3694 | 0.3703 | 346073136 |
| color_ramp_large | multi5 | 0.3821 | 0.3802 | 0.3912 | 346073136 |
| color_ramp_large | multi6 | 0.3702 | 0.3697 | 0.3730 | 346073136 |
| color_ramp_large | multi8 | 0.3681 | 0.3679 | 0.3691 | 346073136 |
| alpha_boundary_small | binary | 0.0304 | 0.0298 | 0.0331 | 26296208 |
| alpha_boundary_small | multi3 | 0.0366 | 0.0361 | 0.0367 | 36060208 |
| alpha_boundary_small | multi4 | 0.0361 | 0.0358 | 0.0363 | 36060208 |
| alpha_boundary_small | multi5 | 0.0359 | 0.0357 | 0.0361 | 36060208 |
| alpha_boundary_small | multi6 | 0.0365 | 0.0363 | 0.0388 | 36060208 |
| alpha_boundary_small | multi8 | 0.0368 | 0.0367 | 0.0377 | 36060208 |
