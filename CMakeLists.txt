cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# import std; サポート設定 (project() より前に記述が必須)
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_STD ON)

# ---------------------------------------------------------------------------
# vcpkg ツールチェーンの自動読み込み (project() の前に記述が必要)
# ---------------------------------------------------------------------------
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()

project(ArtifactSuite LANGUAGES CXX)

# ---------------------------------------------------------------------------
# 共通パス構成 (全サブプロジェクトで共有)
# ---------------------------------------------------------------------------
set(VCPKG_ROOT        "C:/vcpkg"                               CACHE PATH "vcpkg root dir")
set(VCPKG_INSTALLED   "${VCPKG_ROOT}/installed/x64-windows"    CACHE PATH "vcpkg install dir")
set(THIRD_PARTY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "local third_party dir")

# Artifact 本体のディレクトリ
set(ARTIFACT_APP_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Artifact"  CACHE PATH "Artifact app dir")

# ---------------------------------------------------------------------------
# ビルドオプション
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/Zf /Zc:__cplusplus /utf-8 /openmp /MP)
    add_definitions(-D_GUARDOVERFLOW_CRT_ALLOCATORS=1)
endif()

# ---------------------------------------------------------------------------
# DiligentEngine (Submodule)
# ---------------------------------------------------------------------------
set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "Build Diligent samples" FORCE)
set(DILIGENT_BUILD_DEMOS OFF CACHE BOOL "Build Diligent demos" FORCE)
set(DILIGENT_BUILD_UNITY OFF CACHE BOOL "Use Unity build" FORCE)

# 必要に応じてバックエンドを有効化
set(DILIGENT_NO_DIRECT3D11 OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_DIRECT3D12 OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_VULKAN OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)

add_subdirectory(libs/DiligentEngine EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# サブプロジェクトの追加 (順序に注意：依存される側を先に)
# ---------------------------------------------------------------------------
add_subdirectory(ArtifactCore)
add_subdirectory(ArtifactWidgets)
add_subdirectory(Artifact)
