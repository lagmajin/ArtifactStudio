@echo off
REM Setup MSVC environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Change to project directory
cd /d X:\dev\artifactstudio

if not exist "out\build\x64-Debug\build.ninja" (
    echo Build tree missing. Run configure.bat first.
    exit /b 1
)

REM Run build for ArtifactCore target only
echo Starting ArtifactCore build...
cmake --build out/build/x64-Debug --target ArtifactCore --verbose 2>&1
exit /b %errorlevel%
