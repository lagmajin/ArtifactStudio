#!/bin/bash

# ArtifactCore Build Script for Qt Module Purview Compilation Check

PROJECT_DIR="/x/dev/artifactstudio"
BUILD_DIR="$PROJECT_DIR/out/build/x64-Debug"
CMAKE_EXE="cmake.exe"

cd "$PROJECT_DIR" || exit 1

echo "=========================================================================================================="
echo "ArtifactCore Build - Qt Module Purview Compilation Check"
echo "=========================================================================================================="
echo ""
echo "Project directory: $PROJECT_DIR"
echo "Build directory: $BUILD_DIR"
echo ""

# Check if CMake is available
if ! command -v $CMAKE_EXE &> /dev/null; then
    echo "Error: CMake not found in PATH"
    exit 1
fi

echo "=========================================================================================================="
echo "STARTING BUILD"
echo "=========================================================================================================="
echo ""

# Run the build
$CMAKE_EXE --build "$BUILD_DIR" --target ArtifactCore --verbose 2>&1

BUILD_EXIT_CODE=$?

echo ""
echo "=========================================================================================================="
echo "BUILD RESULT"
echo "=========================================================================================================="
echo ""

if [ $BUILD_EXIT_CODE -eq 0 ]; then
    echo "✓✓✓ SUCCESS ✓✓✓"
    echo ""
    echo "The following Qt modules with purview fixes compiled cleanly:"
    echo "  • IO.Async.ImageWriterManager"
    echo "  • IO.ImageExporter"
    echo "  • asio_async_file_writer"
    exit 0
else
    echo "✗✗✗ FAILURE ✗✗✗"
    echo "Build failed with exit code $BUILD_EXIT_CODE"
    exit 1
fi
