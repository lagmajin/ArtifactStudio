@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d X:\Dev\ArtifactStudio

cmake --preset x64-Debug
