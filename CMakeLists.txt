cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# import std; サポートの有効化 (project() より前に記述が必須)
# ---------------------------------------------------------------------------
# CMake 3.30+ では import std は実験的機能のため、特定の UUID を設定する必要があります
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING "Required for import std" FORCE)

# clang-cl / IntelLLVM 系では project() 前の CMAKE_CXX_MODULE_STD ON が
# try_compile と衝突するため、コンパイラ種別を事前チェックします。
set(ARTIFACT_USING_CLANG_LIKE_PREPROJECT OFF)
if(CMAKE_CXX_COMPILER)
    string(TOLOWER "${CMAKE_CXX_COMPILER}" _artifact_cxx_compiler_lower)
    if(_artifact_cxx_compiler_lower MATCHES "clang-cl(\\.exe)?$" OR
       _artifact_cxx_compiler_lower MATCHES "icx-cl(\\.exe)?$" OR
       _artifact_cxx_compiler_lower MATCHES "icx(\\.exe)?$" OR
       _artifact_cxx_compiler_lower MATCHES "icpx(\\.exe)?$")
        set(ARTIFACT_USING_CLANG_LIKE_PREPROJECT ON)
    endif()
endif()

# NOTE: std.ixx の検出と CMAKE_CXX_STDLIB_MODULES_JSON の設定は
#       project() の後（toolchain 処理後）に行います。
#       pre-project フェーズでは CMAKE_CXX_COMPILER が未設定のため、
#       ENV{VCToolsInstallDir} が v145 を指して誤った std.ixx が選ばれるためです。

# ツールチェーン検出時に機能を有効化するために一時的に ON にします
# (これがないと __CMAKE::CXX23 ターゲットが作成されません)
# ただし clang-cl / IntelLLVM 系では CMake の feature try_compile と衝突するため
# project() 前の強制 ON は避けます。
set(ARTIFACT_ENABLE_PREPROJECT_IMPORT_STD ON)
if(ARTIFACT_USING_CLANG_LIKE_PREPROJECT)
    set(ARTIFACT_ENABLE_PREPROJECT_IMPORT_STD OFF)
endif()

if(ARTIFACT_ENABLE_PREPROJECT_IMPORT_STD)
    set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "Enable import std support" FORCE)
else()
    set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "Disable pre-project import std for non-MSVC frontends" FORCE)
