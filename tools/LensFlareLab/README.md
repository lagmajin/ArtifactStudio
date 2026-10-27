# Lens Flare Lab

**最終更新:** 2026-10-27

Standalone CPU look-development program for testing a prescription-based lens
flare model without launching ArtifactStudio. It has no Qt, Diligent, or
ArtifactCore runtime dependency. It traces two-reflection ghost paths through
the paper's Angenieux double-Gauss prescription, evaluates Fresnel reflection
and refraction, clips the aperture and lens housing, approximates RGB glass
dispersion from Abbe numbers, integrates mapped ray triangles over sensor-pixel
areas, and convolves detected highlights with a Fraunhofer-diffraction PSF for
the aperture.

The detector integrates positive luminance above the threshold over each
highlight and keeps its RGB chromaticity separate from that flux. Its footprint
is divided into radiance bins; each occupied bin contributes its own source
direction to the reflected-ghost calculation. Diffraction instead
convolves the original threshold-excess radiance footprint with the aperture
PSF, replacing that source energy rather than adding a second copy. This keeps
the finite source's shape in the diffraction result. `--source-samples 1`
selects the point-source baseline, `--source-samples 9` selects the default
3x3 finite-source ghost mode, and odd-square counts through `225` select grids
up to 15x15 for sampling-convergence comparisons (for example, `49`, `81`,
`121`). The larger grids are offline quality references because each occupied
bin traces the complete selected ghost path. Rays are sampled over the physical stop and
traced backwards through the front group to the entrance pupil, which gives
coarse grids useful coverage at the stated f-number. Reflected ghost paths and
diffraction can be rendered independently for evaluation. Paths that cross the
aperture stop three times are omitted; the Angenieux prescription then has 42
remaining surface pairs, matching the paper's single-stop-crossing model.

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
               [--gain N] [--jacobian-cap N] [--ray-grid N] [--source-samples 1|odd square <=225]
               [--adaptive-ray-refinement on|off] [--adaptive-ray-refinement-threshold PX]
               [--dispersion cauchy|linear] [--spectral-samples 3|7]
               [--rasterizer pixel-area|point-sample]
               [--aperture-radius MM]
               [--aperture-blades N] [--coating on|off]
               [--ghosts on|off] [--diffraction on|off]
               [--ghost-diffraction on|off] [--ghost-diffraction-distance-mm MM]
               [--ghost-diffraction-report]
               [--ghost-frft on|off] [--ghost-frft-resolution N] [--ghost-frft-report]
               [--ghost-frft-mask polygon|transport]
               [--fresnel-frft-order N] [--diffraction-report] [--ghost-report]
               [--ghost-pair FIRST SECOND] [--optical-path-report]
               [--optical-path-map CSV]
               [--jacobian-report] [--source-report]
