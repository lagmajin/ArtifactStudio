cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# import std; サポートの有効化 (project() より前に記述が必須)
# ---------------------------------------------------------------------------
# ツールチェーン検出時に標準ライブラリモジュールのビルド機能を有効にします
set(CMAKE_CXX_MODULE_STD ON)

# ---------------------------------------------------------------------------
# vcpkg ツールチェーンの自動読み込み
# ---------------------------------------------------------------------------
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()

project(ArtifactSuite LANGUAGES CXX)

# ツールチェーンでの有効化が終わったら、外部ライブラリ（Diligent等）を壊さないよう
# 全体のデフォルト値としては一旦 OFF に戻します。
set(CMAKE_CXX_MODULE_STD OFF)

# 全体のデフォルト C++ 規格は 20 に戻しておきます（外部ライブラリ用）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# 共通パス構成
# ---------------------------------------------------------------------------
set(VCPKG_ROOT        "C:/vcpkg"                               CACHE PATH "vcpkg root dir")
set(VCPKG_INSTALLED   "${VCPKG_ROOT}/installed/x64-windows"    CACHE PATH "vcpkg install dir")
set(THIRD_PARTY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "local third_party dir")
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
