# ArtifactCore Combined Standalone Suite

**最終更新:** 2026-10-31

This parent project combines Core value, Core utility, Color, FrameRate, Geometry,
RawImage, Data, Audio, Asset, and DiagnosticEngine standalone suites in one build
tree. Common Core, Container, SharedPtr, and TrackedPtr modules have a single
owner target.
StringConvertor, UniString, and StringLike also share one interface target, and
UniString.cpp has one implementation owner across CoreUtility, FrameRate, and Audio.
All 16 Color tests share one interface and implementation target. Color and
OpenEXR share Core.Parallel. The two FrameRate tests and AspectRatio test share
module outputs, as do the ten Geometry tests.
RawImage reuses the common SharedPtr foundation and owns its OpenCV feature target.
The 152 test source files are grouped into 13 feature executables and CTest
entries: 5 Core value, 54 Core utility, 16 Color, 3 FrameRate, 10 Geometry,
1 RawImage, 7 Data, 50 Audio, 2 OpenEXR, and one each for AssetDatabase,
AssetMetaFile, AssetFingerprint, and DiagnosticEngine. Each feature executable
runs its GoogleTest cases in one process. Configure an individual suite directory
instead to keep its original focused executable boundaries.

Configure with the repository's vcpkg toolchain and triplet. The Audio suite
also needs Qt 6 Gui and Multimedia plus Boost headers. OpenEXR needs OpenImageIO
and TBB.

```powershell
cmake -S tests/ArtifactCore/StandaloneSuite -B tests/ArtifactCore/StandaloneSuite/build `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF
cmake --build tests/ArtifactCore/StandaloneSuite/build --config Debug --parallel 4
ctest --test-dir tests/ArtifactCore/StandaloneSuite/build -C Debug --output-on-failure
```

The CoreValue, CoreUtility (including AnimationDynamics and SimulationSystem), ColorSpace, FrameRate, Geometry, RawImage, Data,
Audio, AssetDatabase, AssetMetaFile, AssetFingerprint, DiagnosticEngine, and
OpenExr directories remain independently configurable for focused work. Use this parent project for a
combined run so shared build artifacts are generated once per owner target.
