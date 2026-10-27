# Lens Flare Optics Evaluation

**最終更新:** 2026-10-27

## Method

The implementation follows the physical stages described by Hullin et al.
(2011): enumerate two-interface reflection paths, trace parallel ray grids
through spherical interfaces, apply Snell refraction and unpolarized Fresnel
weights, clip against the iris and each lens clear aperture, and rasterize the
resulting sensor grids. It also computes a sampled Fraunhofer intensity kernel
from the polygon aperture. A later real-time method similarly stresses that
realistic ghosts depend on the optical prescription, per-interface energy,
aperture coordinates, and lens-housing clipping; its faster polynomial method
requires precomputation and validation against analytical ray tracing. Its
Angenieux f/11 setup reports 42 ghost paths and retains paths with one stop
crossing. The 15 mm stop-to-rear-group air gap is applied once, between
surfaces 6 and 7.

References: [Hullin et al., Physically-Based Real-Time Lens Flare Rendering
(2011)](https://matthias.hullin.net/publications/HullinEtAl-LensflareRendering-SIGGRAPH2011.pdf);
[TUDelft Advanced Optical Imaging, Fourier analysis](https://qiweb.tudelft.nl/aoi/fourieranalysis/fourieranalysis/);
[Takaki, implementation notes and CPU prototype](https://qiita.com/AngularSpectrumMTD/items/aeea1046c0ec5d4dd1d5);
[Bodonyi et al., 2024 real-time lens flare rendering paper](https://link.springer.com/article/10.1007/s00371-024-03625-7);
[Carbajal-Domínguez et al., numerical near-field scalar diffraction with angular spectrum and FFT](https://rmf.smf.mx/ojs/index.php/rmf-e/article/view/4636);
[reference implementation dispersion model](https://github.com/bodonyiandi94/LensFlareFramework/blob/4136459dffa1ed582f199a9a43950730b4ddcd66/Source/Scene/Components/LensFlare/PhysicalCamera.cpp).

Historical evaluation images and measurements in this document were rendered
with the prior `point-sample` rasterizer unless explicitly identified as
`pixel-area`. Use `--rasterizer point-sample` to reproduce those baselines. The
current default and the two updated Fig. 10 rasterization images use
`pixel-area`.

The included 640x360 HDR chart has four similarly bright colored point lights.
The strongest warm source is at `(245, 138)`.

![HDR evaluation input](examples/evaluation-chart.png)

![Lens prescription ray-traced output](examples/physical-optics.png)

At the chart's standard source intensity, the coated ghosts are intentionally
subtle. For a visibility check, the following chart raises only the four light
radiances by 4096 while keeping the background fixed; this drives the source
cores into display clipping, as a high-dynamic-range camera capture would.

![High-radiance HDR input chart](examples/high-exposure-chart.png)

![Lens prescription result at high source radiance](examples/high-exposure-optics.png)

For diagnostics, the next image amplifies the standard-exposure flare
difference by 256 before display mapping. It is not the physically exposed
final image.

![Ray-traced flare difference amplified 256 times for inspection](examples/flare-difference-amplified-x256.png)

This isolates reflected ghosts (`--diffraction off`), subtracts the input, and
amplifies the result by 16384 to inspect their geometry. The display applies
log compression after that gain so bright ghost regions retain visible shape;
this is diagnostic visualization, not a physical exposure curve.

![Reflected ghost paths isolated and amplified for geometry inspection](examples/ghosts-only-amplified-x16384.png)

On the high-radiance four-source chart, the corrected report shows 42 of 42
eligible paths visible for the warm source; a selected-pair report shows `1/1`.
It ranks the strongest warm-source ghosts as surface pairs
`9 -> 8`, `10 -> 8`, and `11 -> 8`. Each isolated render produces one
aperture-shaped hexagonal patch at a distinct sensor position. This shows the
large hexagons in the combined image come from individual traced ghost paths;
they are not created by blending unrelated paths together. This visual check
does not yet validate their coordinates against the paper's reference image.

![Three individually traced ghost paths, using a common diagnostic scale](examples/ghost-path-comparison-x16.png)

A 3x3 source-position sweep uses a single warm light at normalized coordinates
`0.1`, `0.5`, and `0.9` on each axis. The source radiance is multiplied by 4096
to make the ghost-only images visible; each tile subtracts the input and uses
the same 16x diagnostic multiplier. The 42 eligible paths are drawn from the 91 combinatorial surface pairs,
matching the paper's Angenieux count; all 42 are visible on-axis. At
`(0.1, 0.1)`, seven paths remain visible after aperture and housing clipping.

![Three-by-three field response with reflected ghosts only](examples/field-sweep-3x3-ghosts.png)

## Pixel-area rasterization and caustic convergence

The default rasterizer now clips each mapped ray triangle against sensor-pixel
squares and integrates its linearly interpolated ray energy over the overlap.
Unlike a pixel-center test, this deposits a subpixel triangle's full sampled
flux into the covered pixel instead of making the result depend on whether the
pixel center happens to land inside it. The prior point-sample rasterizer
remains available with `--rasterizer point-sample`; its hard Jacobian cap is
useful for historical comparisons but changes energy and centroid as the cap
varies. Pixel-area mode ignores that cap because the finite pixel footprint
provides the integration area.

At the two Fig. 10 source positions, ray-grid 96 to 192 changed integrated
ghost luminance by +1.19% for source A and +1.09% for source B. Peak luminance
changed by -0.02% (0.04772 to 0.04771) and +1.22% (0.08182 to 0.08281). The
luminance centroids moved 0.72 and 0.71 pixels respectively. For comparison,
legacy point-sample mode with cap 512 changed the same peaks by -75.0% and
-72.9%; its integrated luminance changed +0.11% and +0.59%. Pixel-area mode
therefore stabilizes local caustic peaks substantially while keeping total
energy and centroid near the denser-grid result. This is convergence evidence
for the sampled single-source image, not validation against measured optics.

![Point-sample and pixel-area rasterizer comparison at ray-grid 64, 128, and 192](examples/fig10-rasterizer-convergence.png)

On the high-radiance four-source chart at grid 64 with nine source bins and
diffraction disabled, pixel-area mode adds 1,060.56 luminance versus 1,057.46
for legacy point-sample (+0.29%). Its peak is 4.124 versus 2.664 and the
luminance centroid differs by 0.39 pixels. This shows that the rasterizer
choice has little effect on aggregate energy for this scene while changing
how unresolved caustic peaks land in individual pixels.

![High-radiance reflected ghosts with pixel-area integration on the left and legacy point sampling on the right](examples/high-exposure-pixel-area-vs-point-sample-x256.png)

A separate cap sweep at ray-grid 192 for source A further shows why raising
the legacy cap is not a convergence fix: cap 512 to 4096 changes ghost energy
from 10.1684 to 10.3519 (+1.80%), raises the peak from 0.00719 to 0.05099
(7.1x), and moves the luminance centroid about 5.1 pixels. The default
pixel-area result at grid 192 has energy 10.3436 and peak 0.04771, close to the
legacy high-cap energy without its point-sampling spikes.

![Legacy point-sample Jacobian cap sweep at grid 192](examples/fig10-jacobian-sweep.png)

## Angenieux comparison against Fig. 10

Bodonyi et al. show three results for each lens: the full polynomial model,
analytical ray tracing, and their partial polynomial model. The Angenieux setup
is f/11, 100 mm, 14 refracting surfaces, and 42 ghost paths. Their method uses
distant directional sources, so this comparison collapses each light to one
source sample and disables aperture diffraction to isolate reflected ghosts.

The two normalized source positions `(0.246, 0.356)` and `(0.390, 0.693)` are
estimated from the red and green markers in the paper's Angenieux panel; they
are not coordinates published by the authors. The helper script creates a
black 640x360 chart with small neutral Gaussian source cores. Its radiance is
only selected to make the source detector stable; the evaluation image removes
the input before display. To reproduce the renders:

```powershell
python tools/LensFlareLab/make_fig10_comparison_chart.py
LensFlareLab.exe build/tools/LensFlareLab/fig10-angenieux-two-light-input.pfm build/tools/LensFlareLab/fig10-light-a.pfm --position 0.246 0.356 --source-samples 1 --ray-grid 192 --gain 1 --diffraction off
LensFlareLab.exe build/tools/LensFlareLab/fig10-angenieux-two-light-input.pfm build/tools/LensFlareLab/fig10-light-b.pfm --position 0.390 0.693 --source-samples 1 --ray-grid 192 --gain 1 --diffraction off
```

The visualization separates the two point-source contributions and shows their
combined result using one shared logarithmic display mapping. The ghost field
layout is qualitatively plausible relative to the paper's Angenieux reference,
with aperture-shaped patches and source-dependent field changes. This is not a
pixel-matched validation: the paper does not publish exact source coordinates,
radiance, sensor mapping, tone curve, or the per-surface coating data needed to
recreate its rendering setup. Our coatings and d-line/Abbe-derived dispersion
remain approximations.

The historical point-sample baseline at ray-grid 96 to 192 changed source A's
integrated ghost luminance from 10.1576 to 10.1684 (+0.11%) and moved its
centroid 2.34 pixels; source B changed from 20.7533 to 20.8764 (+0.59%) and
moved its centroid 0.05 pixels. Peak luminance fell from 0.02873 to 0.00719
for source A and from 0.03551 to 0.00962 for source B. This grid-dependent
peak behavior is why the newer pixel-area results are reported separately.

![Angenieux Fig. 10 source-position approximation, rendered at ray-grid 192](examples/paper-fig10-angenieux-position-sweep.png)

The highlight detector integrates positive luminance above threshold over each
source footprint, keeps source chromaticity separate, and distributes that
source flux over the sampled pupil rays once. Sampling the physical stop
backwards through the front lens group maps rays to the entrance pupil instead
of wasting samples over the full front-element diameter.

The detector now retains the spatial radiance distribution inside each
highlight footprint. In default mode it accumulates energy and chromaticity in
3x3 bins and traces each occupied bin from its own field direction for the
reflected ghosts. Aperture diffraction instead uses the full input footprint.
This is an image-space proxy for a finite emitter: it assumes the thresholded input
footprint represents emitter extent, so sensor bloom or an existing point
spread function can be mistaken for source size. It does not infer a 3D emitter
or separate emitter size from camera blur. `--source-samples 1` collapses the
same integrated flux and chromaticity to a single point source for comparison;
`--source-samples 25` selects a 5x5 bin grid for source-integration convergence
checks. The historical nine-bin measurements below remain unchanged.
With `--source-report`, the four-chart input preserved a combined integrated
flux of 2,202,271.78 versus 2,202,271.69 summed across the bins (less than
0.00001% numerical difference). The four highlights occupied 9, 3, 5, and 8
of the nine bins.

![Finite-source comparison: one direction per highlight on the left, nine radiance bins on the right](examples/finite-source-comparison-x256.png)

`--source-samples` controls the directions traced for reflected ghosts. The
diffraction calculation uses the original spatial highlight radiance directly,
so changing the ghost sampling count does not collapse the source footprint to
one, nine, or twenty-five diffraction points.

![Reflected ghosts only, point-source baseline on the left and finite-source sampling on the right](examples/finite-source-ghost-comparison-x256.png)

### Finite-source integration refinement

The lab now accepts `--source-samples 25`, partitioning the same thresholded
detector footprint inside a 24-pixel radius into 5x5 radiance bins. The default remains
3x3, and `1` remains the point-source baseline. This refines source-angle
quadrature without changing the lens, stop pupil grid, ray grid, or total input
highlight energy.

For a single warm source at `(0.3834, 0.3844)` with radiance scale 4096,
ray-grid 96, gain 1, and source diffraction off, point / 3x3 / 5x5 ghost
luminance sums inside the sensor were `931.474`, `931.031`, and `926.547`.
Peak luminance fell from `4.659` to `4.233` to `2.818`; centroids were
`(353.413, 199.140)`, `(353.467, 199.168)`, and `(353.856, 199.397)`. The
sensor-frame energy decrease at 5x5 is consistent with ghost rays shifted
beyond the sensor boundary, not with source-energy renormalization. The 5x5 image differs
from 3x3 by 4.24% L1 relative to its in-frame flare luminance, so the current
3x3 approximation has visible sampling dependence. A 5x5 grid is a refinement
sample, not a proven converged result; more bins can change clipping and cost.

![Single-source point, 3x3, 5x5 ghost renders and the 3x3-to-5x5 signed image difference](examples/finite-source-sampling-convergence-x256.png)

Reproduce the input and three renders with:

```powershell
python tools/LensFlareLab/make_evaluation_chart.py --output build/tools/LensFlareLab/finite-source-convergence-input.pfm --light-scale 4096 --single-light-position 0.3834 0.3844
LensFlareLab.exe build/tools/LensFlareLab/finite-source-convergence-input.pfm build/tools/LensFlareLab/finite-source-1-convergence.pfm --position 0.3834 0.3844 --source-samples 1 --ray-grid 96 --diffraction off
LensFlareLab.exe build/tools/LensFlareLab/finite-source-convergence-input.pfm build/tools/LensFlareLab/finite-source-9-convergence.pfm --position 0.3834 0.3844 --source-samples 9 --ray-grid 96 --diffraction off
LensFlareLab.exe build/tools/LensFlareLab/finite-source-convergence-input.pfm build/tools/LensFlareLab/finite-source-25-convergence.pfm --position 0.3834 0.3844 --source-samples 25 --ray-grid 96 --diffraction off
python tools/LensFlareLab/make_source_sampling_comparison.py build/tools/LensFlareLab/finite-source-convergence-input.pfm build/tools/LensFlareLab/finite-source-1-convergence.pfm build/tools/LensFlareLab/finite-source-9-convergence.pfm build/tools/LensFlareLab/finite-source-25-convergence.pfm tools/LensFlareLab/examples/finite-source-sampling-convergence-x256.png
```

### Pupil ray-grid convergence with finite-source bins

Using that same warm source and the pixel-area rasterizer, the 3x3 and 5x5
source-bin renders were also swept at pupil ray grids 64, 96, and 128. The
integrated in-frame ghost luminance and peak were:

| Source bins | Ray grid | Integrated Y | Peak Y | Centroid (px) |
| --- | ---: | ---: | ---: | ---: |
| 3x3 | 64 | 910.728 | 4.096 | (353.328, 199.238) |
| 3x3 | 96 | 931.031 | 4.233 | (353.467, 199.168) |
| 3x3 | 128 | 928.495 | 4.280 | (353.223, 199.263) |
| 5x5 | 64 | 908.465 | 2.833 | (353.592, 199.379) |
| 5x5 | 96 | 926.547 | 2.818 | (353.856, 199.397) |
| 5x5 | 128 | 924.095 | 2.522 | (353.709, 199.521) |

The 64-to-128 image L1 difference is about 3.00% of the 128-grid in-frame
flare energy for both bin counts. The 96-to-128 L1 differences are 2.07% for
3x3 and 1.97% for 5x5. Integrated luminance at 96 and 128 differs by only
0.27% and 0.26%, while peaks and subpixel ghost edges still change. The 9-bin
grid-64 render took 6.72 s and grid-128 took 20.70 s on the evaluation
machine. This sweep supports 96 as a reasonable look-development compromise
for this chart, but does not establish optical accuracy or convergence for
other fields, prescriptions, or source shapes.

![Ray-grid 64, 96, and 128 comparison for a 3x3 source-bin render](examples/finite-source-ray-grid-convergence-x256.png)

![Ray-grid 64, 96, and 128 comparison for a 5x5 source-bin render](examples/finite-source-ray-grid-25bins-convergence-x256.png)

The plate generator is `make_ray_grid_comparison.py`; pass the same input and
three output PFMs in grid order, then set `--source-samples 9` or `25` to label
the plate. The 5x5 render is appreciably more expensive, so use it as an
offline convergence reference rather than the interactive default.

## Diffraction PSF and source-energy conservation

The aperture transmission mask is Fourier transformed and its squared
magnitude forms a Fraunhofer point-spread function. The sampled PSF is
integrated onto sensor-pixel offsets, normalized to unit energy, then
convolved with the detected source footprint. A 512x512 pupil FFT is resampled
to the pixel grid before convolution. The threshold-excess source radiance is
replaced by its PSF-blurred version, avoiding the previous double addition of
an unchanged source core and a second diffraction copy. At gain 0 the source
is unchanged; gain 1 performs the full replacement, and higher values amplify
the PSF after removing the core once. RGB channels use wavelength-specific
PSFs. Finite-source bins remain in use for ghost ray tracing; diffraction
preserves the input source's full pixel-space shape. The source detector still
treats threshold excess as emitter radiance, so existing bloom or camera blur
can be interpreted as emitter extent.

For the standard chart's warm source at `(0.3834, 0.3844)`, ray-grid 64 and one
ghost source sample, the input integrated luminance is 3712.266. With
diffraction enabled it is 3712.366; with diffraction disabled it is 3712.366.
The reflected ghosts contribute about 0.0999 luminance, while the net
diffraction replacement changes the full-frame sum by only 0.00011. On the
high-radiance four-source chart at grid 64 and nine ghost bins, diffraction-on
versus diffraction-off changes the frame sum by +1.054 luminance (0.000048% of
the 2.21-million input luminance; 0.099% of the 1,060.56 ghost contribution).
Source peak luminance falls from 15,632.08 to 15,504.77. The small frame-sum
residual is from float accumulation against the bright HDR source. These checks
support energy-preserving convolution of the assumed source radiance, not
validation against a measured camera PSF.

One high-radiance chart render took 2.25 s with one ghost direction and about
14.0 s with nine directions at ray-grid 64 on the evaluation machine. Both
include the diffraction FFT and source convolution; timing varies with system
load.

![Diffraction PSF source replacement and on/off difference on a four-light HDR chart](examples/diffraction-energy-conservation-x32.png)

An aperture-blade sweep isolates diffraction with a single centered source.
Five, six, eight, and twelve blades produce visibly different PSF footprints;
the six-blade result retains the corresponding sixfold structure. Across the
sweep, positive and negative luminance changes nearly cancel (net frame change
is below 0.05% of the absolute redistributed luminance), and their centroids
remain within 0.02 pixels of the source center. This checks aperture-dependent
shape and energy accounting in the sampled model; it is not a camera capture
comparison.

![Diffraction-only aperture sweep for 5, 6, 8, and 12 blades; warm is positive and blue is negative](examples/diffraction-aperture-blade-sweep.png)

Reproduce the blade sweep with:

```powershell
python tools/LensFlareLab/make_evaluation_chart.py --output build/diffraction-single-source.pfm --light-scale 4096 --single-light-position 0.5 0.5
LensFlareLab.exe build/diffraction-single-source.pfm diffraction-6-blades.pfm --position 0.5 0.5 --source-samples 1 --ray-grid 16 --gain 1 --diffraction on --ghosts off --aperture-blades 6
```

The 2024 reference implementation derives a Cauchy dispersion curve from the
prescription d-line index and Abbe number. LensFlareLab now uses that
parameterization by default; `--dispersion linear` keeps the previous linear
Abbe estimate available for direct comparison. The comparison below uses the
high-radiance four-light chart, ray-grid 64, nine source samples, gain 1, and
diffraction disabled. After subtracting the unchanged input chart, the ghost
luminance sum is 1,067.74 for linear Abbe and 1,057.46 for Cauchy (-0.96%);
peak luminance is 3.076 and 2.664 respectively. The luminance centroid moves
from `(342.36, 200.03)` to `(343.31, 200.73)`. These are modest changes and do
not establish which model better matches physical glass: both are estimates
from d-line index and Abbe number, not measured Sellmeier coefficients.

![Ghost-only A/B comparison of linear Abbe and Cauchy dispersion](examples/dispersion-cauchy-vs-linear-x256.png)

The legacy point-sample rasterizer uses a 1-pixel reference area and exposes a
cap for local caustic gain; the default is 512 and the supported maximum is
4096. This is a numerical guard, not a paper-derived physical parameter. At ray-grid 192, cap
512 clipped 141,378 of 4,878,908 ray triangles (2.90%); cap 128 clipped
234,342 (4.80%), and cap 4096 clipped 128,315 (2.63%). The comparison below
uses an on-axis high-radiance source with diffraction disabled. Each panel uses
the same warm false-color scale (128x display gain); the source core is masked
so the surrounding ghost field is easier to compare.

![Legacy point-sample Jacobian cap comparison at ray-grid 192](examples/jacobian-cap-comparison.png)

At gain 1, the standard chart with one source sample has `192.52` integrated
luminance above the source threshold. With the updated pixel-area rasterizer,
ray-grid 64, and diffraction on, total added luminance is `0.09998`, almost
all of it the `0.09987` reflected-ghost contribution. The earlier additive
diffraction overlay reported `6.38` luminance here; that was a second copy of
the source radiance and has been removed. Turning the coating off at ray-grid
64 raises ghost energy from about `0.0999` to `2.78`, around 27.8 times. These
are synthetic measurements, not a match validated against a captured lens.

For the harder on-axis single-light case with diffraction disabled in legacy
point-sample mode, ghost-only energy at cap 512 was `1312.15`, `1316.72`, and
`1329.00` for ray-grids 64, 128, and 192; peak ghost pixel luminance was `5.206`, `1.289`, and `0.766`.
The 128-to-192 energy difference is `0.93%`. At grid 192, increasing cap from
128 to 512 changes total ghost energy from `1326.85` to `1329.00` (0.16%),
while cap 4096 reaches `1338.74` (0.73% above cap 512) and raises the peak to
`3.202`. Cap 512 was the legacy point-sample look-development compromise between energy
retention and unresolved caustic peaks; 2.90% of mapped triangles still reach
that limit, so it is not a claim of full physical convergence.

## Reproduce

```powershell
python tools/LensFlareLab/make_evaluation_chart.py
LensFlareLab.exe tools/LensFlareLab/evaluation-chart.pfm output.pfm --auto --ray-grid 128 --gain 1
LensFlareLab.exe tools/LensFlareLab/evaluation-chart.pfm ghosts.pfm --auto --ray-grid 128 --gain 1 --diffraction off --ghost-report
python tools/LensFlareLab/make_evaluation_chart.py --output build/high-exposure.pfm --light-scale 4096
LensFlareLab.exe build/high-exposure.pfm high-exposure-output.pfm --auto --ray-grid 128 --gain 1
LensFlareLab.exe build/high-exposure.pfm diffraction-only.pfm --auto --ray-grid 64 --gain 1 --ghosts off --diffraction on
LensFlareLab.exe build/high-exposure.pfm diffraction-off.pfm --auto --ray-grid 64 --gain 1 --ghosts off --diffraction off
LensFlareLab.exe build/high-exposure.pfm one-ghost.pfm --position 0.3834 0.3844 --ray-grid 128 --gain 1 --diffraction off --ghost-pair 9 8 --ghost-report
python tools/LensFlareLab/make_evaluation_chart.py --output build/off-axis.pfm --light-scale 4096 --single-light-position 0.1 0.1
LensFlareLab.exe build/off-axis.pfm off-axis-flare.pfm --auto --ray-grid 64 --gain 1 --diffraction off --ghost-report
```

The tracked PNGs are display-mapped previews. The PFM chart is generated on
demand and is not stored in the source tree.

## Scalar Fresnel propagation on ray-mapped ghost layers

The lab now has a separate, opt-in wave-optics look-development path. For each
source bin, two-reflection path, and RGB wavelength, it first produces the
usual ray-mapped irradiance layer. It uses the square root as a real,
zero-phase scalar field, pads the local image-space layer, and applies the
paraxial free-space transfer function
`H(fx,fy)=exp(-i π λ z (fx²+fy²))` between a 2D FFT and inverse FFT. Squared
amplitude replaces that path-band's geometric irradiance. The physical pixel
pitch comes from sensor width and image width; default `z=100 mm` is the lens
effective focal length. This creates wavelength-dependent Fresnel-scale edge
ripples, while each source bin/path remains mutually incoherent with the next.

This is not a full diffraction model of the compound lens. The geometry pass
does not track optical path length or complex phase through refractive and
reflective surfaces, and the effective propagation plane/distance is only a
proxy. A four-Fresnel-scale local ROI captures the visible near-edge structure;
small numerical flux correction is applied to the full padded ROI, then light
outside the sensor is clipped. `--ghost-diffraction-distance-mm` is exposed to
measure sensitivity rather than imply a calibrated path distance. The option
defaults off, so the geometric baseline remains bit-identical. This path is
not promoted to Core/App.

### Numerical check of the Fresnel propagator

The production FFT and propagation helper were exercised directly by a
temporary standalone probe that includes `main.cpp`; the probe did not alter
the application source. The forward/inverse radix-2 FFT round-trip maximum
absolute complex error was `3.21e-7`, `2.43e-6`, and `6.04e-6` for square
arrays of 8, 32, and 128 samples per side.

For a 550 nm Gaussian beam with a 0.30 mm 1/e field-radius waist, 100 mm
propagation, and 0.01 mm sample pitch, the propagated intensity RMS radius was
`0.216108236 mm`; the paraxial Gaussian-beam solution
`w(z)/sqrt(2)`, with `zR=πw0²/λ`, gives `0.216108219 mm`. The relative
difference is `7.8e-8` and the measured centroid error is below
`2.1e-7 mm`. The raw ROI energy scale was `1.0000017`, so this case did not
depend materially on the flux correction. This checks FFT normalization,
frequency indexing, and the sign-insensitive beam-width evolution for a smooth
field; it does not validate phase-sensitive output, hard aperture edges, or
the ghost exit-pupil approximation.

References for the scalar model and analytic check:
[Carbajal-Domínguez et al., near-field scalar diffraction with angular
spectrum and FFT](https://rmf.smf.mx/ojs/index.php/rmf-e/article/view/4636);
[Goodman, *Introduction to Fourier Optics*, Fresnel transfer function](https://djvu.online/file/ej4aFH0hNKFsF);
[University of Oxford, Gaussian-beam diffraction](https://academic.oup.com/book/25444/chapter-abstract/192619632).

The image comparisons above remain diagnostic: an isolated hexagonal ghost
shows edge redistribution, but the 42-path and default nine-source-bin images
change much less visibly. Without path optical phase and a calibrated
exit-pupil plane, the current scalar propagation is a numerically checked
look-development experiment rather than evidence of camera-realistic flare.

On the isolated 9→8 reflection (source `(0.3834, 0.3844)`, ray grid 128,
point source, Fraunhofer source diffraction off), the geometric ghost has
integrated luminance `128.328`, peak `0.003364`, and centroid `(465.157,
257.015)`. With `z=100 mm`, the result is `128.235`, `0.005375`, and `(465.164,
256.938)`. The small integrated loss is light clipped beyond the sensor edge
(the ghost reaches the bottom boundary); centroid movement is 0.08 px. At 50
and 200 mm, the integrated luminances are `128.269` and `128.190`, and the
peak values are `0.004714` and `0.005295`. This confirms distance-sensitive
ring redistribution, not agreement with a physical camera capture.

![Isolated 9-to-8 reflection before and after scalar Fresnel propagation; difference panel shows added red and removed blue luminance at display gain 300](examples/ghost-fresnel-propagation-x300.png)

With all 42 eligible paths enabled on the same one-source chart (ray grid 96,
source diffraction off), geometric and propagated integrated luminance are
`931.474` and `931.004`; centroid shifts from `(353.413, 199.140)` to
`(353.463, 199.162)`. The peak falls from `4.659` to `1.227`, because the
propagation spreads overlapping caustic peaks across neighboring pixels. The
absolute sum of the signed difference is `38.243` luminance, so this is a
material shape change even though total flux is close. The full default mode
(ray grid 64, nine finite-source bins, Fraunhofer source diffraction on) gives
ghost-only luminance `911.115` (geometric) and `911.309` (propagated), peak
`13.397` and `13.253`, respectively, and centroid motion below `0.06 px`.
This is an integrated default-path
measurement in addition to the source-diffraction-off image comparison below.

![All 42 path comparison for a point source with source diffraction off, rendered at display gain 40](examples/all-ghosts-fresnel-comparison-x40.png)

Reproduce an isolated path comparison and its preview with:

```powershell
LensFlareLab.exe build/ghost-diffraction-input.pfm build/ghost-geometric.pfm --position 0.3834 0.3844 --ray-grid 128 --source-samples 1 --diffraction off --ghost-pair 9 8
LensFlareLab.exe build/ghost-diffraction-input.pfm build/ghost-fresnel.pfm --position 0.3834 0.3844 --ray-grid 128 --source-samples 1 --diffraction off --ghost-pair 9 8 --ghost-diffraction on --ghost-diffraction-distance-mm 100 --ghost-diffraction-report
python tools/LensFlareLab/make_ghost_diffraction_comparison.py build/ghost-diffraction-input.pfm build/ghost-geometric.pfm build/ghost-fresnel.pfm tools/LensFlareLab/examples/ghost-fresnel-propagation-x300.png --gain 300
```

To regenerate the all-path comparison, use the same input with `--ghost-pair`
omitted, ray grid 96, and `--ghost-diffraction on`; render the baseline with
the flag omitted, then run the comparison script with `--crop 0 0 639 359
--gain 40`.

## Limits and next evaluation gates

Hullin et al. distinguish Fraunhofer starburst from near-field ringing in
ghost elements. Their pipeline builds an aperture-transmission texture for
each ghost path and applies a fractional Fourier transform (FrFT), with the
heuristic order `α = 0.15 × (λ / 400 nm) × (f/# / 18)` (Eq. 5). The lab now
exposes `--diffraction-report` and `--fresnel-frft-order` to inspect and sweep
that RGB order. The separate scalar Fresnel propagation prototype above does
not substitute for the per-path aperture texture, and the Fraunhofer source
PSF is not a ghost diffraction model. The first standalone aperture-FrFT
look-development path is evaluated below.

For the default 100 mm, f/11 setup, Eq. (5) gives RGB orders approximately
`0.140, 0.126, 0.106` for 610, 550, and 460 nm. These are the paper's
look-matching heuristic, not a calibration of this prescription.

The prescription table gives d-line refractive indices and Abbe numbers, not
full Sellmeier coefficients. The lab approximates RGB dispersion with a Cauchy
approximation derived from those Abbe values and uses one idealized coating
layer; the real coating stack and manufacturing scatter are unknown. The
aperture diffraction is a discrete FFT approximation, and source
visibility/occlusion is not modeled. One pixel-area, single-source render with
diffraction off took 0.67 s at ray-grid 64, 1.86 s at 128, and 3.97 s at 192
on the evaluation machine. These are single-run timings and vary with CPU load.
The lab remains a look-development reference, not an editor frame path.

Next: compare ghost coordinates against Fig. 10's analytical ray-traced
reference in the [2024 paper](https://link.springer.com/article/10.1007/s00371-024-03625-7)
and against real lens captures. Replace Abbe-derived Cauchy/coating
approximations with measured optical data before considering an
allocation-bounded GPU integration.

## Per-path aperture FrFT look-development

Hullin et al. retain each ray's aperture-plane crossing and use a per-ghost
aperture transmission texture. Their method applies an FrFT to the aperture
mask to introduce near-field ringing, with the order estimated by Eq. (5).
The current lab captures the iris-plane position on each valid path, applies a
separable direct-kernel 2D FrFT to a 256×256 binary polygon aperture, and
weights ray energy by the transformed intensity. Each path/source-sample/
wavelength ray map is normalized to its original sampled ray-energy sum before
rasterization. `--ghost-frft-report` prints the effective orders, texture and
ray-map normalizers, and cold-path precompute time.

The α=0 limit is an exact geometric bypass: the isolated 9→8 path is
pixel-identical to the geometric baseline. For the default f/11 lens, the
paper's Eq. (5) RGB orders are `0.139806, 0.126054, 0.105427`. With a point
source at `(0.3834, 0.3844)`, ray grid 128, and source diffraction off, the
isolated 9→8 path changes from Y flux `128.328` to `131.499`, peak luminance
`0.003364` to `0.005954`, and centroid `(465.157, 257.015)` to
`(457.916, 254.927)`. The sensor-cropped integrated flux rises 2.47% despite
per-map ray normalization; transformed weighting moves some energy across the
sensor boundary. The absolute image difference is 31.62% of geometric ghost Y
flux, with visible sixfold aperture ringing.

![Isolated geometric ghost compared with the Eq. 5 aperture FrFT result, plus signed luminance difference at display gain 300](examples/ghost-aperture-frft-x300.png)

With all 42 eligible paths at ray grid 64, the sensor-cropped Y flux changes
from `911.276` to `930.914`, relative image L1 is 18.37%, peak changes from
`4.388` to `4.585`, and centroid moves from `(353.274, 199.223)` to
`(352.082, 198.837)`. This shows that the aperture pattern remains visible
among overlapping ghosts, while the individual pattern is clearer in the
isolated comparison.

![All 42 geometric ghosts compared with Eq. 5 aperture FrFT, with signed difference at display gain 40](examples/ghost-aperture-frft-all-paths-x40.png)

The α=1 endpoint uses the Fourier kernel and shows the expected aperture
diffraction pattern. It is a transform endpoint, not the order used for normal
ghost rendering. Explicit orders below 0.01 bypass the sampled kernel: at
256² its sharply concentrated kernel is unresolved and produced unstable
energy before this guard. The direct 256² RGB precompute took about 90 ms on
the evaluation machine; it is standalone cold-path work, not suitable for a
per-frame editor path. The remaining 1–3% in-frame flux difference and
empirical ray-map normalization make this a look-development approximation,
not a calibrated optical-energy solution.

![The α=1 Fourier limit compared with the geometric ghost](examples/ghost-aperture-frft-fourier-limit-x300.png)

### Texture-resolution convergence

The resolution is now selectable from 64 to 512 with
`--ghost-frft-resolution`. On the isolated 9→8 path (ray grid 128), increasing
the texture from 128² to 256² changes the image by 1.63% relative L1 and moves
the ghost centroid by about 0.50 px. From 256² to 512², relative L1 falls to
0.068%; Y flux changes from `131.49943` to `131.48870`, peak from `0.0059537`
to `0.0059595`, and centroid by less than 0.001 px. The measured RGB
precompute times were 12, 90, and 723 ms for 128², 256², and 512². This
supports 256² as the current standalone compromise for this path, not a
universal convergence guarantee for every ghost.

![Isolated path comparison using 128², 256², and 512² aperture textures](examples/ghost-aperture-frft-resolution-sweep.png)

### Aperture shape and field sweep

For the same point source and 9→8 path at ray grid 96, 5/6/8/10-blade
apertures produce geometric-to-FrFT Y fluxes of `117.931→121.056`,
`128.967→132.305`, `138.052→143.871`, and `145.234→151.369`. Relative L1
changes are 34.48%, 31.73%, 30.12%, and 30.27%. The ring pattern tracks the
blade count, although its radiometry still includes the per-map normalization
described above.

![Geometric and FrFT ghost shapes for 5, 6, 8, and 10 aperture blades](examples/ghost-aperture-blade-sweep.png)

An all-path field sweep used source positions `(0.1,0.1)`, `(0.5,0.5)`, and
`(0.9,0.9)` with a ray grid of 64. The center source yields 42/42 visible
paths and geometric-to-FrFT Y flux `1288.998→1321.126` (11.996% relative
L1). Each corner source yields 7/42 visible paths and `30.660→32.951`
(30.07% relative L1); the two corners agree closely by symmetry. A 9→8-only
comparison at those corners is empty because that path is clipped, so the
all-path comparison is required for field coverage.

At `(0.1,0.1)` with ray grid 128, the largest per-map normalization is 8.15
on the weak 6→2 path (unweighted valid-ray energy `1.05e-4`); the corner
image's total Y is only `30.66`. Repeating the same field with 512² texture
leaves the max factor at 14.55 at ray grid 64 but changes corner-image flux by
only 0.02% and relative image L1 by 0.05%. This points to sparse path sampling
and low-energy outlier maps, rather than texture resolution, as the source of
the large diagnostic factor. It has little effect on this image's aggregate
flux, but deserves a future adaptive-sampling or low-energy-map policy before
this technique is used as a calibrated model.

![All-path field comparison at upper-left, center, and lower-right source positions](examples/ghost-aperture-field-sweep.png)

The upper-left all-path image was also rendered at ray grids 64/96/128/192.
Relative to grid 192, geometric Y flux differs by 7.08%/3.71%/1.76%, and
image L1 by 7.09%/4.22%/2.50%. With FrFT, Y flux differs by
2.65%/1.41%/0.60%, and image L1 by 4.50%/2.39%/1.31%. Grid 192 is only the
highest available reference, not a converged ground truth. This field case
needs denser rays than the centered example; a fixed grid-96 setting should
not be treated as a general quality guarantee.

![Off-axis geometric and FrFT field outputs at ray grids 64, 96, 128, and 192](examples/ghost-aperture-offaxis-ray-grid-convergence.png)

### Finite-source averaging

At ray grid 48 with diffraction off, source sampling 1/9/25 changes the
geometric ghost peak from `4.420/4.043/2.932` while Y flux stays within 0.08%
(`892.36/892.19/891.72`). With aperture FrFT, peaks are
`4.584/4.121/3.034`, Y flux is `916.30/916.27/915.48`, and the 9-to-25-bin
image difference is 4.22% relative L1. Thus finite-source angular averaging
reduces the caustic peak by about one third without materially changing the
integrated flux in this chart; it also softens the aperture-ring detail.

![Geometric and FrFT output for point, 3×3, and 5×5 source sampling](examples/ghost-aperture-finite-source-sweep.png)

With the normal source diffraction enabled and the 3×3 source sampling, the
integrated output difference between geometric and FrFT modes is 2.91% relative
L1. The shared direct-source PSF dominates that aggregate; the ghost-only
comparison above is more sensitive to the aperture texture.

![Full flare with source diffraction and 3×3 finite-source sampling](examples/ghost-aperture-full-default.png)

Reproduce the texture resolution sweep by rendering the same isolated path at
`--ghost-frft-resolution 128`, `256`, and `512`, then passing each result as a
separate `--case` to `make_ghost_frft_sweep_chart.py`. The aperture sweep uses
the same workflow with `--aperture-blades 5|6|8|10`. For the field sweep,
regenerate the input chart with `make_evaluation_chart.py
--single-light-position U V`, render all eligible paths, then compare the
geometric and FrFT outputs at each field point.

The implementation follows the aperture-coordinate and FrFT look-development
approach in [Hullin et al., 2011](https://matthias.hullin.net/publications/HullinEtAl-LensflareRendering-SIGGRAPH2011.pdf)
and the continuous FrFT kernel described by
[Ozaktas et al., 1996](https://www.ee.bilkent.edu.tr/~haldun/publications/ozaktas108.pdf).
It uses direct separable quadrature rather than Ozaktas's fast
chirp-convolution algorithm. Core/App promotion needs a stable optical-phase
and energy model, calibrated reference captures, and cold-path cached transfer
data before any bounded GPU-frame integration.

Reproduce the isolated comparison and its image plate with:

```powershell
LensFlareLab.exe build/finite-source-convergence-input.pfm build/ghost-geometric.pfm --position 0.3834 0.3844 --ray-grid 128 --source-samples 1 --diffraction off --ghost-pair 9 8
LensFlareLab.exe build/finite-source-convergence-input.pfm build/ghost-frft-eq5.pfm --position 0.3834 0.3844 --ray-grid 128 --source-samples 1 --diffraction off --ghost-pair 9 8 --ghost-frft on --ghost-frft-report
python tools/LensFlareLab/make_ghost_frft_comparison.py build/finite-source-convergence-input.pfm build/ghost-geometric.pfm build/ghost-frft-eq5.pfm tools/LensFlareLab/examples/ghost-aperture-frft-x300.png --gain 300
```
