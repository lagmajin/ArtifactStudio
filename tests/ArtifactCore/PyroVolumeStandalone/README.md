# Pyro volume standalone R&D

This small CMake project links the existing `PyroSimulation` and
`CPUVolumeRenderer` implementations without modifying the application or
`ArtifactCore` child repository. It seeds a genuinely three-dimensional
density, temperature, and velocity field, advances 24 frames (48 fixed steps),
then renders the resulting snapshot from front and side cameras.

The first fixture initializes fields directly to inspect solver transport.
The repeated-emitter fixture uses `PyroSimulation::setSources()` and the
normal fixed-step source/advection path. It compares one centered emitter, a
four-source rotating ring, and deterministic multi-band curl inputs. The
emitters stay fixed so buoyancy and the simulated velocity field determine
plume rise. The test-only curl remains an experimental force adapter.

The tests render front and side views, including lighting and multiple
scattering. This is a CPU-reference appearance experiment. It does not claim
GPU, viewport, cache, or performance parity with Houdini or Cinema 4D.
The first case compares direct light, shadows, and scattering on a fixed field,
then resamples that same field to 48×72×48 and sweeps shadow step widths while
holding physical ray distance constant. Shadow density scale is calibrated per
step width because the current CPU marcher adds density samples without an
explicit path-length factor.
The same case also adds static, mass-normalized three-band density modulation
at 0.25 m and 0.125 m wavelengths to the resampled snapshot. This is a
renderer/detail visibility probe only: the noise is not advected and is not a
physical turbulence model. The altered images remain close to the broad base
puff; field Laplacian energy rises slightly while image coverage expands, so
this method adds grain rather than convincing billows.
The temporal curl-noise case adds a density-windowed, discrete curl of a
time-varying vector potential before each fixed Pyro step. It runs three seeds,
four weak strengths (0, 0.03125, 0.0625, 0.125), and captures three adjacent
terminal frames. At strength 0.0625, density Laplacian RMS rises about 1.3–1.5×
over the no-curl control, with roughly 8% extra mass loss over 24 frames.
Post-projection divergence RMS remains below 0.003. Images show asymmetry and
mottling but not yet convincing layered billows; the measurements are a
solver-coupled appearance probe, not DCC renderer parity.
Two focused diagnostics strengthen those findings. On a deterministic 24³
compressible velocity field (input divergence RMS 0.157845), a test-only copy
of the current pressure update/projection gives 0.188262 / 0.241493 / 0.294179
after 8 / 32 / 96 iterations. A test-only Jacobi operator matched to the
central-difference divergence/gradient gives 0.067230 / 0.007853 / 0.000247.
This is an operator comparison, not a proposed production patch. A scalar
persistence probe with density dissipation 0.6/s measures mass 64 before and
after one 1/60 s step, while a source-only step produces mass 0. The diagnostics
reproduced the snapshot-ordering and pressure-projection defects, then were
converted to regressions and fixes in the ArtifactCore child repository. They
now pass against the actual production `step()` path.
The repeated-emitter case compares a centered production source, a four-source
rotating ring, two-band density-windowed curl at 1× and 16× force strengths,
and a half-wavelength curl band. The stronger field changes mass and trajectory
more than it adds visible small-scale billows; halving the wavelengths also
shows no clear benefit.
Adjacent-field cosine and relative L1 change are logged across the source
sequence as coarse temporal diagnostics; high similarity does not establish
billow coherence.

## Build and run

```powershell
cmake -S tests/ArtifactCore/PyroVolumeStandalone `
  -B build/smoke-pyro-volume-rnd `
  -G "Visual Studio 18 2026" `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_INSTALLED_DIR=J:/dev/ArtifactStudio/build/vcpkg_installed
cmake --build build/smoke-pyro-volume-rnd --config Debug `
  --target ArtifactCorePyroVolumeSmokeTest
$env:ARTIFACT_PYRO_VOLUME_OUTPUT_DIR = `
  'J:\dev\ArtifactStudio\build\smoke-pyro-volume-rnd\captures'
