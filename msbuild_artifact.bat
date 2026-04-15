@echo off
setlocal enabledelayedexpansion

REM Set up Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo Could not find Visual Studio 2022
    exit /b 1
)

cd /d "X:\dev\artifactstudio\build"
echo Building Artifact target...
msbuild Artifact\Artifact.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:normal

if errorlevel 1 (
    echo.
    echo Build failed. Checking for compiler errors...
    exit /b 1
) else (
    echo.
    echo Build completed successfully.
)
