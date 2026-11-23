# Burn Away effect lab

Image-evaluation harness for the `Burn Away` layer effect
(`Artifact.Effect.BurnAway`) and the ash / ember particle presets
(`ParticlePresets::ash`, `ParticlePresets::ember`).  It renders the shipped
effect over a synthetic paper surface, adds the ash and ember particles that
sit on the burn front, and writes PNG frames plus alpha statistics.

The lab is read-only with respect to product code: it links
`ArtifactEffectsStylize` and `ArtifactAppRuntime` and exercises the shipped
effect and particle APIs.

## Build

The lab is excluded from the default build because it is a research tool, not a
shipping target.

```powershell
cmake -S <repo> -B <tree> -DARTIFACT_BUILD_BURN_AWAY_LAB=ON
cmake --build <tree> --target ArtifactBurnAwayLab
```

## Run

```powershell
ArtifactBurnAwayLab --out captures/burn-away-20261119 --width 960 --height 540
```

Outputs (all under `--out`):

| File | Content |
| --- | --- |
| `source.png` | the untouched synthetic paper |
| `burn_pNN.png` | one frame per progress step (0, 10, 25, 40, 60, 80, 100 %) |
| `burn_pNN_particles.png` | the same frame with ash and embers on the burn front |
| `particles_only_pNN.png` | the particle frame alone, without the paper |
| stdout | `progress, mean_alpha, opaque_fraction, mean_luminance, max_luminance, front_y` |

The particle band uses the same normalised front geometry as the effect, so
`front_y` in the statistics is the row the ash and embers are emitted from.

## Note on the particle clock

The particle system advances by a bounded number of fixed sub-steps per
update, so a single large time delta only simulates a fraction of a second.
The lab therefore steps the simulation in 1/60 s increments instead of
jumping to the target time, which is what the player does as well.

## GPU parity

```powershell
ArtifactBurnAwayLab --gpu-parity 960 540 <diff-image.png>
```

The mode dispatches the shader that the effect itself registers
(`findGpuGenericShader("burn_away")`) on the GPU with the same prelude,
cbuffer layout and sixteen parameters the composition layer pipeline uses,
then compares the result with the CPU reference implementation:

```
gpu parity: mean_abs_error=0.000000 rmse=0.000001 max_difference=0.000084
            differing_pixels=0/518400 (0.0000%)
```

## Comparing two PNG files

```powershell
ArtifactBurnAwayLab --compare <a.png> <b.png> [diff-image.png]
```

Prints the mean absolute error, RMSE, maximum difference, the share of
differing pixels and the worst pixel position, and writes the difference
image when a path is given.

## Conventions the lab assumes

- The layer surface buffer stores **B, G, R, A** (`ImageF32x4_RGBA`).
- The layer surface contract is **straight alpha**, so the composite is
  `colour * alpha + background * (1 - alpha)`.
- Colours are written as 8-bit sRGB PNG; the lab converts from the linear
  working space with the standard sRGB transfer function.

## Evidence

The evaluation record, thresholds and the results of the first run live in
[`docs/analysis/BURN_AWAY_EFFECT_RD_PLAN_2026-11-19.md`](../../docs/analysis/BURN_AWAY_EFFECT_RD_PLAN_2026-11-19.md).
