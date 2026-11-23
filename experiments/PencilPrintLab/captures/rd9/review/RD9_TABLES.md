# RD9 integer-shift reference tables (blank conclusion)

No gate is set; no pass/fail or quality ranking is recorded.

## Integer-aligned MAE per frame (pure dither-following residual)

| variant | aligned MAE steps |
|---|---|
| screen | 0.149342,0.149287,0.149310,0.149332,0.149260,0.149233,0.149235 |
| content_nearest | 0.149314,0.149297,0.149328,0.149271,0.149275,0.149261,0.149234 |
| content_interp | 0.149317,0.149322,0.149335,0.149332,0.149243,0.149238,0.149231 |

## Per-step global MAE and flicker proxy

| variant | step MAE | step std |
|---|---|---:|
| screen | 0.002345,0.002570,0.002520,0.002582,0.002388,0.002548,0.002396 | 0.000091 |
| content_nearest | 0.254047,0.253777,0.253662,0.253613,0.253632,0.253577,0.253369 | 0.000192 |
| content_interp | 0.253794,0.253806,0.253833,0.253827,0.253685,0.253585,0.253547 | 0.000111 |

## Descriptor / absolute probe

| variant | descriptor delta | near >1 | nonfinite |
|---|---:|---:|---:|
| screen | 5.96e-08 | 0 | 0 |
| content_nearest | 5.96e-08 | 0 | 0 |
| content_interp | 5.96e-08 | 0 | 0 |

## Performance

| variant | median s | tracemalloc peak |
|---|---:|---:|
| screen | 0.0481 | 39324448 |
| content_nearest | 0.0453 | 38668856 |
| content_interp | 0.0508 | 39324440 |
