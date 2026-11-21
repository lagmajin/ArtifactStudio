# ArtifactCore Audio Standalone Tests

**最終更新:** 2026-11-21

This CMake project registers fifty Audio and shared string / identifier
contract suites in one module graph: Segment and Effect base, AllPassFilter and
LFO, fifteen foundational, analysis, and spatial suites, Decibels, Volume,
twenty-two effects plus Vibrato, SimpleWav, Rasterizer, Spectrum, MixerFxPersistence, Tag,
and Id contract suites.
Shared Core, utility, and Audio sources have one owner target; suites remain
separate test executables.

For regular work, use the combined `tests/ArtifactCore/StandaloneSuite`
configure tree described in `tests/README.md`. Build only the Audio feature
target to reuse its common module dependencies without compiling unrelated
suite executables:

```powershell
cmake --build out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite --config Debug --target ArtifactCoreAudioFeatureTests
ctest --test-dir out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite -C Debug -R '^ArtifactCoreAudioFeatureTests$' --output-on-failure
```

The direct `AudioStandalone` project remains available for a focused
investigation that needs its original executable boundaries. Configure that
exception under `out/standalone-build/<preset>/AudioStandalone`; do not create a
build directory below the test source tree. Its objects are separate from the
combined suite, although it uses the repository toolchain and package install.
