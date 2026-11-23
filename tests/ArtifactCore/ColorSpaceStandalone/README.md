# Standalone Color Space Tests

**最終更新:** 2026-10-31

This project registers sixteen color contract suites. They share one feature
module target and one module implementation target. When included in
`StandaloneSuite`, they also reuse its common Core and Container foundation.
The test executables remain independently selectable.

`../StandaloneTestHelpers.cmake` provides the module and GoogleTest helpers.
Color implementation sources use their actual `.cpp` paths. Keep this project
independent of the full ArtifactStudio application and `ArtifactCore` library.

Requirements: CMake 3.30 or newer, a C++23 compiler with C++ module support,
Qt 6 Core and Gui, and GoogleTest's CMake package. On MSVC,
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