& 'J:\dev\ArtifactStudio\build\smoke-pyro-volume-rnd\Debug\ArtifactCorePyroVolumeSmokeTest.exe'
```

The test emits PPM captures when `ARTIFACT_PYRO_VOLUME_OUTPUT_DIR` is set.
The new force-versus-rotation fixture runs a fixed 3D emitter through the
production `setSources()` and `step()` path for two seeds. It compares no added
disturbance, a density-windowed discrete curl force (0.02), and local
Rodrigues rotation of the existing velocity (0.15 radians), capturing the last
three frames. Both treatments remain visually close to the control. Rotation
preserves local speed to relative error below 5e-8 before solver integration,
but lowers final mass by about 4% and does not increase density high-frequency
energy. The curl force changes high-frequency energy by less than 1% and has no
clear visual billow gain. This fixture is evidence against treating either
simple perturbation as sufficient for DCC-like smoke.

Debug timing is reported only as instrumentation of these test adapters and
this solver setup, not as product performance. The curl adapter evaluates a
three-band vector potential at six neighboring samples for every grid cell,
while the rotation adapter skips low-density cells and boundaries; their
reported roughly 42 ms versus 0.24 ms per application are therefore not a fair
algorithm or GPU comparison. Full simulation totals include the solver and
reflect sequential test order. Render times are similar at about 8 ms for the
three captured 144² frames.
A focused advection-only diagnostic isolates trilinear interpolation from
pressure projection and rendering. For a phased 3D density sinusoid, eight
repeated subcell backtraces of 0.25 cell retain 88.0%, 62.8%, 15.3%, and 0.38%
of the fitted amplitude at wavelengths 16, 8, 4, and 2 cells. At 0.5-cell
backtraces the corresponding retention is 83.3%, 52.1%, 6.2%, and effectively
zero. At exactly 1 cell per step, the sampled grid patterns retain about
94.5–95.9% across these wavelengths. The test's linear reference and the
production `PyroFieldSet::sampleScalar()` agree to reported precision.
Thus semi-Lagrangian detail loss depends strongly on subcell displacement and
phase; it is not a single fixed high-frequency damping factor. This probe
isolates interpolation only and does not include the solver's pressure,
velocity evolution, dissipation, or rendering.
A test-only BFECC probe was compared with first-order trilinear advection on the
same 0.25-cell translation for eight steps. At 8/4/2-cell wavelengths, linear
amplitude retention is 62.8%/15.3%/0.38%; limited BFECC retains
60.7%/11.5%/0.12%, and unlimited BFECC retains
60.9%/12.0%/0.14%. The limiter prevents negative density in this fixture, but
neither variant restores detail. Both drift in total mass (for the 8-cell wave,
+9.57 / +4.23 field units for limited / unlimited; for the 4-cell wave,
-4.36 / -12.05). Do not infer general BFECC behavior from this uniform
translation with a finite window; it is a negative result for this specific
prototype and not a production advection change.
A 2×-resolution detail-field experiment runs a fixed production Pyro emitter for
16 frames, transports a separate 64×96×64 scalar field with the coarse Pyro
velocity, reconstructs the coarse density at that resolution, and compares
mass-normalized renders with no detail, static detail, and advected detail at
modulation amplitudes 1.5 and 2.5. All variants have the same summed density
(1070.25). After transport, detail RMS is 0.427 (from 0.439) and its voxel
high-pass RMS is 0.0665 (from 0.0732), so most of the detail field survives.
The rendered high-pass metric is 0.1194 for the base, 0.1410 static, 0.1265
advected at 1.5, and 0.1441 advected at 2.5. The contact sheet shows static
mottling and advected vertical streaks/local dark holes, not coherent smoke
billows. This indicates field detail can survive separately from coarse
advection, but reconstruction/art direction does not yet turn it into a
convincing result. Three 2× scalar buffers occupy 4.5 MiB; test-only Debug
detail transport took 4.12 s for 16 frames on this run. This is not a product
performance measurement and does not alter the renderer or solver.
The same advected 2× detail field was also applied as a displacement along the
normal of the reconstructed coarse density, instead of multiplying density.
World-space offsets 0.02 / 0.04 m changed raw total mass by +0.4% / +3.0%; a
single global correction of 0.996 / 0.971 restored the base sum 1070.25. The
0.04 m offset expands occupied pixels by about 7% and raises image high-pass
RMS only 4% over base. The contact sheet shows a slightly broader lower edge,
not finer billows. Small offsets preserve the broad form but add little visible
detail; larger offsets primarily reshape the plume. This normal displacement
is still a test-only image-space reconstruction experiment, not a fluid solver
or a renderer feature.

The normal-displacement comparison was extended to front, side, and top views.
At 0.04 m, occupied pixels change from 3897 to 4169 in front, 3889 to 4208
from the side, and 4728 to 4730 from above. The contact sheet
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_multiview_contact.png`
shows broader silhouettes in front/side and almost no top-view coverage change.
This is view-dependent silhouette reshaping, not a stable volumetric billow;
changed-pixel counts alone are not a quality metric. Full standalone CTest
passed 1/1 in 48.33 s after this addition.

