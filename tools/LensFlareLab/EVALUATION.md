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
[Takaki, implementation notes and CPU prototype](https://qiita.com/AngularSpectrumMTD/items/aeea1046c0ec5d4dd1d5);
[2024 real-time lens flare rendering paper](https://link.springer.com/article/10.1007/s00371-024-03625-7).

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

The highlight detector integrates positive luminance above threshold over each
source footprint, keeps source chromaticity separate, and distributes that
source flux over the sampled pupil rays once. Sampling the physical stop
backwards through the front lens group maps rays to the entrance pupil instead
of wasting samples over the full front-element diameter.

The detector now retains the spatial radiance distribution inside each
highlight footprint. In default mode it accumulates energy and chromaticity in
3x3 bins and traces each occupied bin from its own field direction, applying
the same source samples to reflected ghosts and aperture diffraction. This is
an image-space proxy for a finite emitter: it assumes the thresholded input
footprint represents emitter extent, so sensor bloom or an existing point
spread function can be mistaken for source size. It does not infer a 3D emitter
or separate emitter size from camera blur. `--source-samples 1` collapses the
same integrated flux and chromaticity to a single point source for comparison.
With `--source-report`, the four-chart input preserved a combined integrated
flux of 2,202,271.78 versus 2,202,271.69 summed across the bins (less than
0.00001% numerical difference). The four highlights occupied 9, 3, 5, and 8
of the nine bins.

![Finite-source comparison: one direction per highlight on the left, nine radiance bins on the right](examples/finite-source-comparison-x256.png)

For the high-radiance chart at ray-grid 64, the 9-bin full flare changed the
summed added luminance from 74,164.02 to 74,158.06 (-0.008%) and reduced its
maximum pixel from 4,983.38 to 4,759.99 (-4.48%). With diffraction disabled,
reflected-ghost luminance changed from 1,066.54 to 1,067.74 (+0.11%) and its
peak from 3.203 to 3.076 (-3.98%). One measured pass took 4.44 s for the point-source render and 18.25 s for the
finite-source render on the evaluation machine. CPU load and timing noise affect
these standalone measurements. Only 3-9 bins per
source were occupied, so cost scales with the active bins rather than always
multiplying by nine. These measurements show a modest spread and peak reduction
at a notable CPU look-development cost; they do not demonstrate physical
validation against an emitter with known angular size.

![Reflected ghosts only, point-source baseline on the left and finite-source sampling on the right](examples/finite-source-ghost-comparison-x256.png)

The rasterizer uses a 1-pixel reference area and exposes a cap for local
caustic gain; the default is 512 and the supported maximum is 4096. This is a
numerical guard, not a paper-derived physical parameter. At ray-grid 192, cap
512 clipped 141,378 of 4,878,908 ray triangles (2.90%); cap 128 clipped
234,342 (4.80%), and cap 4096 clipped 128,315 (2.63%). The comparison below
uses an on-axis high-radiance source with diffraction disabled. Each panel uses
the same warm false-color scale (128x display gain); the source core is masked
so the surrounding ghost field is easier to compare.

![Jacobian cap comparison at ray-grid 192](examples/jacobian-cap-comparison.png)

At gain 1, the standard chart with 1 source sample has `192.52` integrated luminance above the
source threshold. Total added luminance is `6.49057` at ray-grid 64 and
`6.49163` at 128, a `0.0163%` change. At ray-grid 128, the flare adds `6.492` luminance:
`6.3795` from diffraction and `0.1121` from reflected ghosts. The ghost transfer
is about `0.058%` of the chart's above-threshold source flux for the assumed
single-layer coating. Turning that coating off at ray-grid 64 raises ghost
energy from about `0.1121` to `2.779`, around 24.8 times. These are synthetic
measurements, not a match validated against a captured lens.

For the harder on-axis single-light case with diffraction disabled, ghost-only
energy at cap 512 was `1312.15`, `1316.72`, and `1329.00` for ray-grids 64,
128, and 192; peak ghost pixel luminance was `5.206`, `1.289`, and `0.766`.
The 128-to-192 energy difference is `0.93%`. At grid 192, increasing cap from
128 to 512 changes total ghost energy from `1326.85` to `1329.00` (0.16%),
while cap 4096 reaches `1338.74` (0.73% above cap 512) and raises the peak to
`3.202`. Cap 512 is the current look-development compromise between energy
retention and unresolved caustic peaks; 2.90% of mapped triangles still reach
that limit, so it is not a claim of full physical convergence.

## Reproduce

```powershell
python tools/LensFlareLab/make_evaluation_chart.py
LensFlareLab.exe tools/LensFlareLab/evaluation-chart.pfm output.pfm --auto --ray-grid 128 --gain 1
LensFlareLab.exe tools/LensFlareLab/evaluation-chart.pfm ghosts.pfm --auto --ray-grid 128 --gain 1 --diffraction off --ghost-report
python tools/LensFlareLab/make_evaluation_chart.py --output build/high-exposure.pfm --light-scale 4096
LensFlareLab.exe build/high-exposure.pfm high-exposure-output.pfm --auto --ray-grid 128 --gain 1
LensFlareLab.exe build/high-exposure.pfm one-ghost.pfm --position 0.3834 0.3844 --ray-grid 128 --gain 1 --diffraction off --ghost-pair 9 8 --ghost-report
python tools/LensFlareLab/make_evaluation_chart.py --output build/off-axis.pfm --light-scale 4096 --single-light-position 0.1 0.1
LensFlareLab.exe build/off-axis.pfm off-axis-flare.pfm --auto --ray-grid 64 --gain 1 --diffraction off --ghost-report
```

The tracked PNGs are display-mapped previews. The PFM chart is generated on
demand and is not stored in the source tree.

## Limits and next evaluation gates

The prescription table gives d-line refractive indices and Abbe numbers, not
full Sellmeier coefficients. The lab approximates RGB dispersion with those
Abbe values and uses one idealized coating layer; the real coating stack and
manufacturing scatter are unknown. The aperture diffraction is a discrete FFT
approximation, and source visibility/occlusion is not modeled. The CPU ray
tracing takes about 1.6 seconds at 640x360 and ray-grid 64, and about 7 seconds
at ray-grid 128 on the development machine after excluding the multi-stop
paths and correcting the rear-group placement. It remains a look-development reference, not an editor frame path.

Next: compare ghost coordinates against Fig. 10's analytical ray-traced reference in the [2024 paper](https://link.springer.com/article/10.1007/s00371-024-03625-7)
and real lens captures, replace the Abbe/coating approximations with
measured optical data, and precompute ray-transfer maps before an
allocation-bounded GPU integration.
