# Geometry Standalone Suite

This project groups RotationTurns, ResolutionRemap, BezierPathSampler,
LayerAlignment, LayerBoundsCalculator, UniformScale, Position2D, Size2D, Quaternion, and Matrix4x4 contract tests. Their module interfaces and
implementations have one owner target each, with explicit IFC dependencies for
module implementation units.

Configure this directory for focused work, or use
`tests/ArtifactCore/StandaloneSuite` to include these tests in the combined
build graph. The project requires CMake 3.30, a C++23 compiler with module
support, Qt 6 Core and Gui, and GoogleTest.