A follow-up applies a smooth, analytically divergence-free curl vector field as
a coordinate warp to the same advected detail field. A coarse warp (0.75
low-resolution cells) changes image high-pass RMS only from 0.1194 to 0.1195.
Doubling the curl frequency and using a 1.5-cell warp raises it to 0.1666
(+39.6%), while the mass correction is 0.9969 and occupied pixels change
3897→4147 front, 3889→3971 side, and 4728→5131 top. The contact sheet
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_fine_curl_warp_contact.png`
shows sharper, irregular edges in all views, but still a soft plume with small
protrusions rather than layered rolling billows. Thus frequency and amplitude
can create visible structure, but the scalar high-pass gain overstates the
perceptual gain and the warped structure is not yet smoke-like. This remains a
test-only reconstruction experiment. The targeted GTest and full standalone
CTest both pass; full CTest took 48.16 s.

The fine curl warp was then animated in time and compared with unmodified
reconstruction and velocity-advected scalar detail over the final three
frames. Mean adjacent-image absolute residuals (8-bit normalized) are
0.002320 / 0.002305 / 0.002398 for base / advected detail / curl phase 0.18
rad per frame. Raising phase speed to 0.9 rad per frame increases the residual
to 0.002971 (+28% vs base), but the contact sheet
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_temporal_contact.png`
shows the same protrusion pattern with only slight movement. This is increased
temporal image change, not coherent billow motion; the scalar-detail variant
also does not improve the residual over base. The four sequences are saved as
`pyro_volume_detail_temporal_condition{0..3}_frame{0..2}.ppm`. Targeted test
passed and full standalone CTest passed 1/1 in 49.12 s.

A further test-only variant generates additional detail each solver substep from
the existing 3D velocity curl, gated by the local density surface band and
density gradient, then transports it with the same velocity. Relative to the
advected-detail-only render, image high-pass RMS rises from 0.1265 to 0.1915
(+51%); versus base it is +60%. The generated field's raw total density is
1088.55 versus 1070.25 base (+1.71%), corrected by 0.9832 before rendering.
Across front/side/top, occupied pixels are 3980/3987/4620 versus
3897/3889/4728 base. Visual inspection reveals banded, curled structures from
front and side and ring-like breakup from above. This is the first tested
variant here with a visible rolling appearance, but the top-view breakup and
single seed leave it unqualified. Its three-frame mean image residual is
0.002432 and centroid motion is 2.02 pixels per adjacent pair, only slightly
above base (0.002320 and 1.94 pixels); these figures do not establish temporal
coherence. Five 2× scalar arrays occupy 7.5 MiB, and the combined detail update
(advection plus local generation) took about 11.75 s for 16 frames in this
Debug standalone run, not a product performance result. Full CTest passes 1/1
in 58.87 s. No production solver or renderer source changed.

