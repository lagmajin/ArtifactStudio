# FrameRate Standalone Suite

This project groups FrameRate contract, storage scale, and AspectRatio contract suites.
The FrameRate tests share FrameRate, UniString, StringConvertor, and serialization
modules. AspectRatio reuses UniString and StringConvertor; all three tests use the
same module and implementation targets.

Configure this directory for focused work, or use
`tests/ArtifactCore/StandaloneSuite` to include it in the combined shared build
graph with Core, Color, Data, and Audio suites.

The project requires CMake 3.30, a C++23 compiler with module support, Qt 6
Core, and GoogleTest. The implementation source files are the existing `.cpp`
files in ArtifactCore; this project does not modify or link the full
ArtifactCore library.
