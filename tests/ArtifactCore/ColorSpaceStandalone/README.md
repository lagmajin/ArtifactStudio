# Standalone Color Space Tests

**最終更新:** 2026-10-09

This CMake project builds only the production color modules used by
`ColorSpaceTest.cpp`, `ColorConversionTest.cpp`, `ColorACESContractTest.cpp`,
`ColorBridgeTest.cpp`,
`SurfacePixelConversionTest.cpp`, `ImageSurfaceViewTest.cpp`,
`ColorLUTContractTest.cpp`, `ColorBlendModeStandaloneTest.cpp`,
`ColorLuminanceContractTest.cpp`, and `ColorHarmonizerContractTest.cpp`. It does not load the ArtifactStudio
top-level project or link the full `ArtifactCore` library.

`../StandaloneTestHelpers.cmake` contains the small reusable CMake helpers for
module interfaces, module implementations, and GoogleTest executables. To add
another independent test, add only its production module interface and
implementation source files to this project and list its test source in an
`artifact_add_standalone_gtest()` call. Keep the dependency closure explicit;
do not link the full application or `ArtifactCore` just to make a test compile.
For MSVC, `.cppm` implementation units are copied at configure time into this
build directory with a `.cpp` suffix. This avoids MSVC/CMake treating the
implementation attachment as another module interface; the production source
remains the sole editable source of truth.

Requirements: CMake 3.30 or newer, a C++23 compiler with C++ module support,
Qt 6 Core and Gui, and GoogleTest's CMake package. Qt Gui is used by the
ColorBridge, ColorLUT, ColorBlendMode, and ColorHarmonizer targets; the
SurfacePixelConversion, ImageSurfaceView, and ColorLuminance targets use only
dependency-free color and surface modules. The ACES suite reuses the ColorSpace module closure. On MSVC,
use a recent compiler with experimental C++ modules enabled by the project's
toolchain.

From this directory, configure, build, and run the standalone color suites with:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Each executable is also independently selectable, for example:

```powershell
cmake --build build --config Debug --target ArtifactCoreColorACESTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreColorACESTest
cmake --build build --config Debug --target ArtifactCoreSurfacePixelConversionTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreSurfacePixelConversionTest
cmake --build build --config Debug --target ArtifactCoreImageSurfaceViewTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreImageSurfaceViewTest
cmake --build build --config Debug --target ArtifactCoreColorLUTTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreColorLUTTest
cmake --build build --config Debug --target ArtifactCoreColorBlendModeTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreColorBlendModeTest
cmake --build build --config Debug --target ArtifactCoreColorLuminanceTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreColorLuminanceTest
cmake --build build --config Debug --target ArtifactCoreColorHarmonizerTest
ctest --test-dir build -C Debug --output-on-failure -R ArtifactCoreColorHarmonizerTest
```

Set `CMAKE_PREFIX_PATH` when Qt 6 or GoogleTest is not discoverable by CMake.
