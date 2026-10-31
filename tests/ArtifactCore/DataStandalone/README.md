# ArtifactCore Data Standalone Tests

**最終更新:** 2026-10-31

This standalone CMake project builds seven independent CPU contract suites for
column types, typed columns, CSV parsing, data tables, data sources, CSV data
sources, and the data cache. Each executable can run in its own process.

Production module interfaces are assigned to one target each and linked in a
shared dependency graph. A single configure tree therefore compiles common
modules once for these suites. Building one test target still builds only that
test and its transitive module dependencies.


Configure from this directory to share generated module artifacts across these
suites:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

To share the Core/Container foundation module artifacts with all Audio
standalone suites in the same build tree, configure the combined parent at
`tests/ArtifactCore/StandaloneSuite` instead.

The registered test names are:

- `ArtifactCoreColumnTypeStandaloneTest`
- `ArtifactCoreTypedColumnStandaloneTest`
- `ArtifactCoreCsvParserStandaloneTest`
- `ArtifactCoreDataTableStandaloneTest`
- `ArtifactCoreDataSourceStandaloneTest`
- `ArtifactCoreCsvDataSourceStandaloneTest`
- `ArtifactCoreDataCacheStandaloneTest`

Pass a target name to `cmake --build` to build one suite and its transitive
module dependencies, or pass a test name to `ctest -R` to run one suite. The
project requires CMake 3.30 or newer, a C++23 compiler with C++ modules support,
and the GoogleTest CMake package.

If GoogleTest is not discoverable in the default CMake package paths, set
`CMAKE_PREFIX_PATH` to its installation prefix or pass the toolchain file for
the package manager used by your compiler. For vcpkg, disable manifest mode for
this standalone project unless a local manifest is added.
