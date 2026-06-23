@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d X:\Dev\ArtifactStudio

if not exist "out\build\x64-Debug\build.ninja" (
    echo Build tree missing. Run configure.bat first.
    exit /b 1
)

cmake --build out/build/x64-Debug
