@echo off
setlocal enabledelayedexpansion

REM Test script to verify GTest installation and CMake configuration

echo ===== Test 1: Verify GTest Installation =====
if exist "X:\dev\artifactstudio\out\vcpkg_installed\x64-windows\include\gtest\gtest.h" (
    echo [OK] GTest headers found
) else (
    echo [FAIL] GTest headers NOT found
)

if exist "X:\dev\artifactstudio\out\vcpkg_installed\x64-windows\lib\gtest.lib" (
    echo [OK] GTest release library found
) else (
    echo [FAIL] GTest release library NOT found
)

if exist "X:\dev\artifactstudio\out\vcpkg_installed\x64-windows\debug\lib\gtest.lib" (
    echo [OK] GTest debug library found
) else (
    echo [FAIL] GTest debug library NOT found
)

REM Check metadata
if exist "X:\dev\artifactstudio\out\vcpkg_installed\vcpkg\info\gtest_1.17.0_x64-windows.list" (
    echo [OK] GTest metadata found - version 1.17.0
) else (
    echo [FAIL] GTest metadata NOT found
)

echo.
echo ===== Test 2: Run vcpkg install to ensure dependencies are satisfied =====
cd /d X:\dev\artifactstudio
C:\vcpkg\vcpkg.exe install --triplet x64-windows 2>&1 | find /I "gtest"

echo.
echo ===== Test 3: Configure CMake for x64 =====
cd /d X:\dev\artifactstudio
set "CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "!CMAKE_EXE!" (
    echo Found cmake.exe at: !CMAKE_EXE!
    echo Running cmake preset x64-Debug...
    "!CMAKE_EXE!" --preset x64-Debug 2>&1 | findstr /I "gtest\|GTest\|Configuring\|error"
) else (
    echo [FAIL] Could not find cmake.exe
)

echo.
echo ===== Test Complete =====
