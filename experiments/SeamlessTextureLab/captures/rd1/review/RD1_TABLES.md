# RD1 seamless candidate tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Seam energy ratio (1.0 means the border is indistinguishable from the interior)

| fixture | baseline | st_mirror | st_quilt | st_periodic |
|---|---:|---:|---:|---:|
| checker_small | 11.7418 | 6.6660 | 11.1193 | 5.0556 |
| radial_small | 10.2619 | 9.6365 | 10.5304 | 4.3241 |
| clouds_small | 20.3248 | 17.4737 | 18.7665 | 7.3793 |
| blobs_small | 10.9359 | 11.7962 | 10.6033 | 4.7488 |
| alpha_patch_small | nan | 1.0024 | 1.0556 | 0.7285 |
| checker_large | 28.4624 | 18.6787 | 26.4992 | 15.4945 |
| radial_large | 15.1395 | 14.5493 | 12.8823 | 12.0509 |
| clouds_large | 62.5953 | 52.7963 | 57.2088 | 20.4743 |
| blobs_large | 24.8380 | 26.5210 | 24.1901 | 13.1055 |
| alpha_patch_large | nan | 1.3711 | 1.0351 | 0.9836 |

## Luminance step across the tile border

| fixture | baseline | st_mirror | st_quilt | st_periodic |
|---|---:|---:|---:|---:|
| checker_small | 0.162914 | 0.053037 | 0.162260 | 0.028601 |
| radial_small | 0.055896 | 0.037635 | 0.099352 | 0.010364 |
| clouds_small | 0.204935 | 0.114779 | 0.204591 | 0.036357 |
| blobs_small | 0.110623 | 0.085706 | 0.110624 | 0.019831 |
| alpha_patch_small | 0.000000 | 0.003760 | 0.012354 | 0.000000 |
| checker_large | 0.186755 | 0.076593 | 0.186251 | 0.033052 |
| radial_large | 0.054411 | 0.037158 | 0.054755 | 0.009854 |
| clouds_large | 0.204632 | 0.114592 | 0.204545 | 0.036707 |
| blobs_large | 0.110539 | 0.086380 | 0.110151 | 0.019809 |
| alpha_patch_large | 0.000000 | 0.001969 | 0.005192 | 0.000000 |

## Boundary mode energy share

| fixture | baseline | st_mirror | st_quilt | st_periodic |
|---|---:|---:|---:|---:|
| checker_small | 0.0119 | 0.0103 | 0.0115 | 0.0106 |
| radial_small | 0.212 | 0.0788 | 0.195 | 0.208 |
| clouds_small | 0.0282 | 0.0259 | 0.0277 | 0.026 |
| blobs_small | 0.0341 | 0.0352 | 0.0339 | 0.0321 |
| alpha_patch_small | 0 | 0.123 | 0.197 | 1.89e-17 |
| checker_large | 0.0119 | 0.0096 | 0.0116 | 0.0113 |
| radial_large | 0.22 | 0.0884 | 0.218 | 0.219 |
| clouds_large | 0.0282 | 0.0259 | 0.028 | 0.0273 |
| blobs_large | 0.0339 | 0.0348 | 0.0335 | 0.0333 |
| alpha_patch_large | 0 | 0.122 | 0.197 | 6.01e-18 |

## Repetition peak ratio (duplicated motif)

| fixture | baseline | st_mirror | st_quilt | st_periodic |
|---|---:|---:|---:|---:|
| checker_small | 0.5057 | 0.5057 | 0.5047 | 0.5426 |
| radial_small | 0.9566 | 0.9566 | 0.9306 | 0.9589 |
| clouds_small | 0.5253 | 0.4136 | 0.5201 | 0.5663 |
| blobs_small | 0.4790 | 0.4750 | 0.4778 | 0.5399 |
| alpha_patch_small | nan | 0.9829 | 0.7285 | 0.0106 |
| checker_large | 0.4390 | 0.4390 | 0.4351 | 0.4544 |
| radial_large | 0.9523 | 0.9523 | 0.9442 | 0.9533 |
| clouds_large | 0.5255 | 0.4221 | 0.5242 | 0.5411 |
| blobs_large | 0.4686 | 0.4681 | 0.4689 | 0.4935 |
| alpha_patch_large | nan | 0.9877 | 0.7344 | 0.0038 |

## Descriptor / absolute probe

| candidate | descriptor delta | alpha delta | near >1 | nonfinite |
|---|---:|---:|---:|---:|
| st_mirror | 0.000951 | 0 | 0 | 0 |
| st_quilt | 6.06 | 0 | 0 | 0 |
| st_periodic | 54.4 | 0 | 0 | 0 |

## Performance (clouds_small)

| candidate | median s | tracemalloc peak |
|---|---:|---:|
| st_mirror | 0.0307 | 26215816 |
| st_quilt | 0.0765 | 15077192 |
| st_periodic | 0.0947 | 52439208 |
