cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# import std; サポートの有効化 (project() より前に記述が必須)
# ---------------------------------------------------------------------------
# CMake 3.30+ では import std は実験的機能のため、特定の UUID を設定する必要があります
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING "Required for import std" FORCE)

# ツールチェーン検出時に機能を有効化するために一時的に ON にします
# (これがないと __CMAKE::CXX23 ターゲットが作成されません)
set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "Enable import std support" FORCE)
set(CMAKE_CXX_STANDARD 23 CACHE STRING "Required for import std detection" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# vcpkg ツールチェーンの自動読み込み
# ---------------------------------------------------------------------------
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()

# Force 64-bit dependency resolution by default.
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet" FORCE)

project(ArtifactSuite LANGUAGES CXX)

message(STATUS "CMAKE_EXPERIMENTAL_CXX_IMPORT_STD: ${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD}")

# ---------------------------------------------------------------------------
# グローバル設定のリセット (外部ライブラリへの波及を防止)
# ---------------------------------------------------------------------------
# ツールチェーン検出が終わったので、以降のターゲットがデフォルトで ON にならないよう OFF に戻す
# 必要なターゲット（ArtifactCore等）でのみ個別に ON に設定します。
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "" FORCE)

# 全体のデフォルト C++ 規格も 23 を維持します
set(CMAKE_CXX_STANDARD 23 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# 共通パス構成
# ---------------------------------------------------------------------------
set(VCPKG_ROOT        "C:/vcpkg"                               CACHE PATH "vcpkg root dir")
set(VCPKG_INSTALLED   "${VCPKG_ROOT}/installed/x64-windows"    CACHE PATH "vcpkg install dir")
set(THIRD_PARTY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "local third_party dir")
set(ARTIFACT_APP_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Artifact"  CACHE PATH "Artifact app dir")
set(ARTIFACT_DEPLOY_DIR "" CACHE PATH "Optional output directory for copying Artifact exe and runtime dlls")
option(ARTIFACT_USE_SCCACHE "Use sccache compiler launcher when available" ON)
option(ARTIFACT_ENABLE_SYCL "Enable Intel oneAPI DPC++/SYCL build flags" OFF)

if(ARTIFACT_USE_SCCACHE)
    find_program(SCCACHE_PROGRAM
        NAMES sccache sccache.exe
        HINTS
            "${VCPKG_INSTALLED}/tools/sccache"
            "${VCPKG_ROOT}/installed/x64-windows/tools/sccache"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/sccache"
    )
    if(SCCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}" CACHE STRING "" FORCE)
        message(STATUS "sccache enabled: ${SCCACHE_PROGRAM}")
    else()
        message(STATUS "sccache not found; building without compiler cache")
    endif()
endif()

# ---------------------------------------------------------------------------
# Strict compiler/frontend detection
# ---------------------------------------------------------------------------
set(ARTIFACT_COMPILER_MSVC_CL OFF)
set(ARTIFACT_COMPILER_CLANG_CL OFF)
set(ARTIFACT_COMPILER_INTELLLVM_CL OFF)
set(ARTIFACT_COMPILER_INTELLLVM_GNU OFF)
set(ARTIFACT_FRONTEND_MSVC OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(ARTIFACT_COMPILER_MSVC_CL ON)
    set(ARTIFACT_FRONTEND_MSVC ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(ARTIFACT_COMPILER_CLANG_CL ON)
    set(ARTIFACT_FRONTEND_MSVC ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(ARTIFACT_COMPILER_INTELLLVM_CL ON)
    set(ARTIFACT_FRONTEND_MSVC ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(ARTIFACT_COMPILER_INTELLLVM_GNU ON)
endif()

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler frontend variant: ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}")
message(STATUS "ARTIFACT_COMPILER_MSVC_CL=${ARTIFACT_COMPILER_MSVC_CL}")
message(STATUS "ARTIFACT_COMPILER_CLANG_CL=${ARTIFACT_COMPILER_CLANG_CL}")
message(STATUS "ARTIFACT_COMPILER_INTELLLVM_CL=${ARTIFACT_COMPILER_INTELLLVM_CL}")
message(STATUS "ARTIFACT_COMPILER_INTELLLVM_GNU=${ARTIFACT_COMPILER_INTELLLVM_GNU}")

# ---------------------------------------------------------------------------
# Architecture guard (prevent x86/x64 mixing)
# ---------------------------------------------------------------------------
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "ArtifactSuite requires x64 build. Detected pointer size=${CMAKE_SIZEOF_VOID_P}.")
    endif()

    if(DEFINED CMAKE_GENERATOR_PLATFORM AND
       NOT CMAKE_GENERATOR_PLATFORM STREQUAL "" AND
       NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        message(FATAL_ERROR "ArtifactSuite requires -A x64. Current CMAKE_GENERATOR_PLATFORM='${CMAKE_GENERATOR_PLATFORM}'.")
    endif()

    if(NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows")
        message(FATAL_ERROR "ArtifactSuite requires VCPKG_TARGET_TRIPLET=x64-windows. Current='${VCPKG_TARGET_TRIPLET}'.")
    endif()
endif()

# ---------------------------------------------------------------------------
# ビルドオプション
# ---------------------------------------------------------------------------
if(ARTIFACT_COMPILER_MSVC_CL)
    add_compile_options(/Zf /Zc:__cplusplus /utf-8 /openmp /MP)
    add_definitions(-D_GUARDOVERFLOW_CRT_ALLOCATORS=1)
elseif(ARTIFACT_COMPILER_CLANG_CL)
    # clang-cl: keep MSVC-compatible baseline flags only
    add_compile_options(/Zc:__cplusplus /utf-8)
elseif(ARTIFACT_COMPILER_INTELLLVM_CL)
    # icx-cl: MSVC-style frontend
    add_compile_options(/Zc:__cplusplus /utf-8)
elseif(ARTIFACT_COMPILER_INTELLLVM_GNU)
    # icx/icpx GNU-like frontend
    add_compile_options(-fopenmp)
endif()

# ---------------------------------------------------------------------------
# Optional Intel DPC++ / SYCL enablement
# ---------------------------------------------------------------------------
if(ARTIFACT_ENABLE_SYCL)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
        message(FATAL_ERROR "ARTIFACT_ENABLE_SYCL=ON requires IntelLLVM compiler (dpcpp/icpx).")
    endif()

    if(ARTIFACT_FRONTEND_MSVC)
        message(FATAL_ERROR "ARTIFACT_ENABLE_SYCL currently supports IntelLLVM GNU-like frontend (dpcpp/icpx), not MSVC-like frontend.")
    endif()

    message(STATUS "ARTIFACT_ENABLE_SYCL=ON (enabling -fsycl)")
    add_compile_options(-fsycl)
    add_link_options(-fsycl)
    add_compile_definitions(ARTIFACT_HAS_SYCL=1)
endif()

# ---------------------------------------------------------------------------
# DiligentEngine (Submodule)
# ---------------------------------------------------------------------------
set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_UNITY OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)

add_subdirectory(libs/DiligentEngine EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# サブプロジェクトの追加
# ---------------------------------------------------------------------------
add_subdirectory(ArtifactCore)
add_subdirectory(ArtifactWidgets)
add_subdirectory(Artifact)
