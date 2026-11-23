# ArtifactCore Audio Standalone Tests

This CMake project registers fifty Audio and shared string / identifier
contract suites in one module graph: Segment and Effect base, AllPassFilter and
LFO, fifteen foundational, analysis, and spatial suites, Decibels, Volume,
twenty-two effects plus Vibrato, SimpleWav, Rasterizer, Spectrum, MixerFxPersistence, Tag,
and Id contract suites.
Shared Core, utility, and Audio sources have one owner target; suites remain
separate test executables.

Configure with the repository's vcpkg toolchain and triplet. Dependencies are
Qt 6 Core, Gui, and Multimedia, GTest, and Boost headers for MixerFxPersistence and Id.
Then build and run CTest:

```powershell
cmake -S tests/ArtifactCore/AudioStandalone -B tests/ArtifactCore/AudioStandalone/build `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF
cmake --build tests/ArtifactCore/AudioStandalone/build --config Debug --parallel 4
ctest --test-dir tests/ArtifactCore/AudioStandalone/build -C Debug --output-on-failure
```

The tests are still available from their original independent CMake projects.
To reuse the shared Core/Container foundation artifacts with the Data suites,
configure `tests/ArtifactCore/StandaloneSuite` as the parent project.
