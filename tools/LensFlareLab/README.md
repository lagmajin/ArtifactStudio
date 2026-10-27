# Lens Flare Lab

**最終更新:** 2026-10-28

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
bin traces the complete selected ghost path. `--source-sampling equal-grid`
preserves the default spatial grid. The experimental `weighted-quantile` mode
partitions each source footprint at cumulative luminance quantiles before
forming the requested source bins; this can activate more directions for a
small emitter, at a higher ray-tracing cost. The interactive equivalent is
`set sourceSamplingQuantiles 1` (or `0` for equal-grid). Rays are sampled over
the physical stop and traced backwards through the front group to the entrance
pupil, which gives coarse grids useful coverage at the stated f-number.
Reflected ghost paths and diffraction can be rendered independently for
evaluation. Paths that cross the aperture stop three times are omitted; the
Angenieux prescription then has 42 remaining surface pairs, matching the
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
               [--gain N] [--jacobian-cap N] [--ray-grid N] [--source-samples 1|odd square <=225]
               [--source-sampling equal-grid|weighted-quantile]
               [--adaptive-ray-refinement on|off] [--adaptive-ray-refinement-threshold PX]
               [--adaptive-ray-refinement-energy-threshold FRACTION]
               [--dispersion cauchy|linear] [--spectral-samples 3|7|15|31|61]
               [--rasterizer pixel-area|point-sample]
               [--aperture-radius MM]
               [--aperture-rotation-degrees DEG]
               [--central-obstruction-ratio 0..0.8]
               [--diffraction-resolution 128|256|512|1024|2048|4096]
               [--aperture-blades N] [--coating on|off]
               [--ghosts on|off] [--diffraction on|off]
               [--ghost-diffraction on|off] [--ghost-diffraction-distance-mm MM]
               [--ghost-diffraction-report]
               [--ghost-frft on|off] [--ghost-frft-resolution N] [--ghost-frft-report]
               [--ghost-frft-mask polygon|transport] [--ghost-frft-normalization triangle|vertex]
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
by Hullin et al. Its physical stop semidiameter is set to EFL/22 (the nominal
f/11 opening before pupil imaging); the 15 mm air gap between the stop and rear
group is applied once. Paraxial tracing currently estimates the complete
prescription at f/6.38 because the entrance pupil is magnified; treat the old
f/11 diffraction fixtures as isolated aperture baselines until this model is
validated against an optical-design package. RGB dispersion defaults
to a Cauchy approximation parameterized by the prescription d-line index and
Abbe number, following the coefficient form used by the 2024 reference
implementation. `--dispersion linear` selects the prior linear Abbe
approximation for A/B comparisons. Neither model replaces measured Sellmeier
glass data. `--coating on` applies a
single MgF2-like quarter-wave layer only at air/glass boundaries (either side
within 0.001 of n=1); glass/glass cemented interfaces retain bare Fresnel
reflectance. Its index and 550 nm design wavelength are illustrative, not
prescription or manufacturer data. Thin-film interference varies with angle
and wavelength; it is a spectral sensitivity experiment, not a blanket
ghost-reduction switch. `--coating-report --ghost-pair FIRST SECOND` reports
incidence angle, per-interface Fresnel reflectance with and without the layer,
and the accumulated path factor for a chief pupil ray at 610/550/460 nm. Gain is an exposure/look-
development control and is not a calibrated camera response. `--diffraction
off` isolates reflected ghost paths for image evaluation; `--coating off`
provides an uncoated comparison.
`--ghosts off` isolates diffraction, and `--diffraction off` isolates reflected
ghosts.
`--aperture-rotation-degrees` rotates the shared regular-polygon stop in the
pupil plane. The angle is applied to direct Fraunhofer diffraction, geometric
ghost clipping, and polygon-based ghost FrFT textures so those paths use the
same stop orientation. The polygon repeats every `360 / blade count` degrees.
`--central-obstruction-ratio` models an opaque, concentric circular obstruction
as a fraction of the outer pupil radius. It affects direct Fraunhofer PSFs and
both reflected-ghost aperture paths. Direct PSF energy is additionally scaled
by the sampled open-pupil area relative to the clear polygon; zero preserves
the unobstructed output. This is relative pupil throughput, not absolute
T-stop, glass transmission, or exposure calibration.
The direct-source Fraunhofer PSF defaults to `--diffraction-resampling pixel-area`.
It treats each mapped FFT intensity sample as piecewise constant over its cell
and integrates its overlap with sensor-pixel areas. The alternative
`--diffraction-resampling tent` bilinearly splats each sample onto neighboring
pixels. A subpixel-phase sweep against an independently oversampled pupil FFT
found lower image error for pixel-area resampling in the tested six-blade
near-point cases; see [EVALUATION.md](EVALUATION.md). Both modes preserve unit
kernel energy. The model assumes uniform pixel response and does not change
ghost diffraction or model measured intrapixel sensitivity.
`--diffraction-resolution` sets the square aperture FFT used by the direct
source PSF. The default is 1024²; 512² is the legacy-speed comparison and
2048²/4096² are offline convergence references with progressively longer
render times. This setting does not change the separate ghost-diffraction or
ghost-FrFT texture resolutions.
`evaluate_pupil_psf.py` renders centered neutral point-source cases and reports
throughput, centroid, mirror symmetry, encircled radii, and RGB error against a
dense FFT reference for several blade counts and a central obstruction. Its
plate and raw metrics are included in [EVALUATION.md](EVALUATION.md); it checks
the ideal sampled stop model, not a measured camera PSF.
`evaluate_source_field_psf.py` compares near-point and extended neutral emitters
at the frame center and both horizontal edges against a linear convolution of
the measured point PSF. It records source clipping, PSF throughput, centroid,
encircled radii, and image error; results and limits are in
[EVALUATION.md](EVALUATION.md).
`evaluate_spectral_field_psf.py` extends direct-diffraction color evaluation
to primary-color near-point and extended emitters at center and frame edge. It
compares 3/15/61 wavelength integrations and records per-channel throughput,
PSF radii, RGB-basis cross-channel energy, and negative linear-RGB mass.
`evaluate_sensor_sampling.py` renders the same point PSF on several raster
sizes for the fixed 53.96 mm sensor width, records physical encircled radii,
and cross-checks each PFM against an independent NumPy pupil-FFT accumulator.
Reflected ghosts use the original three wavelength samples by default.
`--spectral-samples 7|15|31|61` selects trapezoidal integration over 430–610 nm
with a piecewise-linear reconstruction from input RGB for both reflected
ghosts and direct-source Fraunhofer diffraction. For counts above three, a
dual basis maps the sampled spectrum back to RGB so a wavelength-independent
transfer preserves integrated RGB channel energy. The 7-band option preserves
its original wavelengths; higher counts are offline convergence probes (61
bands at 3 nm spacing). This is an algebraic reconstruction from three RGB
channels, not a CIE standard-observer conversion, measured source spectrum, or
camera sensitivity model. It can produce small negative linear-RGB values for
some saturated inputs. Higher counts increase CPU time roughly in proportion
to the band count; the default 3-band mode retains the original RGB wavelengths
and output.
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
contain energy. It also reports `touchesFrameBoundary` when above-threshold
source pixels reach the image edge; this warns that the reported flux only
covers visible pixels and does not estimate unseen off-frame radiance.

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
`make_multilobe_source.py` generates an asymmetric three-lobe HDR emitter with
optional color-separated spectra and adjustable lobe spacing;
`measure_rgb_error.py` reports flare-only RGB and per-channel L1 against a PFM
reference. `make_source_diffraction_comparison.py` plates direct-source PSF
on/off and frame-crop differences, while `make_aperture_diffraction_sweep.py`
compares blade-count PSFs at a selected crop. `measure_source_psf.py` measures
per-channel encircled-energy radii from the PFM outputs.
`evaluate_diffraction_pixel_phase.py` creates pixel-area-integrated Gaussian
sources at eight subpixel phases, compares tent and pixel-area PSF resampling,
and records throughput, centroid, and encircled-radius sensitivity.
`evaluate_pixel_phase_convergence.py` repeats the pixel-area phase sweep across
aperture FFT sizes from 128² to 4096² and compares each normalized PSF crop to
the same-phase 4096² result.
`evaluate_wavefront_aberration_psf.py` creates polychromatic PSFs from a
six-blade pupil with controlled Zernike-style wavefront modes, integrates them
over detector pixels, and cross-checks the zero-aberration kernel against the
existing renderer.
`evaluate_prescription_wavefront.py --samples 512` accumulates optical path
length through the direct-transmission prescription, estimates the paraxial
virtual entrance/exit pupils by reverse/forward tracing ray pencils from the
stop through the front/rear groups, maps phase to an image-centered reference
sphere, and checks adjacent-ray OPL increments against the eikonal relation.
It reports the effective f-number from both pupil magnifications; for the
current assumed prescription they agree near f/6.38 even though the physical
stop semidiameter is EFL/22. This is an intermediate wavefront diagnostic, not
a coherent PSF; the ray-map Jacobian is measured, but its geometric amplitude
proxy is not yet applied to a complex pupil.
`evaluate_diffraction_supersampled_reference.py` compares both resamplers with
an independent 4096² pupil FFT and 9× sensor-pixel integration reference.
Pixel-phase evaluation requires Pillow; the supersampled reference requires
NumPy, SciPy, and Pillow; spectral evaluation requires NumPy and Pillow. The
wavefront evaluations require NumPy, SciPy, and Pillow. The renderer itself
has no Python runtime dependency.
`evaluate_direct_diffraction_spectral.py` compares 3/7/15/31/61 wavelength
sampling for neutral and primary-color point sources against its 61-band
numerical reference, using NumPy and Pillow.
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
`--ghost-frft-normalization triangle` is the default: it normalizes each
path/source/wavelength map with the same valid ray triangles and vertex
quadrature used by pixel-area rasterization. `vertex` selects the earlier
valid-node sum for A/B comparison. With adaptive ray refinement, triangle
normalization uses the selected cells’ eight-triangle center fan and the same
four edge-midpoint rays as rasterization; non-refined cells keep the two-triangle
split. Adaptive topology uses the same unweighted optical-trace energy probes
with FrFT on or off, then final FrFT weights are normalized over that mesh.
The report also estimates the integrated RGB-luminance contribution over all
mapped ghost triangles before sensor clipping and the remainder outside the
sensor. Use the default `pixel-area` rasterizer for this estimate; it helps
separate frame cropping from changes in the full mapped field, but is a sampled
ray-map diagnostic rather than a calibrated energy measurement.
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
`make_frft_field_path_sweep.py` composes geometric-versus-FrFT PFM renders
across repeated field/path cases and reports frame luminance, relative image
L1, and centroid movement on one shared exposure scale.
`make_ghost_frft_resolution_chart.py` compares 64²/128²/256²/512² polygon or
transport-mask texture renders against the dense 512² reference.
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
parent pupil-cell ray weight across child triangles. For FrFT, triangle
normalization follows this selected mesh, including each refined cell’s
edge-midpoint rays and center fan. Refinement decisions use raw optical-trace energy, independent of FrFT
texture weighting. Against uniform grid 512, field evaluation found 4.52%/5.52% FrFT image L1
for adaptive grid 64 at corner/center sources. The reference itself changes
2.23%/4.12% from grid 192 to 512, so these are comparisons, not convergence
or a general quality guarantee. This standalone quality experiment costs
extra ray traces and remains off by default.
Use `--adaptive-ray-refinement-threshold PX` to tune the screen-space center
deviation trigger (default 0.75 px, range 0.001–8 px), and
`--adaptive-ray-refinement-energy-threshold FRACTION` to tune relative
unweighted optical-trace energy deviation (default 0.25, range 0.000001–4). The
report lists mapping, energy, validity-boundary, and foldover refinement
triggers separately. See [EVALUATION.md](EVALUATION.md) for off-axis and
per-path comparisons against uniform ray grids.
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
