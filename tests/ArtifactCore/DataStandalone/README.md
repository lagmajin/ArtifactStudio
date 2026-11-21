# ArtifactCore Data Standalone Tests

**最終更新:** 2026-11-21

This standalone CMake project contains seven CPU contract suites for column
types, typed columns, CSV parsing, data tables, data sources, CSV data sources,
and the data cache. The combined parent suite groups them into one feature
executable so the shared module closure is compiled once.

Production module interfaces are assigned to one target each and linked in a
shared dependency graph. A single configure tree therefore compiles common
modules once for these suites. Building one test target still builds only that
test and its transitive module dependencies.


Use the combined parent project for normal development, following the shared
build instructions in `tests/README.md`. Its Data target is
`ArtifactCoreDataFeatureTests`:

```powershell
cmake --build out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite --config Debug --target ArtifactCoreDataFeatureTests
ctest --test-dir out/standalone-build/x64-Debug-Agent/ArtifactCoreStandaloneSuite -C Debug -R '^ArtifactCoreDataFeatureTests$' --output-on-failure
```

Configure `DataStandalone` by itself only for a focused investigation that
needs its original process boundaries; put that build tree under
`out/standalone-build/<preset>/DataStandalone`, not under the source directory.

The registered test names are:

- `ArtifactCoreColumnTypeStandaloneTest`
- `ArtifactCoreTypedColumnStandaloneTest`
- `ArtifactCoreCsvParserStandaloneTest`
- `ArtifactCoreDataTableStandaloneTest`
- `ArtifactCoreDataSourceStandaloneTest`
- `ArtifactCoreCsvDataSourceStandaloneTest`
- `ArtifactCoreDataCacheStandaloneTest`

The direct project requires CMake 3.30 or newer, a C++23 compiler with C++
modules support, and the GoogleTest CMake package.

If GoogleTest is not discoverable in the default CMake package paths, set
`CMAKE_PREFIX_PATH` to its installation prefix or pass the toolchain file for
the package manager used by your compiler. For vcpkg, disable manifest mode for
this standalone project unless a local manifest is added.
