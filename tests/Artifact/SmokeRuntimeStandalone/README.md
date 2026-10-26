# Smoke Runtime Image R&D

This independent project compiles the production
`ArtifactLayerFluidRuntimeState.cpp` and its module interface unchanged. It
uses a test-only `Graphics.ParticleData` API shim and a small test
implementation of `ArtifactIRenderer` to capture the production
`ParticleRenderData` submission. The PNG rasterizer visualizes those submitted
particle positions, sizes, colors, and alpha with a deterministic additive
falloff; it does not run the Diligent GPU renderer or claim pixel parity with
the application's viewport.

The test verifies that the production runtime submits visible particles, the
particle cloud rises above the source, repeated runs match pixel-for-pixel, and
moving the emitter changes the PNG while keeping the plume inside the frame.
It writes `smoke_runtime.png`, `smoke_runtime_repeat.png`, and
`smoke_runtime_shifted.png`.

Configure and build this directory as its own CMake project, then run the test
with `ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR` set to an absolute output directory.

`EvolvesProductionTwoDimensionalSmokeStateInsideDepthResolvedPyro` is a
test-only 3D Pyro bridge probe. It maps an unchanged production 96x72 2D smoke
snapshot into a 48x36x32 volume, sweeps the mirrored centered-divergence /
Jacobi-pressure / centered-gradient projection over 4 to 72 iterations, and
evolves control, depth-curl, and density-coupled buoyancy branches for 24
steps. The projection sweep reduces density-weighted divergence RMS from
1.43771 to 0.58845 at 24 iterations and 0.32296 at 72. The 72-iteration
branch ends with lower divergence (0.15277 vs 0.36391) but also lower density
mass (2324 vs 2249) and a lower Y centroid (26.48 vs 27.34 voxels); neither
branch develops a visible folded billow. Initial temperature set to 1.5x
density with buoyancy 0.7 similarly leaves the 24-step centroid effectively
unchanged (27.34 vs 27.34). This focused standalone test passed 1/1 (8.78 s).
Front and side PNGs are written under the configured capture directory in
`depth-evolved-2d-state-rnd/`. This experiment does not change production Core
or App code and does not establish GPU viewport parity or DCC-level quality.

`ComparesThreeDimensionalSourceVelocityFieldsForSmokeRollup` creates a true
3D density/temperature/velocity field from the Pyro source, clones its
12-step pulse-end state into the original control / curl / timestep / gate
branches, and advances those for another 1.2 s while continuing the emitter.
Four additional source-shape / low-jet thermal branches each generate their
own 12-step initial state and continue for the same 1.2 s.
The current branches compare source velocity, initial divergence-free curl at
two amplitudes, half velocity, 1/120 s fixed steps, and equal-strength
large-scale curl acceleration gated to smoke interior, density interface, or
ambient air near the interface. The ambient-interface gate is repeated with
noise seeds 17, 29, and 71. In the preceding source-free pulse test, the
control mass / Y centroid / divergence RMS were 15.8958 / 13.0075 / 0.02923;
the interior / interface / ambient gates yielded 15.4125 / 12.9594 / 0.03493,
15.6480 / 12.9470 / 0.03490, and 15.8346 / 12.8955 / 0.03867. Under
continuous emission, control mass / Y centroid / divergence RMS are 253.058 /
15.6979 / 0.03442. Smoke-interior, interface, and ambient gates yield
240.484 / 15.2663 / 0.03315, 241.277 / 15.2788 / 0.03078, and 249.095 /
15.5247 / 0.03648. Ambient gating increases side occupied pixels from 8,091
to 8,456 (4.5%); interface gating raises them to 8,170 (1.0%) while reducing
mass by 4.7%. After scaling each terminal density field to the control mass,
side occupied pixels are control 8,091, interior 8,016 (-0.9%), interface
8,113 (+0.3%), and ambient 8,429 (+4.2%). Ambient widening persists after
mass matching, but visual review still reads as a broader smooth puff, not a
folded billow.
The initial force support counts are 342 / 308 / 399 cells for interior /
interface / ambient masks; force RMS is normalized inside each support.
Focused GTest passed 1/1 (30.68 s); raw and mass-matched front/side captures
are in `build/smoke-runtime-rnd/captures/3d-source-interface-massmatch-20261026/3d-source-rollup-rnd/`.
This test-only comparison does not justify a production force or timestep
change and does not demonstrate DCC-level smoke quality.