endif()
set(CMAKE_CXX_STANDARD 23 CACHE STRING "Required for import std detection" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# vcpkg ツールチェーンの自動読み込み
# ---------------------------------------------------------------------------
# Ensure vcpkg triplet is set BEFORE vcpkg.cmake inclusion
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet" FORCE)
endif()

# vcpkg is loaded via include() inside toolchain-msvc-v143.cmake.
# This block is a fallback for direct cmake invocations without a toolchain.
if(NOT CMAKE_TOOLCHAIN_FILE)
    # Try C:/vcpkg first (external), then fallback to VS embedded
    set(_vcpkg_path "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    if(NOT EXISTS "${_vcpkg_path}")
        if(DEFINED ENV{VCPKG_ROOT})
            set(_vcpkg_path "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        endif()
    endif()

    if(EXISTS "${_vcpkg_path}")
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_path}" CACHE FILEPATH "")
        message(STATUS "Using vcpkg.cmake: ${_vcpkg_path}")
    else()
        message(WARNING "vcpkg.cmake not found at ${_vcpkg_path}")
    endif()
endif()

project(ArtifactSuite LANGUAGES C CXX)

# Re-assert v143 compiler if toolchain was used, in case vcpkg reset it
if(CMAKE_TOOLCHAIN_FILE MATCHES "toolchain-msvc-v143")
    # Verify compiler is v143 (14.x)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        string(REGEX MATCH "14\\." _is_v143 "${CMAKE_CXX_COMPILER_VERSION}")
        if(NOT _is_v143)
            message(WARNING
                "CMAKE_CXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION} is not v143 (14.x). "
                "Expected MSVC v143. Check Visual Studio 2022 installation.")
        else()
            message(STATUS "Verified MSVC compiler version: v${CMAKE_CXX_COMPILER_VERSION} (v143)")
        endif()
    endif()
endif()

message(STATUS "CMAKE_EXPERIMENTAL_CXX_IMPORT_STD: ${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD}")

# ---------------------------------------------------------------------------
# std.ixx メタデータの設定 (project() 後 = toolchain 処理後に実行)
# ---------------------------------------------------------------------------
# CMAKE_CXX_COMPILER が確定した後に実行することで、コンパイラパスから v143 の
# std.ixx を正確に導出できます。ENV{VCToolsInstallDir} は VS 18 の dev env から
# v145 を指す場合があるため、project() 前での検出は行いません。
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(ARTIFACT_MSVC_STD_IXX "")

    # Priority 1: Derive from CMAKE_CXX_COMPILER path (always v143 since toolchain forced it)
    # Compiler: .../VC/Tools/MSVC/<ver>/bin/HostX64/x64/cl.exe
    # std.ixx:  .../VC/Tools/MSVC/<ver>/modules/std.ixx
    get_filename_component(_std_x64   "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_std_host  "${_std_x64}"           DIRECTORY)
    get_filename_component(_std_bin   "${_std_host}"          DIRECTORY)
    get_filename_component(_std_root  "${_std_bin}"           DIRECTORY)
    if(EXISTS "${_std_root}/modules/std.ixx")
        set(ARTIFACT_MSVC_STD_IXX "${_std_root}/modules/std.ixx")
        message(STATUS "std.ixx (compiler-derived): ${ARTIFACT_MSVC_STD_IXX}")
    endif()

    # Priority 2: VS 2022 v143 explicit path (fallback if compiler path fails)
    if(NOT ARTIFACT_MSVC_STD_IXX)
        file(GLOB_RECURSE _std_ixx_cands LIST_DIRECTORIES FALSE
            "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/14.*/modules/std.ixx")
        list(SORT _std_ixx_cands ORDER DESCENDING)
        if(_std_ixx_cands)
            list(GET _std_ixx_cands 0 ARTIFACT_MSVC_STD_IXX)
            message(STATUS "std.ixx (VS 2022 v143 search): ${ARTIFACT_MSVC_STD_IXX}")
        endif()
    endif()

    if(ARTIFACT_MSVC_STD_IXX)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc-stdlib-modules.json.in"
            "${CMAKE_BINARY_DIR}/msvc-stdlib-modules.json"
            @ONLY)
        set(CMAKE_CXX_STDLIB_MODULES_JSON
            "${CMAKE_BINARY_DIR}/msvc-stdlib-modules.json"
            CACHE FILEPATH "MSVC standard library module metadata override" FORCE)
        message(STATUS "std.ixx metadata configured: ${CMAKE_CXX_STDLIB_MODULES_JSON}")
    else()
        message(WARNING "std.ixx not found; import std metadata override is disabled.")
    endif()
endif()

# clang-cl + C++20 modules: ensure clang-scan-deps is configured so CMake can
# build module import graphs.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
        find_program(ARTIFACT_CLANG_SCAN_DEPS
            NAMES clang-scan-deps.exe clang-scan-deps
            HINTS
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/x64/bin"
                "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/x64/bin"
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin"
                "C:/Program Files/Microsoft Visual Studio/2022/Preview/VC/Tools/Llvm/x64/bin"
                "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin"
                "C:/Program Files/Microsoft Visual Studio/18/Insiders/VC/Tools/Llvm/x64/bin"
                "C:/Program Files/LLVM/bin"
        )
        if(ARTIFACT_CLANG_SCAN_DEPS)
            set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
                "${ARTIFACT_CLANG_SCAN_DEPS}"
                CACHE FILEPATH "clang-scan-deps executable for C++ modules" FORCE)
        else()
            message(WARNING "clang-cl is selected but clang-scan-deps was not found. C++ module dependency scanning may fail.")
        endif()
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES ON CACHE BOOL "Enable C++ modules scan for clang-cl" FORCE)
endif()

# ---------------------------------------------------------------------------
# グローバル設定のリセット (外部ライブラリへの波及を防止)
# ---------------------------------------------------------------------------
# ツールチェーン検出が終わったので、以降のターゲットがデフォルトで ON にならないよう OFF に戻す
# 必要なターゲット（ArtifactCore等）でのみ個別に ON に設定します。
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "" FORCE)

