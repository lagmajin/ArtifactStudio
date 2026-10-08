# Standalone Color Space Tests

**最終更新:** 2026-10-08

This CMake project builds only the production `Color.ColorSpace` and
`Color.GamutConversion` modules plus `ColorSpaceTest.cpp`. It does not load the
ArtifactStudio top-level project or link the full `ArtifactCore` library.

Requirements: CMake 3.30 or newer, a C++23 compiler with C++ module support,
Qt 6 Core, and GoogleTest's CMake package. On MSVC, use a recent compiler with
experimental C++ modules enabled by the project's toolchain.

From this directory, configure, build, and run the suite with:

```powershell
cmake -S . -B build -G Ninja
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Set `CMAKE_PREFIX_PATH` when Qt 6 or GoogleTest is not discoverable by CMake.
