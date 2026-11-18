# RD8 sub-pixel phase tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Alpha fixtures

| fixture | variant | descriptor delta | visible max | near max | near >1 | nonfinite |
|---|---|---:|---:|---:|---:|---:|
| alpha_boundary_small | nearest | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | interp | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | screen_interp | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | nearest | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | interp | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | screen_interp | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |

## Opaque max delta from nearest

| fixture | variant | max delta |
|---|---|---:|
| color_ramp_small | nearest | 0 |
| color_ramp_small | interp | 0.3333 |
| color_ramp_small | screen_interp | 0.3333 |
| thin_lines_small | nearest | 0 |
| thin_lines_small | interp | 0.3333 |
| thin_lines_small | screen_interp | 0.3333 |
| flat_highlight_small | nearest | 0 |
| flat_highlight_small | interp | 0.7331 |
| flat_highlight_small | screen_interp | 0.7331 |
| color_ramp_large | nearest | 0 |
| color_ramp_large | interp | 0.3333 |
| color_ramp_large | screen_interp | 0.3333 |
| thin_lines_large | nearest | 0 |
| thin_lines_large | interp | 0.3333 |
| thin_lines_large | screen_interp | 0.3333 |
| flat_highlight_large | nearest | 0 |
| flat_highlight_large | interp | 0.7336 |
| flat_highlight_large | screen_interp | 0.7336 |

## Temporal integer-aligned MAE per frame

| variant | aligned MAE steps |
|---|---|
| nearest | 0.127302,0.005339,0.129411,0.010223,0.131504,0.015226,0.134003 |
| interp | 0.126777,0.007928,0.126660,0.014846,0.126633,0.021394,0.126605 |
| screen_interp | 0.127477,0.253553,0.148204,0.042976,0.147923,0.252865,0.138021 |

## Performance (color_ramp_large)

| variant | median s | tracemalloc peak |
|---|---:|---:|
| nearest | 0.6415 | 371198320 |
| interp | 0.6351 | 377490576 |
| screen_interp | 0.6452 | 377489976 |
