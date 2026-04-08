@echo off
REM Setup MSVC environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Change to project directory
cd /d X:\dev\artifactstudio

REM Check if build tree exists and configure if needed
if not exist "out\build\x64-Debug\CMakeCache.txt" (
    echo Configuring CMake with x64-Debug preset...
    cmake --preset x64-Debug
    if errorlevel 1 (
        echo CMAKE CONFIGURATION FAILED
        exit /b 1
    )
)

REM Run build for ArtifactCore target only
echo Starting ArtifactCore build...
cmake --build out/build/x64-Debug --target ArtifactCore 2>&1

exit /b %errorlevel%
