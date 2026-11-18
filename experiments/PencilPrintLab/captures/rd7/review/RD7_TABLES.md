# RD7 content-locked dither tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Alpha fixtures

| fixture | variant | model | descriptor delta | visible max | near max | near >1 | nonfinite |
|---|---|---|---:|---:|---:|---:|---:|
| alpha_boundary_small | screen_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | screen_multi4 | v5 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | content_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | content_multi4 | v5 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | global_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_small | global_multi4 | v5 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | screen_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | screen_multi4 | v5 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | content_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | content_multi4 | v5 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | global_multi4 | v4 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| alpha_boundary_large | global_multi4 | v5 | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |

## Opaque v4/v5 max RGB delta

| fixture | variant | max delta |
|---|---|---:|
| color_ramp_small | screen_multi4 | 0 |
| color_ramp_small | content_multi4 | 0 |
| color_ramp_small | global_multi4 | 0 |
| thin_lines_small | screen_multi4 | 0 |
| thin_lines_small | content_multi4 | 0 |
| thin_lines_small | global_multi4 | 0 |
| flat_highlight_small | screen_multi4 | 0 |
| flat_highlight_small | content_multi4 | 0 |
| flat_highlight_small | global_multi4 | 0 |
| color_ramp_large | screen_multi4 | 0 |
| color_ramp_large | content_multi4 | 0 |
| color_ramp_large | global_multi4 | 0 |
| thin_lines_large | screen_multi4 | 0 |
| thin_lines_large | content_multi4 | 0 |
| thin_lines_large | global_multi4 | 0 |
| flat_highlight_large | screen_multi4 | 0 |
| flat_highlight_large | content_multi4 | 0 |
| flat_highlight_large | global_multi4 | 0 |

## Temporal integer-aligned MAE per frame

| variant | aligned MAE steps |
|---|---|
| screen_multi4 | 0.127302,0.253170,0.148070,0.042970,0.147811,0.252625,0.135975 |
| content_multi4 | 0.130366,0.007928,0.134126,0.014846,0.136899,0.021394,0.139512 |
| global_multi4 | 0.127477,0.253553,0.148204,0.042976,0.147923,0.252865,0.138021 |

## Performance

| fixture | variant | model | seconds | tracemalloc peak |
|---|---|---|---:|---:|
| color_ramp_small | screen_multi4 | v4 | 0.0449 | 36060776 |
| color_ramp_small | screen_multi4 | v5 | 0.0552 | 37357544 |
| color_ramp_small | content_multi4 | v4 | 0.0482 | 36060152 |
| color_ramp_small | content_multi4 | v5 | 0.0506 | 37357544 |
| color_ramp_small | global_multi4 | v4 | 0.0479 | 36060152 |
| color_ramp_small | global_multi4 | v5 | 0.0508 | 37357544 |
| color_ramp_large | screen_multi4 | v4 | 0.4337 | 346073080 |
| color_ramp_large | screen_multi4 | v5 | 0.4393 | 358615016 |
| color_ramp_large | content_multi4 | v4 | 0.3783 | 346073080 |
| color_ramp_large | content_multi4 | v5 | 0.5032 | 358615016 |
| color_ramp_large | global_multi4 | v4 | 0.5457 | 346073080 |
| color_ramp_large | global_multi4 | v5 | 0.4925 | 358615016 |
| alpha_boundary_small | screen_multi4 | v4 | 0.0484 | 36060152 |
| alpha_boundary_small | screen_multi4 | v5 | 0.0553 | 37357544 |
| alpha_boundary_small | content_multi4 | v4 | 0.0532 | 36060152 |
| alpha_boundary_small | content_multi4 | v5 | 0.0493 | 37357544 |
| alpha_boundary_small | global_multi4 | v4 | 0.0385 | 36060152 |
| alpha_boundary_small | global_multi4 | v5 | 0.0518 | 37357544 |
