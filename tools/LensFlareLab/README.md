# Lens Flare Lab

**最終更新:** 2026-10-27

Standalone CPU look-development program for testing a prescription-based lens
flare model without launching ArtifactStudio. It has no Qt, Diligent, or
ArtifactCore runtime dependency. It traces two-reflection ghost paths through
the paper's Angenieux double-Gauss prescription, evaluates Fresnel reflection
and refraction, clips the aperture and lens housing, approximates RGB glass
dispersion from Abbe numbers, rasterizes the ray grid, and adds a
Fraunhofer-diffraction kernel for the aperture.

The detector integrates positive luminance above the threshold over each
highlight and keeps its RGB chromaticity separate from that flux. By default,
that footprint is divided into 3x3 radiance bins; each occupied bin contributes
its own source direction to the ghost and diffraction calculations. This gives
an image-space finite-source approximation while preserving integrated source
flux. `--source-samples 1` selects the point-source baseline and
`--source-samples 9` selects the default finite-source mode. Rays are
sampled over the physical stop and traced backwards through the front group to
the entrance pupil, which gives coarse grids useful coverage at the stated
f-number. Reflected ghost paths and diffraction can be rendered independently
for evaluation. Paths that cross the aperture stop three times are omitted;
the Angenieux prescription then has 42 remaining surface pairs, matching the
paper's single-stop-crossing model.

## Build

The target is opt-in and is excluded from normal app builds. Configure the
repository with the option enabled, then build the target:

```powershell
cmake -S . -B build/lens-flare-lab -DARTIFACT_BUILD_LENS_FLARE_LAB=ON
cmake --build build/lens-flare-lab --target LensFlareLab
```

The target is not part of normal builds. It only needs a C++17 compiler.

## Inputs and outputs

The program reads binary PPM (`P6`, 8-bit RGB) and little-endian PFM (`PF`,
32-bit RGB float) images. PFM is recommended for HDR input and output. PPM
output applies a simple Reinhard display mapping and sRGB transfer function;
PFM preserves values above 1.0. Output alpha is not stored.

```text
LensFlareLab input.pfm output.pfm [--auto|--position X Y] [--threshold N]
               [--gain N] [--jacobian-cap N] [--ray-grid N] [--source-samples 1|9]
               [--dispersion cauchy|linear]
               [--aperture-radius MM]
               [--aperture-blades N] [--coating on|off]
               [--diffraction on|off] [--ghost-report]
               [--ghost-pair FIRST SECOND] [--jacobian-report] [--source-report]
```

With no options, an interactive command loop allows parameter changes, render,
save, and quit. `help` lists commands. Position coordinates are normalized
0..1; `--auto` detects up to eight strong highlight sources and traces each.
Parameter changes re-render
from the original source, so repeated iterations do not accumulate flare.
When options are supplied, the program renders once and exits; the CLI and
interactive paths support the same settings.

The default lens is the Angenieux 100 mm double-Gauss prescription tabulated
by Hullin et al. The f/11 stop radius is derived from focal length and f-number; the 15 mm air
gap between the stop and rear group is applied once. RGB dispersion defaults
to a Cauchy approximation parameterized by the prescription d-line index and
Abbe number, following the coefficient form used by the 2024 reference
implementation. `--dispersion linear` selects the prior linear Abbe
approximation for A/B comparisons. Neither model replaces measured Sellmeier
glass data. `--coating on` applies a
single MgF2-like quarter-wave layer at every refracting interface. These are
explicit model assumptions, not a claim that the original lens had that exact
coating stack. Gain is an exposure/look-development control and is not a
calibrated camera response. `--diffraction off` isolates reflected ghost paths
for image evaluation; `--coating off` provides an uncoated comparison.
`--ghost-report` ranks the 0-based reflecting-surface pairs by accumulated
image luminance and prints their sensor bounds. The visible-path count uses
only paths eligible under the single-stop-crossing rule. `--ghost-pair 9 8`
renders only the path that reflects at surface 9 and then surface 8, making a
single ghost easy to inspect. `--jacobian-cap` sets the maximum ray-grid
irradiance amplification (default 512, maximum 4096); `--jacobian-report`
reports how many triangles hit that cap. `--source-report` compares the
integrated highlight flux with the sum of the nine bin fluxes and reports how
many bins contain energy.

See [EVALUATION.md](EVALUATION.md) for paper-grounded evaluation images and
current model limits. Regenerate the chart with
`python tools/LensFlareLab/make_evaluation_chart.py`. To raise only the source
radiances for a high-dynamic-range visibility check, use
`python tools/LensFlareLab/make_evaluation_chart.py --output build/high-exposure.pfm --light-scale 4096`.
Use `--single-light-position U V` to generate off-axis charts for field sweeps.

## Core/app integration path

The current editor effect is still an empirical screen-space implementation.
Do not treat the lab output as already integrated. Promotion to Core/App needs
a cold-path cache for ray-transfer grids and an allocation-bounded render path;
tracing every prescription path for every frame is too slow for the editor.
