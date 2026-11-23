# RD0 tables (descriptive; gates unset)

数値は記述的 baseline である。合否判定・順位付けは行っていない。

## 入力と control の分布（small 解像度）

| fixture | control | luma mean | luma std | srgb8 luma mean | alpha mean |
|---|---|---|---|---|---|
| color_ramp_small | baseline_raw | 0.267146 | 0.197853 | 132.194 | 1.000000 |
| color_ramp_small | control_lowpass | 0.267146 | 0.191765 | 132.862 | 1.000000 |
| color_ramp_small | control_detail_only | 0.004170 | 0.022435 | 4.382 | 1.000000 |
| thin_lines_small | baseline_raw | 0.275504 | 0.127514 | 141.115 | 1.000000 |
| thin_lines_small | control_lowpass | 0.275504 | 0.087802 | 141.937 | 1.000000 |
| thin_lines_small | control_detail_only | 0.010399 | 0.054309 | 6.261 | 1.000000 |
| texture_small | baseline_raw | 0.299070 | 0.010055 | 147.152 | 1.000000 |
| texture_small | control_lowpass | 0.299070 | 0.009567 | 147.158 | 1.000000 |
| texture_small | control_detail_only | 0.000393 | 0.000854 | 1.346 | 1.000000 |
| flat_highlight_small | baseline_raw | 0.323261 | 0.313935 | 123.633 | 1.000000 |
| flat_highlight_small | control_lowpass | 0.323261 | 0.308718 | 125.137 | 1.000000 |
| flat_highlight_small | control_detail_only | 0.004214 | 0.023952 | 4.706 | 1.000000 |
| alpha_boundary_small | baseline_raw | 0.306648 | 0.199921 | 139.820 | 0.476027 |
| alpha_boundary_small | control_lowpass | 515.718804 | 4117.818508 | 88.239 | 0.476027 |
| alpha_boundary_small | control_detail_only | 0.013334 | 0.060542 | 7.813 | 0.476027 |
| color_ramp_large | baseline_raw | 0.268428 | 0.199011 | 132.327 | 1.000000 |
| color_ramp_large | control_lowpass | 0.268428 | 0.197065 | 132.572 | 1.000000 |
| color_ramp_large | control_detail_only | 0.001383 | 0.012760 | 1.529 | 1.000000 |
| thin_lines_large | baseline_raw | 0.271225 | 0.115944 | 140.404 | 1.000000 |
| thin_lines_large | control_lowpass | 0.271225 | 0.099490 | 140.741 | 1.000000 |
| thin_lines_large | control_detail_only | 0.004271 | 0.027400 | 3.855 | 1.000000 |
| texture_large | baseline_raw | 0.299800 | 0.009808 | 147.313 | 1.000000 |
| texture_large | control_lowpass | 0.299800 | 0.009369 | 147.317 | 1.000000 |
| texture_large | control_detail_only | 0.000329 | 0.000624 | 1.075 | 1.000000 |
| flat_highlight_large | baseline_raw | 0.323074 | 0.313976 | 123.574 | 1.000000 |
| flat_highlight_large | control_lowpass | 0.323074 | 0.312454 | 124.090 | 1.000000 |
| flat_highlight_large | control_detail_only | 0.001492 | 0.013481 | 2.087 | 1.000000 |
| alpha_boundary_large | baseline_raw | 0.306971 | 0.199882 | 139.889 | 0.474885 |
| alpha_boundary_large | control_lowpass | 169.014537 | 2330.819027 | 79.446 | 0.474885 |
| alpha_boundary_large | control_detail_only | 0.013652 | 0.062252 | 7.373 | 0.474885 |

## 灰色ランプの段差（入力 vs control）

| fixture | control | step mean | step std | step min | step max | monotonic fraction |
|---|---|---|---|---|---|---|
| color_ramp_small | baseline_raw | 0.002093 | 0.000081 | 0.001859 | 0.002301 | 1.0000 |
| color_ramp_small | control_lowpass | 0.001224 | 0.008073 | -0.126576 | 0.002076 | 0.9791 |
| color_ramp_small | control_detail_only | 0.000712 | 0.007899 | -0.000151 | 0.128761 | 0.7395 |
| color_ramp_large | baseline_raw | 0.000698 | 0.000042 | 0.000568 | 0.000838 | 1.0000 |
| color_ramp_large | control_lowpass | 0.000415 | 0.004718 | -0.129084 | 0.000710 | 0.9915 |
| color_ramp_large | control_detail_only | 0.000232 | 0.004612 | -0.000076 | 0.129811 | 0.5558 |

## 局所コントラスト（平坦部 std / 忙丙部 std）

| fixture | control | pair | flat std | busy std | ratio |
|---|---|---|---|---|---|
| color_ramp_small | baseline_raw | flat_bg_vs_gray_ramp | 0.000000 | 0.260409 | 260409046.711 |
| color_ramp_small | control_lowpass | flat_bg_vs_gray_ramp | 0.000000 | 0.254764 | 254764470.339 |
| color_ramp_small | control_detail_only | flat_bg_vs_gray_ramp | 0.000000 | 0.032541 | 32541449.774 |
| texture_small | baseline_raw | flat_region_vs_texture_region | 0.000446 | 0.016512 | 37.024 |
| texture_small | control_lowpass | flat_region_vs_texture_region | 0.000063 | 0.015676 | 248.767 |
| texture_small | control_detail_only | flat_region_vs_texture_region | 0.000254 | 0.001129 | 4.450 |
| flat_highlight_small | baseline_raw | flat_low_vs_highlight | 0.000599 | 0.517220 | 862.765 |
| flat_highlight_small | control_lowpass | flat_low_vs_highlight | 0.000082 | 0.502455 | 6148.111 |
| flat_highlight_small | control_detail_only | flat_low_vs_highlight | 0.000341 | 0.017699 | 51.872 |
| color_ramp_large | baseline_raw | flat_bg_vs_gray_ramp | 0.000000 | 0.260008 | 260007527.331 |
| color_ramp_large | control_lowpass | flat_bg_vs_gray_ramp | 0.000000 | 0.258211 | 258211023.479 |
| color_ramp_large | control_detail_only | flat_bg_vs_gray_ramp | 0.000000 | 0.018507 | 18506802.360 |
| texture_large | baseline_raw | flat_region_vs_texture_region | 0.000448 | 0.016217 | 36.214 |
| texture_large | control_lowpass | flat_region_vs_texture_region | 0.000063 | 0.015490 | 246.149 |
| texture_large | control_detail_only | flat_region_vs_texture_region | 0.000254 | 0.000838 | 3.307 |
| flat_highlight_large | baseline_raw | flat_low_vs_highlight | 0.000600 | 0.515958 | 859.353 |
| flat_highlight_large | control_lowpass | flat_low_vs_highlight | 0.000086 | 0.514242 | 5966.425 |
| flat_highlight_large | control_detail_only | flat_low_vs_highlight | 0.000340 | 0.006541 | 19.251 |
