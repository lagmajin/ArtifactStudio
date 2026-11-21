# Standalone Color Space Tests

**最終更新:** 2026-11-21

This project registers sixteen color contract suites. They share one feature
module target and one module implementation target. The combined
`StandaloneSuite` also reuses the common Core and Container foundation and
groups these sources in the `ArtifactCoreColorFeatureTests` executable.

`../StandaloneTestHelpers.cmake` provides the module and GoogleTest helpers.
Color implementation sources use their actual `.cpp` paths. Keep this project
independent of the full ArtifactStudio application and `ArtifactCore` library.

Requirements: CMake 3.30 or newer, a C++23 compiler with C++ module support,
Qt 6 Core and Gui, and GoogleTest's CMake package. On MSVC,
use a recent compiler with experimental C++ modules enabled by the project's
toolchain.

Use the combined parent project for normal development, following the shared
build instructions in `tests/README.md`:

```powershell
cmake --build out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite --config Debug --target ArtifactCoreColorFeatureTests
ctest --test-dir out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite -C Debug -R '^ArtifactCoreColorFeatureTests$' --output-on-failure
```

The direct `ColorSpaceStandalone` project remains useful for focused
investigations that require the original executable boundaries. If used,
configure it under `out/standalone-build/<preset>/ColorSpaceStandalone` rather
than creating a build directory under the test source tree. In the combined
executable, GoogleTest cases can be selected with `--gtest_filter`.

Set `CMAKE_PREFIX_PATH` when Qt 6 or GoogleTest is not discoverable by CMake.
