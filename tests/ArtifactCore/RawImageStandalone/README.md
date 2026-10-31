# RawImage Standalone Test

This project configures the RawImage contract suite with a focused image module
target. In the combined `StandaloneSuite`, RawImage reuses the shared Core and
Container foundation, including `Memory.SharedPtr`.

Requirements: CMake 3.30 or newer, a C++23 compiler with module support, Qt 6
Core, OpenCV, and GoogleTest. Configure this directory for focused work, or use
`tests/ArtifactCore/StandaloneSuite` to share the foundation artifacts with the
other standalone groups.
