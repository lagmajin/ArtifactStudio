@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d X:\dev\ArtifactStudio
echo [Reconfiguring CMake...]
cmake --preset x64-Debug
echo [Building Artifact...]
cmake --build out\build\x64-Debug --target Artifact