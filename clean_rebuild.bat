@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d X:\dev\ArtifactStudio

echo [Removing stale build directory...]
rmdir /s /q out\build\x64-Debug 2>nul
echo [Build directory cleaned]

echo.
echo [Reconfiguring CMake...]
cmake --preset x64-Debug
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

echo.
echo [Building Artifact...]
cmake --build out\build\x64-Debug

echo.
echo [Clean rebuild complete]
pause