Seed robustness was checked by keeping the Pyro velocity/density history and
initial detail field fixed while changing only the phase of the generated
source noise (seed 29 vs 83), at the same generation strength 0.28. The curl
bands reproduce in front and side for both seeds; the top-view ring breakup
also reproduces, so it is a property of this gated source/reconstruction rather
than a single seed accident. Image high-pass RMS is 0.1915 / 0.1876, and
front/side/top occupied pixels are 3980/3987/4620 vs 3986/3983/4577. The
sequences remain similarly slow: adjacent image residual 0.002432 / 0.002417
and centroid motion 2.022 / 2.029 pixels per pair. Raw density sums before
render correction are 1088.55 / 1091.59 (+1.71% / +1.99%). This isolates
noise seed only; it is not a different Pyro simulation seed or a multi-run
solver robustness study. The comparison sheet is
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_vorticity_seed_comparison.png`.
Seven 2× scalar arrays occupy 10.5 MiB; the combined update took 16.67 s for
16 frames in this Debug standalone run. Full CTest passed 1/1 in 62.86 s.

The generated-detail test also advects a second field alongside the first,
keeping the Pyro trajectory and initial detail identical while using seed-83
phase for the source instead of seed 29. Seed 29 / 83 image high-pass RMS is
0.1915 / 0.1876 (seed 83 retains 98%); front/side/top coverage is
3980/3987/4620 vs 3986/3983/4577. Raw mass is 1088.55 / 1091.59, corrected
by 0.9832 / 0.9805 to the same 1070.25 render mass. Both seeds show similar
front/side curl bands and the same top-view ring breakup. Adjacent image
residual is 0.002432 / 0.002417 and centroid motion 2.022 / 2.029 px per
adjacent pair. This varies only the detail-source noise phase: the Pyro source,
velocity history, and base density history are identical, so it does not prove
robustness to different simulation trajectories. Seven detail buffers occupy
10.5 MiB; update time is 16.67 s/16 frames in this Debug test. CTest passed
1/1 in 62.48 s.

A same-history ablation compared combined `densityBand × gradientGate × curlGate`,
`densityBand × curlGate` only, and `densityBand × gradientGate` only. The
combined image high-pass RMS is 0.1915; curl-only is 0.2032 and gradient-only
is 0.1775. The higher curl-only scalar metric does not correspond to the best
shape: front/side/top captures show strong, parallel ring bands from either
single gate, while the combined source softens the ring pattern and retains
more irregular front/side rolls. Coverage (front/side/top) is combined
3980/3987/4620, curl-only 3874/3851/4703, gradient-only 3875/3863/4705.
Raw mass offsets are +1.71% / +1.79% / +0.81% and are globally corrected for
render comparison. This suggests the interaction between vorticity and the
density-gradient gate suppresses the most obvious directional rings; it does
not yet remove top-view breakup or establish physically coherent billows.
Contact sheet: `build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_generation_ablation.png`.
Eleven 2× scalar buffers occupy 16.5 MiB; combined advection/generation took
23.06 s for 16 frames in this Debug standalone fixture. Full CTest passed 1/1
in 68.82 s.

A curl-aligned source uses the local normalized velocity-curl direction to
orient the generated noise phase, with the same combined density/gradient/curl
gates and update strength. Its image high-pass RMS is the largest at 0.2211,
but the contact sheet
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_generation_vector_alignment.png`
shows vertical streaks from front/side and radial spokes from above. Coverage
is 4033/4048/4824 vs 3980/3987/4620 for the scalar combined source; luma is
lower in front/side and materially lower above. Raw density is 1099.45
(+2.70%), corrected by 0.9734. Directional alignment replaces the previous
rings with a different, more obvious anisotropic artifact, so it is rejected
at this setting. Thirteen 2× scalar arrays occupy 19.5 MiB; update cost is
26.76 s/16 frames in the Debug fixture. Full CTest passed 1/1 in 72.12 s.