```

With no options, an interactive command loop allows parameter changes, render,
save, and quit. `help` lists commands. Position coordinates are normalized
0..1; `--auto` detects up to eight strong highlight sources and traces each.
Parameter changes re-render
from the original source, so repeated iterations do not accumulate flare.
When options are supplied, the program renders once and exits; the CLI and
interactive paths support the same settings.

The default lens is the Angenieux 100 mm double-Gauss prescription tabulated
by Hullin et al. The f/11 stop radius is derived from focal length and f-number;
the 15 mm air gap between the stop and rear group is applied once. RGB dispersion defaults
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
`--ghosts off` isolates diffraction, and `--diffraction off` isolates reflected
ghosts.
Reflected ghosts use the original three wavelength samples by default.
`--spectral-samples 7` opts into a 430–610 nm piecewise-linear reconstruction
from input RGB and trapezoidal wavelength integration back to RGB. This is an
exploratory interpolation between the three existing RGB primaries, not a CIE
standard-observer conversion or measured source spectrum. It roughly doubles
CPU time for a traced ghost path and does not change the source Fraunhofer
starburst, which remains three-band.
`--rasterizer pixel-area` is the default. It clips each mapped ray triangle to
unit-area sensor pixels and integrates the linearly interpolated ray energy
inside each overlap. This conserves sampled ray flux when a triangle is smaller
than a pixel and reduces caustic aliasing as the ray grid changes.
`--rasterizer point-sample` retains the prior pixel-center rasterizer for A/B
comparisons. `--jacobian-cap` (default 512, maximum 4096) and
`--jacobian-report` apply to that legacy mode; pixel-area mode integrates the
finite sensor pixel footprint and does not apply the Jacobian cap.
`--ghost-report` ranks the 0-based reflecting-surface pairs by accumulated
image luminance and prints their sensor bounds. The visible-path count uses
only paths eligible under the single-stop-crossing rule. `--ghost-pair 9 8`
renders only the path that reflects at surface 9 and then surface 8, making a
single ghost easy to inspect. `--source-report` compares the integrated
highlight flux with the sum of the selected bin fluxes and reports how many bins
contain energy.

See [EVALUATION.md](EVALUATION.md) for paper-grounded evaluation images and
current model limits. `--diffraction-report` prints the RGB Fresnel/FrFT order
from Hullin et al.'s Eq. (5), using the current f-number and wavelengths.
The default order is the paper's heuristic; `--fresnel-frft-order N` overrides
it for A/B experiments. The separate `--ghost-diffraction on` option enables
scalar Fresnel propagation of each source-bin/path/wavelength ghost layer.
It converts the ray-rasterized irradiance to a zero-phase amplitude, applies
the paraxial transfer function
`exp(-i π λ z (fx² + fy²))` with a padded 2D FFT, then squares the propagated
amplitude. `--ghost-diffraction-distance-mm` sets the effective propagation
distance; zero (the default) uses the 100 mm effective focal length. A local
ROI extending four Fresnel scales is energy-normalized before clipping at the
sensor boundary. This is a look-development approximation: ray tracing does
not provide per-ray optical phase or a path-specific exit-pupil plane, and
this is not Hullin et al.'s FrFT implementation. The two-source Angenieux Fig. 10 approximation can be
regenerated with `python tools/LensFlareLab/make_fig10_comparison_chart.py`.
Regenerate the general evaluation chart with
`python tools/LensFlareLab/make_evaluation_chart.py`. To raise only the source
radiances for a high-dynamic-range visibility check, use
`python tools/LensFlareLab/make_evaluation_chart.py --output build/high-exposure.pfm --light-scale 4096`.
Use `--single-light-position U V` to generate off-axis charts for field sweeps;
add `--single-light-neutral` for equal-RGB point sources in dispersion
comparisons. `make_dispersion_comparison.py` builds a shared-exposure plate and
reports per-channel ghost flux, peak, centroid, and maximum RGB centroid spread
for repeatable A/B cases. Use `--model-labels` to name comparisons other than
Cauchy versus linear Abbe.
`python tools/LensFlareLab/make_source_sampling_comparison.py` creates an
image comparison plate and radiometric summary from the 1/9/25 source-sample
PFM renders.
`python tools/LensFlareLab/make_ray_grid_comparison.py` compares the same
source-sample mode across 64/96/128 pupil ray grids and reports in-frame
flux, peak, centroid, and image differences. Its `--source-samples` accepts
the same odd-square counts through 225.

`--ghost-frft on` is a separate look-development switch for the Hullin-style
aperture-coordinate texture. It records each successful ghost ray's stop
crossing, transforms a binary aperture with a separable, direct-kernel 2D FrFT,
and weights the ray grid by the resulting intensity. `--fresnel-frft-order`
chooses the order; otherwise the RGB orders use Eq. (5). `--ghost-frft-report`
prints the effective orders, texture and ray-map normalizers, and cold-path
precompute time. `--ghost-frft-resolution` sets the square texture resolution
(64–512, default 256) for convergence comparisons. The α=0 option is an exact geometric bypass. Orders below
0.01 also use that bypass because the 256² texture cannot resolve their narrow
kernel without aliasing. This is a CPU reference implementation for the
standalone lab, not an editor frame path or a measured-camera match.
`--ghost-frft-mask polygon` is the default shared iris transform. The
experimental `transport` mode requires `--ghost-pair FIRST SECOND`; it builds a
binary visibility mask from that ghost path's ray grid before applying FrFT.
Use it for isolated path comparisons. It costs one direct 2D FrFT per active
path, source sample, and wavelength, and has no calibrated photographic
reference yet. `make_ghost_transport_resolution_chart.py` compares texture
resolutions against a dense reference and reports crop flux and image error.
`make_transport_mask_path_sweep.py` compares polygon and transport-mask
FrFT across selected isolated ghost paths, including signed differences and
per-crop flux/error metrics.
Use `make_evaluation_chart.py --source-only --single-light-position U V
--source-radius R` to create a size-controlled isolated emitter, and
`make_ghost_flux_linearity_chart.py` to compare normalized flare output across
source scales and extraction thresholds.
`make_finite_source_convergence_chart.py` compares odd-square source grids up
to 15x15 and reports crop flux, peak, centroid, and image error against the
dense source-bin render.
`make_finite_source_size_sweep.py` compares those sampling grids across several
source radii in one image plate.
`--ray-grid` accepts values from 8 to 512; the upper range is intended for
offline convergence checks and can be substantially slower than the default.
The opt-in adaptive refinement probes cell centers and refines cells whose
mapped center deviates from interpolation, whose ray energy changes sharply,
or whose mapping folds. It reports probe/refinement counts and preserves the
parent pupil-cell ray weight across child triangles. This standalone quality
experiment costs extra ray traces and remains off by default.
Use `--adaptive-ray-refinement-threshold PX` to tune the screen-space center
deviation trigger (default 0.75 px); the relative energy trigger is fixed at
25%. See [EVALUATION.md](EVALUATION.md) for an off-axis comparison against
uniform ray grids.
`--optical-path-report` requires `--ghost-pair FIRST SECOND` and reports five
pupil-ray optical paths at each RGB wavelength, referenced to a common plane
100 mm before the front surface. It is a ray-path diagnostic; coating phase,
complex Fresnel amplitudes, phase unwrapping, and coherent image formation are
not yet modeled.
`--optical-path-map CSV` writes the selected path's pupil-grid ray map at the
active `--ray-grid` resolution. `make_optical_path_phase_chart.py` visualizes
four grids and reports neighboring-ray phase steps; wrapped colors are a
sampling diagnostic, not a diffraction render.

## Core/app integration path

The current editor effect is still an empirical screen-space implementation.
Do not treat the lab output as already integrated. Promotion to Core/App needs
a cold-path cache for ray-transfer grids and an allocation-bounded render path;
tracing every prescription path for every frame is too slow for the editor.
