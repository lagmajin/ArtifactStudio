# RD6 deep temporal tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Global MAE / aligned MAE / seek MAE per frame

| model | frame | step MAE | aligned MAE | seek MAE |
|---|---:|---:|---:|---:|
| binary | 1 | 0.001544 | 0.225375 | 0.001544 |
| binary | 2 | 0.001573 | 0.449168 | 0.002507 |
| binary | 3 | 0.001727 | 0.259946 | 0.003554 |
| binary | 4 | 0.001652 | 0.070351 | 0.004628 |
| binary | 5 | 0.001605 | 0.259388 | 0.005587 |
| binary | 6 | 0.001560 | 0.448617 | 0.006492 |
| binary | 7 | 0.001414 | 0.234857 | 0.007450 |
| multi4 | 1 | 0.001442 | 0.127477 | 0.001442 |
| multi4 | 2 | 0.001541 | 0.253553 | 0.002373 |
| multi4 | 3 | 0.001596 | 0.148204 | 0.003476 |
| multi4 | 4 | 0.001577 | 0.042976 | 0.004555 |
| multi4 | 5 | 0.001409 | 0.147923 | 0.005429 |
| multi4 | 6 | 0.001617 | 0.252865 | 0.006434 |
| multi4 | 7 | 0.001558 | 0.138021 | 0.007405 |
| multi5 | 1 | 0.001543 | 0.054192 | 0.001543 |
| multi5 | 2 | 0.001517 | 0.106873 | 0.002419 |
| multi5 | 3 | 0.001600 | 0.068895 | 0.003536 |
| multi5 | 4 | 0.001636 | 0.030811 | 0.004626 |
| multi5 | 5 | 0.001496 | 0.068758 | 0.005575 |
| multi5 | 6 | 0.001510 | 0.106700 | 0.006554 |
| multi5 | 7 | 0.001603 | 0.062750 | 0.007546 |

## Flicker proxy and static-region temporal std

| model | step std | static std |
|---|---:|---:|
| binary | 0.000090 | 0.000779 |
| multi4 | 0.000073 | 0.000426 |
| multi5 | 0.000051 | 0.000243 |

## Per-frame performance

| model | median s | max s | tracemalloc peak max |
|---|---:|---:|---:|
| binary | 0.0310 | 0.0312 | 26296536 |
| multi4 | 0.0393 | 0.0417 | 36060776 |
| multi5 | 0.0389 | 0.0476 | 36060776 |
