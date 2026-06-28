@echo off
setlocal enabledelayedexpansion
cd /d "X:\dev\artifactstudio"

REM Try x64-Release preset build first
if exist "out\build\x64-Release" (
    echo Building with existing x64-Release preset...
    cmake --build out/build/x64-Release --target Artifact --config Release -- -k 0
) else if exist "out\build\x64-Debug" (
    echo Building with existing x64-Debug preset...
    cmake --build out/build/x64-Debug --target Artifact --config Debug -- -k 0
) else if exist "build" (
    echo Building with existing build directory...
    cd build
    cmake --build . --target Artifact --config Release -- -k 0
    if errorlevel 1 (
        echo Build failed with Release, trying Debug...
        cmake --build . --target Artifact --config Debug -- -k 0
    )
) else (
    echo No existing build tree found!
    exit /b 1
)
