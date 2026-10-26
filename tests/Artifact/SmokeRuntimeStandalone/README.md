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

`MeasuresMassNormalizedThreeDimensionalEmitterFootprintSweep` runs the same 48×36×32 Pyro domain for 72 fixed steps with narrow/baseline/wide ellipsoidal sources. Source density is inversely scaled with source-box volume to keep nominal per-step injected density volume constant. Measured terminal mass / voxel-Y centroid / X-Z spread / density Laplacian RMS were narrow 222.66 / 11.39 / 1.35,1.37 / 0.05841; baseline 225.06 / 14.14 / 1.83,1.85 / 0.02341; wide 201.31 / 16.54 / 2.40,2.41 / 0.00993. The normalization is nominal only: discretized source sampling and transport still cause terminal mass to vary. Wider sources rise farther and broaden the plume while reducing high-frequency density; narrower sources retain sharper gradients but rise less. This scalar/shape probe does not provide a renderer capture, so treat the result as quantitative guidance rather than a confirmed image-quality ranking.

`ComparesSingleAndThreeLobedPyroEmitterAtMatchedNominalSourceVolume` compares one centered source against three triangularly arranged sources at separations 0.10 / 0.14 / 0.22 world units for 72 fixed steps. The discrete source falloff kernel is summed on the actual voxel grid and source density is scaled so nominal per-step injection matches the single source. Terminal masses are 225.06 / 200.93 / 199.94 / 205.35; front/side occupied pixels at densityScale 7.2 are single 6,419/6,822, tight triple 6,904/6,988, triple 7,140/8,024, wide triple 6,971/10,646. The tight sources merge into one broad puff; 0.14 retains separate upper lobes; 0.22 produces visibly separate columns, especially in side view. None shows coherent billow folds.

The same test sweeps three 50%-duty pulse periods, 0.2 / 0.4 / 0.6 s, at spacing 0.14. Density, temperature and velocity are doubled during the on half and disabled during the off half; each 1.2 s run contains an integer number of cycles, so the nominal total source integral matches continuous emission. Terminal mass / voxel-Y centroid / front-side occupied pixels are: continuous 199.94 / 11.76 / 7,140,8,024; 0.2 s period 193.15 / 12.41 / 7,969,8,574; 0.4 s period 182.25 / 13.11 / 8,629,9,014; 0.6 s period 176.24 / 14.05 / 8,526,8,833. Longer bursts rise higher but lose more density by 1.2 s; 0.4 s has the greatest render coverage, while 0.6 s pushes higher. This ranking is exposure/coverage data, not a perceptual quality metric. Checkpoints and front/side captures at each half-cycle are saved as `pyro_triple_t*_*.png` and `pyro_triple_pulse_0p*_t*_*.png` in `build/smoke-runtime-rnd/captures/3d-emitter-topology-rnd/`. Pulsing creates distinct smooth puffs, not folded billows. These are CPU volume renderer captures, not GPU viewport parity.

`SeparatesBuoyancyAdvectionAndDensityDecayAfterEmitterPulse` runs a 0.2 s Pyro source pulse, copies identical pulse-end fields into eighteen source-free branches, and evolves each for 1.0 s. Full flow / no-new-buoyancy / buoyancy-from-rest / density-decay-only / static-density-control retain 47.05% / 47.06% / 78.15% / 78.23% / 100% mass, with density-weighted world-Y centroids 0.396 / 0.396 / 0.205 / 0.205 / 0.204 m. The 2-cell boundary shell contains no density in any branch. Pulse-end temperature sum/max/support is 10.184 / 0.1601 / 320 cells (`temperature>1e-4`); after the first buoyancy-only step, 180 active temperature cells remain. At buoyancy .7 and pressure 8, temperature-weighted vertical velocity / curl magnitude is .000697 / .000475; across pressure iterations 1→32, curl stays .000475 while temperature-weighted vertical velocity declines .000844→.000658. At buoyancy 20, the same measures rise to .01990 / .01358. Pressure iteration 1/4/8/16/32 reduces first-step divergence after projection 1.007e-5→5.215e-7 but has negligible 1 s mass/centroid effect; raising buoyancy .7→7→20 raises centroid only to .2053/.2071/.2106 m. This supports that projection removes much of the gradient-like vertical kick while preserving the weaker curl component; longer-run lift remains small. Front/side images are in `build/smoke-runtime-rnd/captures/3d-emitter-decay-rnd/`.

