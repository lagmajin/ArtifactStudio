cmake_minimum_required(VERSION 3.28)
project(ArtifactSuite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# 共通パス構成 (全サブプロジェクトで共有)
# ---------------------------------------------------------------------------
set(VCPKG_INSTALLED   "X:/vcpkg/installed/x64-windows"          CACHE PATH "vcpkg install dir")
set(OPENCV_BUILD_DIR  "X:/opencv/build"                          CACHE PATH "OpenCV build dir")
set(VLC_DIR           "X:/vlc-3.0.21/sdk"                       CACHE PATH "VLC SDK dir")
set(BOX2D_DIR         "X:/box2d"                                 CACHE PATH "Box2D dir")
set(FMT_DIR           "X:/fmt/fmt"                               CACHE PATH "fmt dir")
set(SDL2_DIR          "X:/SDL2"                                  CACHE PATH "SDL2 dir")
set(LIBSND_DIR        "X:/libsnd"                                CACHE PATH "libsndfile dir")
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
# サブプロジェクトの追加 (順序に注意：依存される側を先に)
# ---------------------------------------------------------------------------
add_subdirectory(ArtifactCore)
add_subdirectory(ArtifactWidgets)
add_subdirectory(Artifact)