# 全体のデフォルト C++ 規格: C++23 (/std:c++latest で最新 draft を有効化)
set(CMAKE_CXX_STANDARD 23 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# vcpkg パッケージの探索とリンク
# ---------------------------------------------------------------------------
# nanosvg
find_package(nanosvg CONFIG QUIET)
if(nanosvg_FOUND)
    message(STATUS "Found nanosvg via vcpkg config")
else()
    # Fallback: ヘッダのみのライブラリとして手動探索
    find_path(NANOSVG_INCLUDE_DIRS nanosvg.h
        PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanosvg/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanosvg/include"
            "${VCPKG_INSTALLED}/include"
            "${VCPKG_INSTALLED}/include/nanosvg"
    )
    if(NANOSVG_INCLUDE_DIRS)
        message(STATUS "Found nanosvg headers: ${NANOSVG_INCLUDE_DIRS}")
    endif()
endif()
set(VCPKG_ROOT        "C:/vcpkg"                               CACHE PATH "vcpkg root dir")
set(VCPKG_INSTALLED   "${VCPKG_ROOT}/installed/x64-windows"    CACHE PATH "vcpkg install dir")
set(THIRD_PARTY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "local third_party dir")
set(ARTIFACT_APP_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Artifact"  CACHE PATH "Artifact app dir")
set(ARTIFACT_DEPLOY_DIR "" CACHE PATH "Optional output directory for copying Artifact exe and runtime dlls")
option(ARTIFACT_USE_SCCACHE "Use sccache compiler launcher when available" ON)
option(ARTIFACT_ENABLE_SYCL "Enable Intel oneAPI DPC++/SYCL build flags" OFF)
option(ARTIFACT_USE_LOCAL_QADS "Prefer vendored Qt Advanced Docking System source over vcpkg package" ON)

set(ARTIFACT_QADS_SOURCE_DIR "${THIRD_PARTY_DIR}/Qt-Advanced-Docking-System")
set(ARTIFACT_HAS_LOCAL_QADS OFF)
if(ARTIFACT_USE_LOCAL_QADS AND EXISTS "${ARTIFACT_QADS_SOURCE_DIR}/CMakeLists.txt")
    set(ARTIFACT_HAS_LOCAL_QADS ON)
endif()

if(ARTIFACT_USE_SCCACHE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
        unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
        message(STATUS "sccache disabled for MSVC-style frontends to avoid module scan/PDB contention.")
    else()
        find_program(SCCACHE_PROGRAM
            NAMES sccache sccache.exe
            HINTS
                "${VCPKG_INSTALLED}/tools/sccache"
                "${VCPKG_ROOT}/installed/x64-windows/tools/sccache"
                "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/sccache"
        )
        if(SCCACHE_PROGRAM)
            set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}" CACHE STRING "" FORCE)
            set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}" CACHE STRING "" FORCE)
            message(STATUS "sccache enabled: ${SCCACHE_PROGRAM}")
        else()
            message(STATUS "sccache not found; building without compiler cache")
        endif()
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
if(ARTIFACT_FRONTEND_MSVC)
    # C++ modules + dyndep with a shared target PDB frequently hit C2471.
    # Use embedded debug info (/Z7) to avoid compile-time PDB write contention.
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded" CACHE STRING "" FORCE)
endif()

if(ARTIFACT_COMPILER_MSVC_CL)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:/Zf>
        $<$<COMPILE_LANGUAGE:CXX>:/Zf>
        $<$<COMPILE_LANGUAGE:C>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:C>:/utf-8>
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:C>:/openmp>
        $<$<COMPILE_LANGUAGE:CXX>:/openmp>
        $<$<COMPILE_LANGUAGE:C>:/MP>
        $<$<COMPILE_LANGUAGE:CXX>:/MP>)
    add_definitions(-D_GUARDOVERFLOW_CRT_ALLOCATORS=1)
    # Override /std:c++23 with /std:c++latest to enable C++26 draft features
    set(CMAKE_CXX23_STANDARD_COMPILE_OPTION   "/std:c++latest" CACHE STRING "" FORCE)
    set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION  "/std:c++latest" CACHE STRING "" FORCE)
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