The follow-up repeats only the ambient-interface gate with noise seeds 17, 29,
and 71. At mass-matched captures, side occupied-pixel deltas against control
at steps 24 / 48 / 72 are +0.46% / +1.83% / +3.10% for seed 17,
-1.80% / -2.15% / -2.52% for seed 29, and +0.37% / +4.03% / +4.31% for
seed 71. Thus two seeds widen later in the plume while one consistently
contracts; this is not a seed-robust effect. Visual inspection of the step-72
control and seed-71 side views shows only a slightly broader smooth puff, with
no folded billow. The focused test passed 1/1 (47.95 s); captures are in
`build/smoke-runtime-rnd/captures/3d-source-gate-seeds-20261026/3d-source-rollup-rnd/`.

The current follow-up initializes wide and tall source shapes independently
from step 0, then continues each for 1.2 s. Their density injection rates are
normalized by numerically integrating the same voxel-center falloff used by
`PyroSimulation`, before independently scaling terminal images to control
density mass. At step 72, mass-matched side occupancy is 14,118 pixels for the
wide source and 12,212 for the tall source, versus 8,091 for control; their Y
centroids are 17.20 and 18.70 versus 15.70. Both also increase divergence RMS
to 0.0577 / 0.0686 from 0.0344, and visual review shows smooth broad columns
without folded billows. This is a source-shape effect, not evidence of a more
realistic smoke solver.