A surface-tangent variant projects the fixed noise wavevector onto the plane
perpendicular to the local density gradient before evaluating the generated
source. Against the scalar combined source, image high-pass RMS drops
0.1915→0.1561. Front/side coverage drops 3980/3987→3860/3856 and shows
vertical streaking; top coverage is 4620→4691 (base 4728), and the radial
spokes become softer mottling rather than disappearing. Top luminance is
246235 versus 243289 combined and 246591 base. Raw mass is 1070.74 (+0.05%),
with correction 0.99954, substantially better conserved than previous warps.
Three-frame image residual is 0.002581 and centroid motion 2.025 px/pair, so
there is still no clear temporal billow. Contact sheet:
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_tangent_alignment.png`.
This trades top-view radial structure for softer top mottling and front/side
streaks; it is not a qualified visual improvement. Fifteen 2× scalar buffers
occupy 22.5 MiB and update took 31.57 s/16 frames in Debug. Full CTest passed
1/1 in 77.76 s.

A three-band tangent-plane source combines three projected wavevectors with
separate wavelengths, phase offsets, and weights 0.55/0.30/0.15. Compared
with the single-band tangent source, image high-pass RMS falls 0.1561→0.1359;
front/side/top coverage is 3878/3866/4700 vs 3860/3856/4691. The top-view
pattern is more diffuse, but front/side vertical streaks remain and fine
structure is softer than both single tangent and scalar combined generation.
Mass correction is 1.0 (raw mass matches base at reported precision). Three-frame
image residual is 0.002456 and centroid motion 2.012 px/pair, only a small
change from single tangent (0.002581 / 2.025 px). The contact sheet
`build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_multiband_tangent.png`
shows no coherent rolling billows. This multi-band tangent source improves
mass behavior and diffuses the top pattern but does not improve overall smoke
quality. Seventeen 2× scalar arrays occupy 25.5 MiB; update took 35.44 s/16
frames in Debug. Full CTest passed 1/1 in 81.32 s.


A surface curl-potential warp computes a test-only displacement as density-gradient crossed with a three-band scalar-potential gradient. It removes most visible fine structure: image high-pass RMS is 0.1196, essentially the 0.1194 base value, while front/side/top coverage is 3898/3869/4710. Raw mass is 1.35% below base before a global 1.0137 correction. Three-frame residual 0.002322 and centroid motion 1.948 px/pair are close to base (0.002320 / 1.945), with no visible coherent roll. The contact sheet `build/smoke-pyro-volume-rnd/captures/pyro_volume_detail_surface_curl_potential.png` confirms an overly smooth plume from all views. This variant is not promoted. Full standalone CTest passed 1/1 in 82.21 s.


A test-only canonical vorticity-confinement adapter was compared with the current Pyro setting and with confinement disabled. The adapter computes `omega = curl(velocity)`, `N = grad(|omega|)/|grad(|omega|)|`, and adds `epsilon * voxelSize * dt * (N × omega)` before the production `step()`. Epsilon 1/2/4/8/16 was swept at 32×48×32; every condition used the same 16-frame centered emitter and was rendered from front, side, and top. The contact sheet is `build/smoke-pyro-volume-rnd/captures/pyro_vorticity_confinement_contact.png`. Epsilon 1 through 4 produces no visible rolling billows; at 8 and 16 the plume pinches/loses mass instead. Mass changes from 133.68 (off) to 125.60 (epsilon 4), 118.84 (8), and 116.42 (16). Divergence RMS rises from 0.0141 to 0.0344 at 16. The current setting (0.45) is visually similarly smooth. This rules out simply increasing confinement as a quality path for this fixture; it does not prove the canonical force is generally ineffective. Full standalone CTest passed 1/1 in 85.83 s.

Donor documentation frames smoke shaping as several distinct controls: Houdini separates disturbance, shredding, turbulence, and confinement; Cinema 4D separates Vorticity from scale/octave/time-varying Turbulence. The next experiment should compare persistent multi-scale curl-noise turbulence with speed-preserving shredding at the same spatial envelope, rather than raising confinement alone. References: [SideFX Pyro Solver Sparse](https://www.sidefx.com/docs/houdini/nodes/dop/pyrosolver_sparse.html), [Maxon Pyro Scene](https://help.maxon.net/c4d/2024/en-us/Content/html/OPBDSCENE-PBDSCENE_GROUP_PYROPROPERTIES.html), [Fedkiw, Stam & Jensen, Visual Simulation of Smoke](https://physbam.stanford.edu/papers/stanford2001-01.pdf).


A follow-up normalizes three-band curl turbulence and speed-preserving shredding to the same local input velocity-change fraction (5% or 15%), using the same time-varying vector potential, density envelope, wavelengths (0.36/0.18/0.09 m), fixed source, and two noise seeds. The contact sheet is `build/smoke-pyro-volume-rnd/captures/pyro_matched_curl_turbulence_shredding.png`. At 5%, neither variant forms clear rolls; both reduce final mass by about 5% in this solver. At 15%, curl turbulence creates larger local clumps but mass falls about 19%, and divergence RMS rises to 0.026–0.038. Shredding preserves the adapter's pre-step velocity magnitude (relative error below 4e-8) and has lower post-projection divergence (0.007–0.009), but mass still falls about 17% and images show displaced blobs rather than rolling smoke. Density high-frequency RMS rises at 15%, but the effect varies by seed and is not a visual-quality score. This comparison is better amplitude matched than the earlier force-vs-rotation test; the kick is defined per fixed step, so timestep/substep independence remains untested. The full standalone CTest passed 1/1 in 86.57 s.


A resolution × substep comparison holds the world-space domain, emitter position/extents/velocity, source settings, and 0.4 s simulated time constant. It compares 32×48×32 and 48×72×48 grids, each at 60 Hz and 120 Hz fixed steps; pressure iterations scale from 12 to 24 with grid width. The 120 Hz step is executed twice for every 60 Hz frame. Contact sheet: `build/smoke-pyro-volume-rnd/captures/pyro_grid_substep_contact.png`. At fixed resolution, halving the step changes world-integrated density mass only about 2% (32 grid) / 1.2% (48 grid) and leaves the broad puff visually similar, while divergence RMS falls about 40% / 36%. Raising resolution changes world mass by about +5.8% at 60 Hz and +5.0% at 120 Hz; world-space density gradient RMS rises about 28%, but no rolling structures appear and front/top coverage actually decreases. The per-voxel high-frequency RMS is not directly comparable across grid scales. Full standalone CTest passed 1/1 in 92.25 s. This indicates substeps improve projection consistency in this case, while grid refinement alone does not create art-directed smoke.


An emission-pulse experiment compares 0.1, 0.2, and 0.4 s source durations with rate scales 2.0, 1.0, and 0.5. Density, temperature, and injected velocity are all scaled, keeping their nominal time integrals equal to 0.2 source-seconds. Each source is explicitly cleared after its pulse; the simulation then advances to 1.2 s. Front evolution sheet: `build/smoke-pyro-volume-rnd/captures/pyro_emitter_pulse_evolution_front.png`; final views: `build/smoke-pyro-volume-rnd/captures/pyro_emitter_pulse_final_views.png`. At 1.2 s, world-space density mass is 0.000776 / 0.000813 / 0.000892 and vertical centroid is 0.476 / 0.467 / 0.448 m for short/medium/long pulses. All become similarly smooth single blobs; no rolling billows appear. From the pulse-end capture to 1.2 s, world mass falls about 55% / 52% / 43%, much more than the configured 0.035/s density dissipation alone would suggest. This is observed mass loss, not yet attributed to a specific solver stage. Temporal density cosine remains high (0.9986–0.9990) while relative L1 change averages 8.5–8.9% per frame. Full standalone CTest passed 1/1 in 94.88 s. The pulse-rate normalization is only nominal: nonlinear buoyancy, advection, source timing, and grid discretization can still change resulting mass and shape.


A trace-distance/boundary-mode diagnostic repeats the 0.2 s emitter pulse at source speeds 60/120/240 and runs each with Open and Closed boundaries. It logs maximum and density-weighted backtrace distance in grid cells per step, boundary-shell density, divergence, and world-integrated mass at pulse end and 1.2 s. Contact sheet: `build/smoke-pyro-volume-rnd/captures/pyro_trace_boundary_contact.png`. Open-boundary mass retention is 67.9%/59.7%/48.4% as source speed rises; Closed is 67.6%/59.2%/47.6%. Boundary-shell mass remains zero in all six conditions, so boundary outflow does not explain this case. Density-weighted trace distance is only 0.054/0.104/0.196 cells per step at pulse end; maximum is below 0.38 cells. Higher source speed also raises divergence RMS (0.0027/0.0054/0.0107 at pulse end). This supports a relationship between stronger flow/compression and mass loss, but does not isolate interpolation from divergence/projection. Although `PyroSimulationSettings::advectionClamp` is set to 1.0 in the fixture, the production backtrace path inspected here does not read that setting. Full standalone CTest passed 1/1 in 97.18 s.