function(artifact_enable_delay_load target)
    if(NOT WIN32 OR NOT ARTIFACT_FRONTEND_MSVC)
        return()
    endif()

    cmake_parse_arguments(AEDL "" "" "DEBUG_DLLS;RELEASE_DLLS" ${ARGN})
    if(NOT AEDL_DEBUG_DLLS AND NOT AEDL_RELEASE_DLLS)
        return()
    endif()

    get_filename_component(_artifact_msvc_link_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_artifact_msvc_link_dir "${_artifact_msvc_link_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_link_dir "${_artifact_msvc_link_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_link_dir "${_artifact_msvc_link_dir}" DIRECTORY)
    set(_artifact_delayimp_lib "${_artifact_msvc_link_dir}/lib/x64/delayimp.lib")
    if(EXISTS "${_artifact_delayimp_lib}")
        target_link_libraries(${target} PRIVATE "${_artifact_delayimp_lib}")
    else()
        target_link_libraries(${target} PRIVATE delayimp)
    endif()
    foreach(dllName IN LISTS AEDL_DEBUG_DLLS)
        target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:/DELAYLOAD:${dllName}>")
    endforeach()
    foreach(dllName IN LISTS AEDL_RELEASE_DLLS)
        target_link_options(${target} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/DELAYLOAD:${dllName}>")
    endforeach()
endfunction()

# ---------------------------------------------------------------------------
# ATL include path (Diligent より前に設定が必要)
# ---------------------------------------------------------------------------
if(MSVC)
    # cl.exe のパス: .../MSVC/<ver>/bin/Hostx64/x64/cl.exe -> <ver> root
    get_filename_component(_MSVC_TOOLCHAIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)  # x64
    get_filename_component(_MSVC_TOOLCHAIN_DIR "${_MSVC_TOOLCHAIN_DIR}" DIRECTORY)  # Hostx64
    get_filename_component(_MSVC_TOOLCHAIN_DIR "${_MSVC_TOOLCHAIN_DIR}" DIRECTORY)  # bin
    get_filename_component(_MSVC_TOOLCHAIN_DIR "${_MSVC_TOOLCHAIN_DIR}" DIRECTORY)  # <ver>

    set(_ATL_INCLUDE_DIR "${_MSVC_TOOLCHAIN_DIR}/atlmfc/include")
    set(_ATL_LIB_DIR     "${_MSVC_TOOLCHAIN_DIR}/atlmfc/lib/x64")

    # Fallback: active toolset が atlmfc を持たない場合 (VS Insiders/Preview で多発)、
    # 他の VS インストールから atlbase.h を探す。
    if(NOT EXISTS "${_ATL_INCLUDE_DIR}/atlbase.h")
        set(_ATL_INCLUDE_DIR "")
        set(_ATL_LIB_DIR     "")
        foreach(_atl_vs_major IN ITEMS 18 2022 17 16)
            foreach(_atl_ed IN ITEMS Insiders Community Enterprise Professional Preview BuildTools)
                file(GLOB _atl_cands
                    "C:/Program Files/Microsoft Visual Studio/${_atl_vs_major}/${_atl_ed}/VC/Tools/MSVC/14.*/atlmfc/include/atlbase.h"
                    "C:/Program Files (x86)/Microsoft Visual Studio/${_atl_vs_major}/${_atl_ed}/VC/Tools/MSVC/14.*/atlmfc/include/atlbase.h")
                if(_atl_cands)
                    list(SORT _atl_cands ORDER DESCENDING)
                    list(GET _atl_cands 0 _atl_found_h)
                    get_filename_component(_ATL_INCLUDE_DIR "${_atl_found_h}" DIRECTORY)
                    # atlmfc root -> lib/x64
                    get_filename_component(_atl_mfc_root "${_ATL_INCLUDE_DIR}" DIRECTORY)
                    if(EXISTS "${_atl_mfc_root}/lib/x64")
                        set(_ATL_LIB_DIR "${_atl_mfc_root}/lib/x64")
                    endif()
                    message(STATUS "ATL fallback: ${_ATL_INCLUDE_DIR}")
                    break()
                endif()
            endforeach()
            if(_ATL_INCLUDE_DIR)
                break()
            endif()
        endforeach()
    endif()

    if(_ATL_INCLUDE_DIR AND EXISTS "${_ATL_INCLUDE_DIR}/atlbase.h")
        # グローバル include path に追加 (DiligentEngine サブディレクトリにも伝播)
        include_directories("${_ATL_INCLUDE_DIR}")
        if(EXISTS "${_MSVC_TOOLCHAIN_DIR}/lib/x64")
            link_directories("${_MSVC_TOOLCHAIN_DIR}/lib/x64")
        endif()
        if(_ATL_LIB_DIR AND EXISTS "${_ATL_LIB_DIR}")
            link_directories("${_ATL_LIB_DIR}")
        endif()
        # check_include_file() / try_compile() が ATL を見つけられるようにする
        list(APPEND CMAKE_REQUIRED_INCLUDES "${_ATL_INCLUDE_DIR}")
        # DiligentEngine の try_compile(HAS_ATL) が走る前に結果をキャッシュに設定。
        # try_compile はキャッシュに結果があれば再コンパイルしない。
        # HAS_D3D11/HAS_D3D12 も同様に先行設定（d3d11.h/d3d12.h は Windows SDK に常に存在）。
        set(HAS_ATL  TRUE CACHE INTERNAL "ATL is available"  FORCE)
        set(HAS_D3D11 TRUE CACHE INTERNAL "D3D11 headers available" FORCE)
        set(HAS_D3D12 TRUE CACHE INTERNAL "D3D12 headers available" FORCE)
        # D3D11/D3D12 サポートフラグも強制更新（再 configure で古い FALSE が残る対策）
        set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 is supported" FORCE)
        set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 is supported" FORCE)
        # VCToolsInstallDir は atlmfc を含む VS ツールセットルートを指す
        get_filename_component(_ATL_VCTOOLS_ROOT "${_ATL_INCLUDE_DIR}/../.." ABSOLUTE)
        set(ENV{VCToolsInstallDir} "${_ATL_VCTOOLS_ROOT}/")
        message(STATUS "ATL include (global): ${_ATL_INCLUDE_DIR}")
        message(STATUS "VCToolsInstallDir: ${_ATL_VCTOOLS_ROOT}/")
    else()
        set(HAS_ATL FALSE CACHE INTERNAL "ATL is not available" FORCE)
        message(WARNING "atlbase.h が見つかりません。VS Installer で 'C++ 用の ATL (最新 v14x)' をインストールしてください。D3D11/D3D12 は無効になります。")
    endif()
endif()

if(WIN32 AND MSVC)
    set(_artifact_windows_sdk_root "C:/Program Files (x86)/Windows Kits/10")
    if(EXISTS "${_artifact_windows_sdk_root}/Lib")
        file(GLOB _artifact_windows_sdk_versions LIST_DIRECTORIES true "${_artifact_windows_sdk_root}/Lib/*")
        list(SORT _artifact_windows_sdk_versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _artifact_windows_sdk_versions 0 _artifact_windows_sdk_version_dir)
        if(EXISTS "${_artifact_windows_sdk_version_dir}/um/x64")
            link_directories("${_artifact_windows_sdk_version_dir}/um/x64")
        endif()
        if(EXISTS "${_artifact_windows_sdk_version_dir}/ucrt/x64")
            link_directories("${_artifact_windows_sdk_version_dir}/ucrt/x64")
        endif()
        message(STATUS "Windows SDK lib: ${_artifact_windows_sdk_version_dir}")
    endif()
endif()

# ---------------------------------------------------------------------------
# DiligentEngine (Submodule)
# ---------------------------------------------------------------------------
set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_DEMOS   OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_UNITY   OFF CACHE BOOL "" FORCE)

# Graphics backend selection
set(DILIGENT_NO_DIRECT3D11 OFF CACHE BOOL "" FORCE)  # DX11 enabled
set(DILIGENT_NO_DIRECT3D12 OFF CACHE BOOL "" FORCE)  # DX12 enabled
set(DILIGENT_NO_VULKAN     OFF CACHE BOOL "" FORCE)  # Vulkan enabled
set(DILIGENT_NO_OPENGL     ON  CACHE BOOL "" FORCE)  # OpenGL disabled
set(DILIGENT_NO_WEBGPU     ON  CACHE BOOL "" FORCE)  # WebGPU disabled

# Third-party trees are non-modular and occasionally emit malformed .ddi during
# MSVC dependency scanning. Disable module scan in this subtree only.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
add_subdirectory(libs/DiligentEngine EXCLUDE_FROM_ALL)
if(ARTIFACT_HAS_LOCAL_QADS)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC OFF CACHE BOOL "" FORCE)
    set(ADS_VERSION "4.4.0" CACHE STRING "Vendored QADS version" FORCE)
    add_subdirectory(third_party/Qt-Advanced-Docking-System EXCLUDE_FROM_ALL)
    if(TARGET qtadvanceddocking-qt6 AND MSVC)
        target_compile_options(qtadvanceddocking-qt6 PRIVATE /Zm200 /bigobj)
    endif()
endif()
add_subdirectory(third_party/ufbx EXCLUDE_FROM_ALL)
# TaskFlow submodule was removed
# set(TF_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
# set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# set(TF_BUILD_PROFILER OFF CACHE BOOL "" FORCE)
# set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(TF_BUILD_MODULES OFF CACHE BOOL "" FORCE)
# add_subdirectory(third_party/taskflow EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# サブプロジェクトの追加
# ---------------------------------------------------------------------------
# Project targets use C++ modules heavily; re-enable scanning for local code.
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
add_subdirectory(ArtifactCore)
add_subdirectory(ArtifactWidgets)
add_subdirectory(Artifact)

# ---------------------------------------------------------------------------
# テスト設定
# ---------------------------------------------------------------------------
option(ARTIFACT_BUILD_TESTS "Build unit tests" ON)
if(ARTIFACT_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(WARNING "GTest was not found; skipping unit tests.")
    endif()
endif()