For a thermal sensitivity check, three independently initialized branches use
the same low source velocity (30 versus the control jet's 240): baseline,
8x buoyancy, or 12x source temperature. At step 72, mass-matched side
occupancy is 823 / 897 / 1,144 pixels, and Y centroids are 8.18 / 8.36 / 8.61.
Temperature changes the low-jet plume more than buoyancy in this setup, but
the low-jet CPU volume captures contain a dark shadow artifact; they are not
used as evidence of visual quality. Focused GTest passed 1/1 (56.06 s), with
captures in `build/smoke-runtime-rnd/captures/3d-source-shape-thermal-20261026/3d-source-rollup-rnd/`.

`RechecksPyroSourceShapeAcrossSeedsAtHigherResolution` compares control / wide /
tall source shapes at 72x54x48 voxels, with 96 pressure iterations in every
branch and two phases (17 / 71) of a bounded height-varying lateral shear.
The production confinement applies only in smoke density, using a smooth gate.
Removing that gate in an exploratory run raises strength-8 divergence to
.111 / .151 for the two phases and lowers the mass-matched image luma, so the
unlocalized force is not used.
At step 60, mass-matched side occupied pixels are 3,976 / 6,172 / 5,476 for
control / wide / tall at phase 17, and 3,982 / 6,195 / 5,557 at phase 71.
The broad source effect therefore persists across both phases at higher
resolution. Y centroids are 15.95 / 16.74 / 18.08 and 15.91 / 16.71 / 18.06;
images remain smooth caps/columns without folded billows. Terminal divergence
RMS ranges .024-.046; full-plume residuals are not monotonic as iteration
count changes because each pressure setting produces a different evolving
trajectory. The separate one-step projection diagnostic holds the input state
fixed: divergence RMS drops from .72898 before projection to .60667 / .54278 /
.44738 / .31718 at 12 / 24 / 48 / 96 iterations. In the same high-resolution
fixture, the Core vorticity implementation is exercised at strengths 0 / 3 / 8.
At strength 8, side occupancy is 6,350 / 6,360 versus 6,122 / 6,149 for the
wide-source control at phases 17 / 71; divergence RMS is .0272 / .0377 versus
.0264 / .0260. Captures show mild stacked lobes in both phases, but no clear
folded billows. Focused tests passed: 28.38 s for source shape, 0.35 s for
projection, and 91.01 s for the Core confinement sweep plus projection check.
Captures are under
`build/smoke-runtime-rnd/captures/3d-source-production-curlconf-20261026-optimized/`.

An earlier value-noise scale sweep of this same case compared deterministic,
smooth 3D value-noise vector potentials applied as density-gated curl
acceleration, with equal density-weighted force RMS for large
(0.75/0.48/0.30 world unit), fine (0.30/0.20/0.14), and multiscale
(0.75/0.40/0.22/0.13) bands. At 1.2 s, large-slow, large-pulsed, fine-slow,
and multiscale-slow yield mass / Y centroid / divergence RMS 15.0566 /
12.8817 / 0.03792, 15.6777 / 13.0178 / 0.03735, 15.7103 / 12.8637 / 0.03512,
and 15.0465 / 12.8511 / 0.03358 respectively. None creates folded billows;
image changes are small cap shifts and coverage variations. That focused run
passed 1/1 in 39.0 s after computing each vector-potential voxel once per step
(the initial repeated-sampling version took 142.7 s with identical metrics).

The `ReportsSteadyStateRenderDataPreparationCost` case is an R&D microbenchmark.
It advances 32x32, 79x79, 128x128, and 512x512 smoke states for 24 frames,
warms render-data preparation 12 times, then reports mean and p95 over 100
calls. Run it with a Release build for useful CPU numbers. It measures CPU
particle-data preparation and compares integer density/velocity reads with
bilinear field reads at the same jittered sample positions. Solver stepping and
GPU upload/render are excluded, so it is not a full viewport frame-time
measurement.
It also times the velocity-free runtime submission path used by screen-aligned
GPU smoke particles; the test renderer captures data but does not submit GPU
work.
## Reconstruction quality R&D

`ReportsCappedGridReconstructionParameterTradeoffs` rasterizes one fixed production
runtime particle stream with five reconstruction settings: the production
baseline, the same deterministic sample hash with jitter reduced from 0.30 to
0.10, an 18% larger sprite kernel, and 25% / 40% larger kernels with area-based
alpha compensation. It reports visible coverage, green channel excess, and
saturated pixels, and writes five PNGs when
`ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR` is set. This isolates reconstruction from
solver variation; it is a proxy image, not a D3D12 rasterization reference.

`ComparesCommonFieldReconstructionAcrossDensityResolutions` upsamples one
fixed 96x72 solver field to 192x144 and compares it at the same output size
against native reconstruction and a separately simulated high-resolution
field. `ReportsVelocityAdvectedSmokeDetailAcrossFramesAndSeeds` tests bounded
MacCormack transport of a seeded scalar detail pattern through the production
runtime's captured velocity over frames 20-22 at the production-default 20
solver iterations and seeds 17, 29, and 71. It also generates zero-mean detail
from the current density interface by projecting a smooth multi-scale noise
gradient onto the local interface normal and tangent. The orientation variants
are render-only density perturbations; they do not alter the captured runtime
velocity. The test reports mass, plume centroid, visible occupancy, spatial
high-pass energy, and writes orientation captures under
`captures/2d-interface-direction-rnd/` when run with a matching output folder.
`ReportsProductionIterationVorticityDetailTradeoff` compares vorticity
confinement strengths 0.1, 0.8, and 1.5 at the same solver iteration count.
These are offline CPU reconstruction diagnostics; they do not modify the
production solver or represent viewport/GPU performance.

The interface-direction experiment uses amplitude 0.8 for a low-signal check,
then amplitude 4.0 with modulation clamped to ±0.2 (density multiplier remains
positive) to make the effect visible. Both interface-normal and interface-
tangent variants compare against the same frame and seed; the high-amplitude
case preserves mass within 0.048% and keeps the X centroid within 0.35 solver
cells, but adds only modest image high-pass energy and slight surface roughness,
not billows.
The same three captured frames are also aligned by the density centroid using
integer output-pixel translation, and a radius-1 high-pass residual is measured
between adjacent images. Baseline residuals are 0.00264 / 0.00267 for frames
20→21 / 21→22; the normal and tangent variants range 0.00266–0.00270, about
0.3–1.5% higher. This is only a translation-registered image diagnostic; it
does not compensate for local flow or establish perceptual flicker.
An additional flow-backtrace registration samples the captured frame velocity
and uses the solver's `dt*(gridDimension-2)` displacement before comparing the
high-pass images. Baseline residuals drop to 0.002124 / 0.002174; normal detail
changes them by -0.61%..+1.84%, while tangent detail raises them by 0.60%..+2.11%
across seeds and adjacent frame pairs. This is a one-step backward advection
approximation, not optical flow; source injection during each frame remains in
the measured residual.
The same flow-backtrace metric is repeated only for normalized grid y<0.68,
excluding the emitter centered at y=0.78 and both current/backtraced pixels
outside the upper-plume region. Baseline ROI residuals are 0.002322 / 0.002391,
about 9–10% above the full-frame values. Normal-detail changes range
-0.96%..+2.08%; tangent detail remains +0.33%..+2.27%. Thus the emitter footprint
does not dominate the temporal residual; this ROI still includes smoke that
was emitted earlier and advected into the cap/stem.

`ComparesFixedDensityOpticalDepthAndDirectionalSelfShadowRendering` uses the same frame-34 production 2D smoke density for an unlit optical-depth image, a directional in-plane self-shadow approximation, an ambient-fill variant, and a density-gradient normal variant. The ray/shading model is deliberately a test-only 2D proxy, not physical volume integration or a production shader. It writes four PNGs when the output directory is set. On the current field, unlit / direct-shadow / ambient-fill / gradient-lit mean luminance excess is 7.60 / 3.77 / 4.84 / 5.98 and luminance standard deviation is 17.79 / 8.62 / 10.78 / 13.97. Visual review shows the shadow variants darken or flatten the broad plume rather than add coherent billows. This indicates that lighting alone cannot compensate for the current smooth 2D density field; next compare a depth-varying 2.5D field through the existing CPU volume renderer.

`RendersDepthVaryingVolumeFromTheSameTwoDimensionalSmokePlume` adds the existing CPU volume renderer to this standalone test target and seeds it with a frame-34 production `FluidSolver2D` density snapshot. It compares front and side views for a uniform-depth extrusion, a Gaussian depth envelope, and a multi-scale noise-warped depth field. The warped field is normalized per screen-space column so its integrated density matches the original 2D column. CPU volume lighting uses the existing smoke transfer function with directional lighting, shadows, and multiple scattering. PNGs include both linear-output and display gamma-encoded captures; the gamma encoding is applied only to the test captures because `CPUVolumeRenderer::render()` writes linear values directly to 8-bit channels.

Current 160x160 linear image stats (occupied pixels / luminance sum / occupied-pixel luma standard deviation): uniform extrusion front 13,494 / 238,854 / 2.96, side 14,062 / 223,115 / 2.30; Gaussian-depth front 13,663 / 205,051 / 3.14, side 12,226 / 181,391 / 2.61; warped-depth front 13,705 / 201,543 / 2.78, side 12,034 / 178,030 / 3.14. The side view confirms that depth variation changes the silhouette and local contrast, but this single-slice extrusion still reads as a thin sheet, with periodic/warped structures that are not yet coherent billows. This test establishes the 2D-snapshot→3D-volume renderer path, not a production feature or DCC-quality result. The full independent suite currently passes 26/26 with ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR configured.


`ComparesStaticTwoPointFiveDWithTimeEvolvingThreeDimensionalEmitter` maps the same 2D emitter center/radius into a 48×36×32 open Pyro domain, runs a real 3D Pyro source for 36 frames × 2 fixed steps (1.2 s), and compares it with a static Gaussian-depth extrusion of the 2D runtime frame at the same time. Front and side images use the same CPU volume renderer; three terminal frames are captured. The test also compares an existing-style density-windowed temporal curl force at strength 0.0625. Baseline final density mass / centroidY,Z is 225.1 / (14.14,15.50); mean adjacent-field cosine/L1 are 0.9938 / 0.1473. Curl mass / temporal cosine/L1 at the last three frames are 213.7, 219.4, 217.1 / 0.9944 / 0.1436. Final density Laplacian RMS drops from 0.02341 baseline to 0.02185 with curl, and equal renderer densityScale 7.2 gives front/side occupied pixels 6,423/6,824 baseline versus 6,207/6,670 with curl. Visual review shows the 3D plume rising but staying a soft rounded puff; this curl force adds no visible billows. At the default renderer densityScale 1.8, the plume is very faint (1,204/1,611 occupied front/side pixels), so the 7.2 capture is included as an exposure/transfer sensitivity view, not a recommended product value. Density masses across 2D and 3D are not directly comparable because the 2D-to-volume conversion extrudes each screen-space column across 32 depth cells. Captures are in `build/smoke-runtime-rnd/captures/3d-emitter-rnd/`. This is test-only and does not change production defaults or the child repository.

`EvolvesProductionTwoDimensionalSmokeStateInsideDepthResolvedPyro` advances the unchanged production 96×72 smoke runtime to frame 28, maps its density and velocity snapshots into two identical 48×36×32 Pyro initial fields, and runs 24 source-free 3D steps. The Gaussian depth envelope preserves local density amplitude; dividing by the layer count would preserve a 2D column sum but would be incorrect for 3D volume density and made the first exploratory captures too faint. The mapped velocity uses the target grid's cell-rate scale and flips the 2D downward-positive Y axis. Both branches first run the test adapter for the current Pyro centered-divergence/Jacobi-pressure projection (24 iterations); mapped-flow divergence RMS falls from 1.524 before projection to 0.6324 after it. One branch is an unmodified control. The other adds a deterministic density-supported vector-potential curl whose RMS is 15% of the projected base velocity RMS; its discrete divergence RMS is 9.52e-9 and both branches have the same initial total divergence RMS. At step 0/8/16/24, control mass is 2452.88/2438.79/2448.72/2248.86 and depth-roll mass is 2452.88/2410.73/2400.33/2213.79. Final density centroid Y is 27.34/27.02 voxels and Z is 15.5/15.59. Interval field cosine at the final interval is .873/.893 and normalized L1 change is .424/.399 for control/depth-roll. The front view remains a smooth symmetric column/puff; side width and contrast change slightly, but folded billows do not appear. The extra curl branch retains 90.3% mass versus 91.7% for control. Captures are in `build/smoke-runtime-rnd/captures/depth-roll-projected-comparison-20261026/depth-evolved-2d-state-rnd/`. This is a test-only research path; it does not demonstrate DCC-quality motion or GPU viewport performance.

`MeasuresMassNormalizedThreeDimensionalEmitterFootprintSweep` runs the same 48×36×32 Pyro domain for 72 fixed steps with narrow/baseline/wide ellipsoidal sources. Source density is inversely scaled with source-box volume to keep nominal per-step injected density volume constant. Measured terminal mass / voxel-Y centroid / X-Z spread / density Laplacian RMS were narrow 222.66 / 11.39 / 1.35,1.37 / 0.05841; baseline 225.06 / 14.14 / 1.83,1.85 / 0.02341; wide 201.31 / 16.54 / 2.40,2.41 / 0.00993. The normalization is nominal only: discretized source sampling and transport still cause terminal mass to vary. Wider sources rise farther and broaden the plume while reducing high-frequency density; narrower sources retain sharper gradients but rise less. This scalar/shape probe does not provide a renderer capture, so treat the result as quantitative guidance rather than a confirmed image-quality ranking.

`ComparesSingleAndThreeLobedPyroEmitterAtMatchedNominalSourceVolume` compares one centered source against three triangularly arranged sources at separations 0.10 / 0.14 / 0.22 world units for 72 fixed steps. The discrete source falloff kernel is summed on the actual voxel grid and source density is scaled so nominal per-step injection matches the single source. Terminal masses are 225.06 / 200.93 / 199.94 / 205.35; front/side occupied pixels at densityScale 7.2 are single 6,419/6,822, tight triple 6,904/6,988, triple 7,140/8,024, wide triple 6,971/10,646. The tight sources merge into one broad puff; 0.14 retains separate upper lobes; 0.22 produces visibly separate columns, especially in side view. None shows coherent billow folds.

The same test sweeps three 50%-duty pulse periods, 0.2 / 0.4 / 0.6 s, at spacing 0.14. Density, temperature and velocity are doubled during the on half and disabled during the off half; each 1.2 s run contains an integer number of cycles, so the nominal total source integral matches continuous emission. Terminal mass / voxel-Y centroid / front-side occupied pixels are: continuous 199.94 / 11.76 / 7,140,8,024; 0.2 s period 193.15 / 12.41 / 7,969,8,574; 0.4 s period 182.25 / 13.11 / 8,629,9,014; 0.6 s period 176.24 / 14.05 / 8,526,8,833. Longer bursts rise higher but lose more density by 1.2 s; 0.4 s has the greatest render coverage, while 0.6 s pushes higher. This ranking is exposure/coverage data, not a perceptual quality metric. Checkpoints and front/side captures at each half-cycle are saved as `pyro_triple_t*_*.png` and `pyro_triple_pulse_0p*_t*_*.png` in `build/smoke-runtime-rnd/captures/3d-emitter-topology-rnd/`. Pulsing creates distinct smooth puffs, not folded billows. These are CPU volume renderer captures, not GPU viewport parity.

`SeparatesBuoyancyAdvectionAndDensityDecayAfterEmitterPulse` runs a 0.2 s Pyro source pulse, copies identical pulse-end fields into eighteen source-free branches, and evolves each for 1.0 s. Full flow / no-new-buoyancy / buoyancy-from-rest / density-decay-only / static-density-control retain 47.05% / 47.06% / 78.15% / 78.23% / 100% mass, with density-weighted world-Y centroids 0.396 / 0.396 / 0.205 / 0.205 / 0.204 m. The 2-cell boundary shell contains no density in any branch. Pulse-end temperature sum/max/support is 10.184 / 0.1601 / 320 cells (`temperature>1e-4`); after the first buoyancy-only step, 180 active temperature cells remain. At buoyancy .7 and pressure 8, temperature-weighted vertical velocity / curl magnitude is .000697 / .000475; across pressure iterations 1→32, curl stays .000475 while temperature-weighted vertical velocity declines .000844→.000658. At buoyancy 20, the same measures rise to .01990 / .01358. Pressure iteration 1/4/8/16/32 reduces first-step divergence after projection 1.007e-5→5.215e-7 but has negligible 1 s mass/centroid effect; raising buoyancy .7→7→20 raises centroid only to .2053/.2071/.2106 m. This supports that projection removes much of the gradient-like vertical kick while preserving the weaker curl component; longer-run lift remains small. Front/side images are in `build/smoke-runtime-rnd/captures/3d-emitter-decay-rnd/`.

The same test adds three test-only standard vorticity-confinement branches over the pulse's remaining velocity: epsilon 2 and 8 over the full volume, plus epsilon 8 gated by active temperature and relative density gradient. It computes physical-space `curl(u)`, `N=grad(|curl|)/|grad(|curl|)|`, and applies `epsilon*h*dt*(N×curl)` at each source-free step. Against the no-added-confinement baseline (47.05% mass retention, centroidY .396 m), epsilon 2 retains 36.41% / .395 m, epsilon 8 retains 26.94% / .320 m, and the gated epsilon 8 retains 37.18% / .376 m. Stronger confinement raises active-support speed and mass loss; front/side captures remain smooth puffs without coherent folds. The interface gate moderates the force but does not produce visible billows. Captures: `post_pulse_residual_curl_conf_{2,8}_{front,side}.png` and `post_pulse_interface_curl_conf_8_{front,side}.png` under `build/smoke-runtime-rnd/captures/3d-emitter-decay-rnd/`. This adapter is isolated to the R&D test and does not change production Pyro.

Four additional branches isolate density-only shredding from velocity forcing: a no-shred control, 8-cell low-frequency modulation at amplitude .12, 8/4/2.5-cell multi-scale modulation at .12, and the same multi-scale modulation at .28. A smoothly translated deterministic 3D value-noise field is weighted by relative density gradient, mean-corrected by density mass, and applied before each source-free step. The maximum per-step pre-transport mass error is below 6.8e-9; branch velocity metrics remain identical. At 1 s, control / low / multi-.12 / multi-.28 mass is 20.19 / 22.86 / 20.93 / 22.06, and density Laplacian RMS is .00595 / .00957 / .00645 / .01016. The final centroid is .396 / .370 / .394 / .372 m; normalized density L1 change between the 0.5 s and 1 s checkpoints is .496 / .479 / .499 / .489. Thus higher-frequency modulation adds visible edge/interior roughness and a higher Laplacian, but alters later density transport and the stronger conditions shift the plume downward; it does not create folded billows. Half-run and terminal front/side PNGs use `post_pulse_<condition>_{half,front,side}.png` in `build/smoke-runtime-rnd/captures/3d-emitter-decay-rnd/`. This is a test-only conservative density perturbation, not an emitter or solver change.

`ComparesIndependentRollAndFineDisturbanceOperators` now also tests a smooth, analytic Gaussian vortex whose center tracks the 2D plume density centroid. The field is tangent to circles around its center, has a 7-cell radius, and is applied after each runtime frame as a test-only velocity increment; acceleration variants scale that increment by `1/30 s`. A weak 1.2-unit/s acceleration changes final mass from 472.485 to 495.993 and divergence RMS from .0155404 to .0164264, while adjacent image delta is essentially unchanged (.374903 vs .376309). At 3.6 units/s, mass reaches 593.904 and divergence .0189435; after final-image-only mass normalization, visible pixels are 181,560 vs 180,690 baseline. The stronger .24-per-frame increment reaches mass 701.328, divergence .0246548, and adjacent image delta .488417; normalizing only the final density still leaves 215,259 visible pixels and a broad laterally displaced cap. Visual review of both seeds shows cap displacement/expansion, not coherent folded billows. Captures are `smoke_operator_seed*_tracked-vortex-*.png` in `build/smoke-runtime-rnd/captures/tracked-vortex-rnd/`. Final-image normalization is only a shape diagnostic; it does not make the simulation mass-conserving. The focused GTest passed with all 13 conditions across seeds 17 and 71 (45.21 s). This remains independent test code, not a production operator.