The same test adds three test-only standard vorticity-confinement branches over the pulse's remaining velocity: epsilon 2 and 8 over the full volume, plus epsilon 8 gated by active temperature and relative density gradient. It computes physical-space `curl(u)`, `N=grad(|curl|)/|grad(|curl|)|`, and applies `epsilon*h*dt*(N×curl)` at each source-free step. Against the no-added-confinement baseline (47.05% mass retention, centroidY .396 m), epsilon 2 retains 36.41% / .395 m, epsilon 8 retains 26.94% / .320 m, and the gated epsilon 8 retains 37.18% / .376 m. Stronger confinement raises active-support speed and mass loss; front/side captures remain smooth puffs without coherent folds. The interface gate moderates the force but does not produce visible billows. Captures: `post_pulse_residual_curl_conf_{2,8}_{front,side}.png` and `post_pulse_interface_curl_conf_8_{front,side}.png` under `build/smoke-runtime-rnd/captures/3d-emitter-decay-rnd/`. This adapter is isolated to the R&D test and does not change production Pyro.

Four additional branches isolate density-only shredding from velocity forcing: a no-shred control, 8-cell low-frequency modulation at amplitude .12, 8/4/2.5-cell multi-scale modulation at .12, and the same multi-scale modulation at .28. A smoothly translated deterministic 3D value-noise field is weighted by relative density gradient, mean-corrected by density mass, and applied before each source-free step. The maximum per-step pre-transport mass error is below 6.8e-9; branch velocity metrics remain identical. At 1 s, control / low / multi-.12 / multi-.28 mass is 20.19 / 22.86 / 20.93 / 22.06, and density Laplacian RMS is .00595 / .00957 / .00645 / .01016. The final centroid is .396 / .370 / .394 / .372 m; normalized density L1 change between the 0.5 s and 1 s checkpoints is .496 / .479 / .499 / .489. Thus higher-frequency modulation adds visible edge/interior roughness and a higher Laplacian, but alters later density transport and the stronger conditions shift the plume downward; it does not create folded billows. Half-run and terminal front/side PNGs use `post_pulse_<condition>_{half,front,side}.png` in `build/smoke-runtime-rnd/captures/3d-emitter-decay-rnd/`. This is a test-only conservative density perturbation, not an emitter or solver change.

`ComparesIndependentRollAndFineDisturbanceOperators` now also tests a smooth, analytic Gaussian vortex whose center tracks the 2D plume density centroid. The field is tangent to circles around its center, has a 7-cell radius, and is applied after each runtime frame as a test-only velocity increment; acceleration variants scale that increment by `1/30 s`. A weak 1.2-unit/s acceleration changes final mass from 472.485 to 495.993 and divergence RMS from .0155404 to .0164264, while adjacent image delta is essentially unchanged (.374903 vs .376309). At 3.6 units/s, mass reaches 593.904 and divergence .0189435; after final-image-only mass normalization, visible pixels are 181,560 vs 180,690 baseline. The stronger .24-per-frame increment reaches mass 701.328, divergence .0246548, and adjacent image delta .488417; normalizing only the final density still leaves 215,259 visible pixels and a broad laterally displaced cap. Visual review of both seeds shows cap displacement/expansion, not coherent folded billows. Captures are `smoke_operator_seed*_tracked-vortex-*.png` in `build/smoke-runtime-rnd/captures/tracked-vortex-rnd/`. Final-image normalization is only a shape diagnostic; it does not make the simulation mass-conserving. The focused GTest passed with all 13 conditions across seeds 17 and 71 (45.21 s). This remains independent test code, not a production operator.
