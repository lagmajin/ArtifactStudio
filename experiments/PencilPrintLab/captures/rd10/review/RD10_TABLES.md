# RD10 analytic threshold tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Lock-aligned MAE on the integer sequence (1 px/frame, blur-free reference)

`inner` excludes the replicate-fill band of the shifted reference (as wide as the largest motion in the sequence); the filled columns disagree by construction.

| variant | aligned MAE steps | mean | inner mean |
|---|---|---:|---:|
| dither_bayer_screen | 0.198095,0.042101,0.197607,0.000793,0.197257,0.042573,0.196762 | 0.125027 | 0.12508633 |
| dither_bayer_content | 0.000360,0.000401,0.000793,0.000793,0.001154,0.001194,0.001587 | 0.000898 | 0.00000000 |
| dither_ign_screen | 0.214629,0.074605,0.194428,0.145957,0.139098,0.197715,0.067978 | 0.147773 | 0.14779020 |
| dither_ign_content | 0.000423,0.000574,0.000954,0.001243,0.001514,0.001898,0.002035 | 0.001234 | 0.00000000 |
| dither_r2_content | 0.000321,0.000741,0.001074,0.001103,0.001406,0.001815,0.002163 | 0.001232 | 0.00000000 |
| full_bayer_screen | 0.253339,0.043059,0.252749,0.026222,0.252209,0.046608,0.251557 | 0.160821 | 0.16117400 |
| full_bayer_content | 0.006594,0.012663,0.018531,0.023275,0.027468,0.030056,0.031815 | 0.021486 | 0.02064237 |
| full_ign_content | 0.005362,0.010389,0.015264,0.019567,0.023553,0.027054,0.029689 | 0.018697 | 0.01752554 |
| full_r2_content | 0.007876,0.015324,0.021828,0.026533,0.030155,0.032178,0.032430 | 0.023761 | 0.02273251 |

## Lock-aligned MAE on the half-pixel sequence (0.5 px/frame, blur-limited reference)

| variant | aligned MAE steps | mean | inner mean |
|---|---|---:|---:|
| dither_bayer_screen | 0.099591,0.198095,0.120118,0.042101,0.119764,0.197607,0.099740 | 0.125288 | 0.12541534 |
| dither_bayer_content | 0.099262,0.000360,0.099295,0.000401,0.099305,0.000793,0.099286 | 0.056958 | 0.05664087 |
| dither_ign_screen | 0.107868,0.214629,0.144561,0.074605,0.134548,0.194428,0.170154 | 0.148685 | 0.14886461 |
| dither_ign_content | 0.142719,0.000423,0.142824,0.000574,0.142671,0.000954,0.142801 | 0.081852 | 0.08153372 |
| dither_r2_content | 0.207649,0.000321,0.207393,0.000741,0.207123,0.001074,0.207091 | 0.118770 | 0.11857858 |
| full_bayer_screen | 0.127389,0.253339,0.148175,0.043059,0.147913,0.252749,0.139521 | 0.158878 | 0.15917733 |
| full_bayer_content | 0.126912,0.006594,0.126809,0.012663,0.126710,0.018531,0.126679 | 0.077843 | 0.07775279 |
| full_ign_content | 0.143908,0.005362,0.144046,0.010389,0.143892,0.015264,0.144121 | 0.086712 | 0.08627783 |
| full_r2_content | 0.233778,0.007876,0.233389,0.015324,0.233297,0.021828,0.233255 | 0.139821 | 0.13966184 |

## Per-step MAE and flicker proxy (half-pixel sequence)

| variant | step MAE | step std | static std |
|---|---|---:|---:|
| dither_bayer_screen | 0.001155,0.001113,0.001099,0.001138,0.001179,0.001138,0.001050 | 0.000039 | 0.000374 |
| dither_bayer_content | 0.103332,0.095222,0.103311,0.095224,0.103309,0.095204,0.103295 | 0.004006 | 0.002142 |
| dither_ign_screen | 0.001144,0.001128,0.001181,0.001118,0.001220,0.001088,0.001115 | 0.000041 | 0.000293 |
| dither_ign_content | 0.142712,0.142829,0.142704,0.142827,0.142696,0.142805,0.142704 | 0.000058 | 0.000279 |
| dither_r2_content | 0.207728,0.207654,0.207709,0.207624,0.207691,0.207604,0.207648 | 0.000042 | 0.000501 |
| full_bayer_screen | 0.001537,0.001480,0.001560,0.001636,0.001546,0.001577,0.001611 | 0.000047 | 0.000406 |
| full_bayer_content | 0.124162,0.129884,0.123773,0.130005,0.123964,0.129698,0.123796 | 0.002942 | 0.000778 |
| full_ign_content | 0.143981,0.143923,0.144025,0.143950,0.143974,0.143987,0.144103 | 0.000054 | 0.000826 |
| full_r2_content | 0.233763,0.233643,0.233505,0.233676,0.233841,0.233894,0.233942 | 0.000143 | 0.001720 |

## Threshold contrast ratio at a 0.5 px/frame offset

| source | zero-offset std | contrast ratios (0.5 px steps) |
|---|---:|---|
| bayer | 0.288111 | 0.4728,1.0000,0.4728,1.0000,0.4728,1.0000,0.4728 |
| ign | 0.288588 | 1.0000,1.0000,1.0000,1.0000,1.0000,1.0000,1.0000 |
| r2 | 0.288676 | 1.0000,1.0000,1.0000,1.0000,1.0000,1.0000,1.0000 |

## Descriptor / absolute probe

| variant | descriptor delta | visible max | near max | near >1 | nonfinite |
|---|---:|---:|---:|---:|---:|
| dither_bayer_screen | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| dither_bayer_content | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| dither_ign_screen | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| dither_ign_content | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| dither_r2_content | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| full_bayer_screen | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| full_bayer_content | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| full_ign_content | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |
| full_r2_content | 5.96e-08 | 0.6667 | 0.6667 | 0 | 0 |

## Performance

| variant | median s | tracemalloc peak |
|---|---:|---:|
| dither_bayer_screen | 0.0555 | 30871024 |
| dither_bayer_content | 0.0691 | 33492736 |
| dither_ign_screen | 0.0616 | 30870864 |
| dither_ign_content | 0.0626 | 33492360 |
| dither_r2_content | 0.0596 | 33492360 |
| full_bayer_screen | 0.1033 | 39324088 |
| full_bayer_content | 0.0964 | 41945704 |
| full_ign_content | 0.0999 | 41945480 |
| full_r2_content | 0.0821 | 41945480 |
