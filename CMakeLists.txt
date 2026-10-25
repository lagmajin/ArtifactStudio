cmake_minimum_required(VERSION 3.30)

# Opt-in parallel COFF linking with LLVM LLD. CMake's MSVC platform expects
# CMAKE_LINKER_LLD to name lld-link directly; Ninja -j controls build actions,
# while /threads controls the parallelism inside each LLD link action.
option(ARTIFACT_USE_LLD "Use lld-link for Windows link steps" OFF)
set(ARTIFACT_LLD_THREADS "8" CACHE STRING
    "Maximum worker threads used by each lld-link action")
if(ARTIFACT_USE_LLD)
    if(NOT ARTIFACT_LLD_THREADS MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "ARTIFACT_LLD_THREADS must be a positive integer")
    endif()
    find_program(ARTIFACT_LLD_LINK_EXECUTABLE
        NAMES lld-link.exe lld-link
        HINTS
            "$ENV{ProgramFiles}/LLVM/bin"
            "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin"
            "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/bin"
            "$ENV{VCToolsInstallDir}/../Llvm/x64/bin"
    )
    if(NOT ARTIFACT_LLD_LINK_EXECUTABLE)
        message(FATAL_ERROR
            "ARTIFACT_USE_LLD=ON, but lld-link was not found. Set "
            "ARTIFACT_LLD_LINK_EXECUTABLE to its full path.")
    endif()
    set(CMAKE_LINKER_LLD "${ARTIFACT_LLD_LINK_EXECUTABLE}" CACHE FILEPATH
        "LLVM COFF linker used by CMAKE_LINKER_TYPE=LLD" FORCE)
    set(CMAKE_LINKER_TYPE LLD CACHE STRING
        "Select LLVM LLD for project link steps" FORCE)

    # CMake's MSVC linker wrapper invokes the resource compiler by name.
    # This shell may not have the Windows SDK bin directory on PATH, so seed
    # the compiler before project() initializes the MSVC platform rules.
    if(CMAKE_HOST_WIN32)
        if(CMAKE_RC_COMPILER AND NOT CMAKE_RC_COMPILER STREQUAL "rc")
            set(_artifact_rc_compiler "${CMAKE_RC_COMPILER}")
        else()
            find_program(_artifact_rc_compiler
                NAMES rc.exe
                HINTS
                    "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64"
                    "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x86"
                    "$ENV{ProgramFiles(x86)}/Windows Kits/10/bin/$ENV{WindowsSDKVersion}/x64"
            )
        endif()
        if(_artifact_rc_compiler)
            set(CMAKE_RC_COMPILER "${_artifact_rc_compiler}" CACHE FILEPATH
                "Windows resource compiler used by CMake's MSVC linker wrapper" FORCE)
            set(CMAKE_RC_COMPILER "${_artifact_rc_compiler}")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# import std; サポートの有効化 (project() より前に記述が必須)
# ---------------------------------------------------------------------------
# CMake 3.30+ gates import std behind a version-specific experimental UUID.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4.0")
    set(_artifact_import_std_uuid "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.0")
    set(_artifact_import_std_uuid "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.3")
    set(_artifact_import_std_uuid "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.0")
    set(_artifact_import_std_uuid "a9e1cf81-9932-4810-974b-6eccaf14e457")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31.8")
    set(_artifact_import_std_uuid "d0edc3af-4c50-42ea-a356-e2862fe7a444")
else()
    set(_artifact_import_std_uuid "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
endif()
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_uuid}")
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_uuid}" CACHE STRING
    "Version-specific gate for import std (CMake 3.30+)" FORCE)

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

# CMake 4.2's compiler feature probe needs the clang-cl P1689 command before
# project() enables CXX. Load the override at top-level scope so its rule
# variables are available while CMake initializes the language.
if(ARTIFACT_USING_CLANG_LIKE_PREPROJECT AND
   _artifact_cxx_compiler_lower MATCHES "clang-cl(\\.exe)?$")
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/clangcl-p1689-override.cmake")
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

# Enable CMake's standard-library module producer. Local targets also carry an
# explicit std.ifc reference for the MSVC module scanner, so the producer must
# exist before any interface or implementation unit imports std.
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "Use the explicit MSVC std.ifc producer" FORCE)
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
    CACHE STRING "Propagate C++ module settings to try_compile" FORCE)
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

if(ARTIFACT_USE_LLD)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "ARTIFACT_USE_LLD currently requires the Windows MSVC frontend")
    endif()
    enable_language(RC)
    add_link_options("/threads:${ARTIFACT_LLD_THREADS}")
    message(STATUS "Using lld-link: ${CMAKE_LINKER_LLD} (/threads:${ARTIFACT_LLD_THREADS})")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Visual Studio / CMake IDEs should open the main Artifact app by default.
set(CMAKE_VS_STARTUP_PROJECT Artifact CACHE STRING "Visual Studio startup project" FORCE)

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
# CMAKE_CXX_COMPILER が確定した後に実行することで、実際に使用される cl.exe と
# 同じ MSVC toolset 配下の std.ixx を導出します。import std の供給元は
# コンパイラと同じ toolset に揃っている必要があるため、旧 VS / 旧 toolset の
# std.ixx にフォールバックしないよう厳密に扱います。
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(ARTIFACT_MSVC_MODULES_JSON "")

    # Priority 1: Derive from the active compiler path.
    # Compiler: .../VC/Tools/MSVC/<ver>/bin/HostX64/x64/cl.exe
    # modules.json: .../VC/Tools/MSVC/<ver>/modules/modules.json
    get_filename_component(_std_x64   "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_std_host  "${_std_x64}"           DIRECTORY)
    get_filename_component(_std_bin   "${_std_host}"          DIRECTORY)
    get_filename_component(_std_root  "${_std_bin}"           DIRECTORY)
    if(EXISTS "${_std_root}/modules/modules.json")
        set(ARTIFACT_MSVC_MODULES_JSON "${_std_root}/modules/modules.json")
        message(STATUS "modules.json (compiler-derived): ${ARTIFACT_MSVC_MODULES_JSON}")
    endif()

    # Priority 2: Search for the exact compiler version under known VS installs.
    # This keeps modules.json aligned with the actual toolset even when the compiler
    # path is wrapped or normalized by the generator.
    if(NOT ARTIFACT_MSVC_MODULES_JSON)
        set(_artifact_msvc_version_glob "${CMAKE_CXX_COMPILER_VERSION}")
        if(NOT _artifact_msvc_version_glob)
            set(_artifact_msvc_version_glob "14.*")
        endif()
        file(GLOB_RECURSE _std_ixx_cands LIST_DIRECTORIES FALSE
            "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/${_artifact_msvc_version_glob}/modules/modules.json"
            "C:/Program Files/Microsoft Visual Studio/18/*/VC/Tools/MSVC/${_artifact_msvc_version_glob}/modules/modules.json"
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/${_artifact_msvc_version_glob}/modules/modules.json"
            "C:/Program Files (x86)/Microsoft Visual Studio/18/*/VC/Tools/MSVC/${_artifact_msvc_version_glob}/modules/modules.json")
        list(SORT _std_ixx_cands ORDER DESCENDING)
        if(_std_ixx_cands)
            list(GET _std_ixx_cands 0 ARTIFACT_MSVC_MODULES_JSON)
            message(STATUS "modules.json (compiler-version search): ${ARTIFACT_MSVC_MODULES_JSON}")
        endif()
    endif()

    if(ARTIFACT_MSVC_MODULES_JSON)
        set(CMAKE_CXX_STDLIB_MODULES_JSON
            "${ARTIFACT_MSVC_MODULES_JSON}"
            CACHE FILEPATH "MSVC standard library module metadata override" FORCE)
        message(STATUS "Active MSVC compiler: ${CMAKE_CXX_COMPILER}")
        message(STATUS "Active MSVC version: ${CMAKE_CXX_COMPILER_VERSION}")
        message(STATUS "std.ixx metadata configured: ${CMAKE_CXX_STDLIB_MODULES_JSON}")
    else()
        message(WARNING "modules.json not found; import std metadata override is disabled.")
    endif()
endif()

# CMake 4.3 does not yet populate __cmake_cxx_std_23 for MSVC 14.51, leaving
# that target without sources. Build the MSVC standard module explicitly; the
# reference is attached later only to sources that directly import `std`.
if(MSVC AND ARTIFACT_MSVC_MODULES_JSON)
    set(ARTIFACT_USE_EXPLICIT_STD_IFC ON CACHE BOOL
        "Use the project-owned MSVC std IFC producer" FORCE)
    get_filename_component(_artifact_msvc_modules_dir
        "${ARTIFACT_MSVC_MODULES_JSON}" DIRECTORY)
    set(_artifact_std_ixx "${_artifact_msvc_modules_dir}/std.ixx")
    set(_artifact_std_ifc "${CMAKE_BINARY_DIR}/CMakeFiles/__cmake_cxx23.dir/artifact_std.ifc")
    set(_artifact_std_obj "${CMAKE_BINARY_DIR}/CMakeFiles/__cmake_cxx23.dir/artifact_std.obj")
    get_filename_component(_artifact_std_compiler_bin
        "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_artifact_std_msvc_root
        "${_artifact_std_compiler_bin}/../../.." ABSOLUTE)
    set(_artifact_std_msvc_include "${_artifact_std_msvc_root}/include")
    file(GLOB _artifact_std_sdk_includes LIST_DIRECTORIES true
        "C:/Program Files (x86)/Windows Kits/10/Include/*")
    list(SORT _artifact_std_sdk_includes COMPARE NATURAL ORDER DESCENDING)
    list(GET _artifact_std_sdk_includes 0 _artifact_std_sdk_include_root)
    set(_artifact_std_include_args)
    foreach(_artifact_std_include IN ITEMS
        "${_artifact_std_msvc_include}"
        "${_artifact_std_sdk_include_root}/ucrt"
        "${_artifact_std_sdk_include_root}/shared"
        "${_artifact_std_sdk_include_root}/um"
        "${_artifact_std_sdk_include_root}/winrt")
        if(_artifact_std_include AND EXISTS "${_artifact_std_include}")
            list(APPEND _artifact_std_include_args "/I${_artifact_std_include}")
        endif()
    endforeach()
    if(EXISTS "${_artifact_std_ixx}")
        add_custom_command(
            OUTPUT "${_artifact_std_ifc}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${CMAKE_BINARY_DIR}/CMakeFiles/__cmake_cxx23.dir"
            COMMAND "${CMAKE_CXX_COMPILER}" /nologo /std:c++latest
                    /experimental:module /TP /c /interface /MDd /EHsc
                    /D_DEBUG /D_ITERATOR_DEBUG_LEVEL=2
                    ${_artifact_std_include_args}
                    "/ifcOutput${_artifact_std_ifc}"
                    "/Fo${_artifact_std_obj}"
                    "${_artifact_std_ixx}"
            DEPENDS "${_artifact_std_ixx}"
            COMMENT "Building MSVC standard library module std"
            VERBATIM)
        add_custom_target(ArtifactStdModule DEPENDS "${_artifact_std_ifc}")
    endif()
endif()

# Only enable CMake's native import-std target when its compiler probe found a
# standard-library module for C++23. The MSVC path above has its own explicit
# IFC producer; clang-cl's MSVC ABI alone does not provide CMAKE_CXX23's std.ifc.
set(ARTIFACT_CXX23_MODULE_STD_AVAILABLE OFF)
if(TARGET ArtifactStdModule OR "23" IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    set(ARTIFACT_CXX23_MODULE_STD_AVAILABLE ON)
endif()

# clang-cl + C++20 modules: ensure clang-scan-deps is configured so CMake can
# build module import graphs.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    if(CMAKE_VERSION VERSION_LESS 4.4)
        message(FATAL_ERROR
            "clang-cl C++ modules require CMake 4.4 or newer; detected ${CMAKE_VERSION}.")
    endif()
    if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
        message(FATAL_ERROR
            "clang-cl C++ modules currently require a Ninja generator in this project.")
    endif()
    if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
        get_filename_component(_artifact_clang_compiler_dir
            "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_program(ARTIFACT_CLANG_SCAN_DEPS
            NAMES clang-scan-deps.exe clang-scan-deps
            HINTS
                "${_artifact_clang_compiler_dir}"
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
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU" AND WIN32)
    if(CMAKE_CXX_COMPILER_TARGET AND
       NOT CMAKE_CXX_COMPILER_TARGET MATCHES "windows-msvc$")
        message(FATAL_ERROR
            "The Windows clang++ lane requires an MSVC ABI target; got '${CMAKE_CXX_COMPILER_TARGET}'.")
    endif()
    if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
        get_filename_component(_artifact_clang_compiler_dir
            "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_program(ARTIFACT_CLANG_SCAN_DEPS
            NAMES clang-scan-deps.exe clang-scan-deps
            HINTS
                "${_artifact_clang_compiler_dir}"
                "$ENV{VCToolsInstallDir}/../Llvm/x64/bin"
                "$ENV{ProgramFiles}/LLVM/bin")
        if(ARTIFACT_CLANG_SCAN_DEPS)
            set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${ARTIFACT_CLANG_SCAN_DEPS}"
                CACHE FILEPATH "clang-scan-deps executable for C++ modules" FORCE)
        else()
            message(FATAL_ERROR "Windows upstream clang++ requires clang-scan-deps for C++ modules.")
        endif()
    endif()
    # Clang 22's P1689 scanner can misreport `module Name;` attachments.
    # Keep scanning enabled globally, then let each project source manifest
    # opt implementation units in or out where dependency ordering requires it.
    # Upstream Clang lanes classify legacy .cppm implementations per source.
    set(CMAKE_CXX_SCAN_FOR_MODULES ON CACHE BOOL "Enable upstream Clang P1689 module scanning" FORCE)
endif()

# ---------------------------------------------------------------------------
# グローバル設定のリセット (外部ライブラリへの波及を防止)
# ---------------------------------------------------------------------------
# import std is supplied to local module targets by CMake's std.ifc producer
# and the explicit MSVC references below.

# 全体のデフォルト C++ 規格: C++23 (/std:c++latest で最新 draft を有効化)
set(CMAKE_CXX_STANDARD 23 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# vcpkg パッケージの探索とリンク
# ---------------------------------------------------------------------------
# Prefer the toolchain-provided out/vcpkg_installed tree when available.
if(NOT DEFINED VCPKG_ROOT)
    set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "vcpkg root dir")
endif()
if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}")
    set(VCPKG_INSTALLED "${VCPKG_INSTALLED_DIR}/x64-windows" CACHE PATH "vcpkg install dir" FORCE)
else()
    set(VCPKG_INSTALLED "${VCPKG_ROOT}/installed/x64-windows" CACHE PATH "vcpkg install dir")
endif()
if(EXISTS "${VCPKG_INSTALLED}/include/libavcodec/avcodec.h")
    set(FFMPEG_DIR "${VCPKG_INSTALLED}" CACHE PATH "FFmpeg root for Qt Multimedia detection" FORCE)
    set(FFMPEG_ROOT "${VCPKG_INSTALLED}" CACHE PATH "FFmpeg root for Qt Multimedia detection" FORCE)
    set(PC_FFMPEG_INCLUDE_DIRS "${VCPKG_INSTALLED}/include" CACHE PATH "FFmpeg pkg-config include hint" FORCE)
    set(PC_FFMPEG_LIBRARY_DIRS "${VCPKG_INSTALLED}/lib" CACHE PATH "FFmpeg pkg-config library hint" FORCE)
endif()

set(THIRD_PARTY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "local third_party dir")
set(ARTIFACT_APP_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Artifact"  CACHE PATH "Artifact app dir")
# ARTIFACT_PR_APP_DIR removed - ArtifactPr disabled in build
set(ARTIFACT_DEPLOY_DIR "" CACHE PATH "Optional output directory for copying Artifact exe and runtime dlls")
option(ARTIFACT_USE_SCCACHE "Use sccache compiler launcher when available" ON)
option(ARTIFACT_USE_SCCACHE_WITH_CLANG_CL
    "Opt in to sccache for clang-cl; C++ module caching is partial and unsupported commands bypass the cache"
    OFF)
option(ARTIFACT_ENABLE_SYCL "Enable Intel oneAPI DPC++/SYCL build flags" OFF)
option(ARTIFACT_BUILD_PR "Build the ArtifactPr NLE application" OFF)
option(ARTIFACT_BUILD_PR_TESTS
    "Build the ArtifactPr contract tests (requires ARTIFACT_BUILD_PR)" OFF)
option(ARTIFACT_BUILD_FX_STUDIO_TEST
    "Build the FxStudio FX animation MCP test host (ArtifactFxStudioMcpTest)" OFF)
if(ARTIFACT_BUILD_PR_TESTS)
    enable_testing()
endif()

if(ARTIFACT_USE_SCCACHE)
    set(_artifact_sccache_allowed ON)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU" AND WIN32)
        message(STATUS "sccache enabled for Windows upstream clang++; module command support is validated with cache statistics.")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        if(ARTIFACT_USE_SCCACHE_WITH_CLANG_CL)
            message(STATUS "sccache opt-in enabled for clang-cl; unsupported module commands may bypass the cache.")
        else()
            set(_artifact_sccache_allowed OFF)
            unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
            unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
            message(STATUS "sccache disabled for clang-cl; set ARTIFACT_USE_SCCACHE_WITH_CLANG_CL=ON to opt in.")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
           CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(_artifact_sccache_allowed OFF)
        unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
        unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
        message(STATUS "sccache disabled for MSVC-style compilers without supported module caching.")
    endif()

    if(_artifact_sccache_allowed)
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
set(ARTIFACT_COMPILER_CLANG_GNU OFF)
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
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU" AND WIN32)
    set(ARTIFACT_COMPILER_CLANG_GNU ON)
endif()

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler frontend variant: ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}")
message(STATUS "ARTIFACT_COMPILER_MSVC_CL=${ARTIFACT_COMPILER_MSVC_CL}")
message(STATUS "ARTIFACT_COMPILER_CLANG_CL=${ARTIFACT_COMPILER_CLANG_CL}")
message(STATUS "ARTIFACT_COMPILER_CLANG_GNU=${ARTIFACT_COMPILER_CLANG_GNU}")
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
elseif(ARTIFACT_COMPILER_CLANG_GNU)
    # Windows upstream clang++ targets the MSVC ABI with Clang's GNU driver.
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
    set(_MSVC_INCLUDE_DIR "${_MSVC_TOOLCHAIN_DIR}/include")

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
        if(EXISTS "${_MSVC_INCLUDE_DIR}/vcruntime.h")
            include_directories("${_MSVC_INCLUDE_DIR}")
        endif()
        # Do not inject Windows SDK include paths into CMAKE_<LANG>_FLAGS.
        # They precede target include directories and make Diligent's
        # #include "Query.h" resolve to the SDK's unrelated um/Query.h.
        # Remove both forms written by older configurations. The SDK path was
        # not always known at this point, which left the malformed /I"/um"
        # sequence in existing build caches.
        set(_artifact_explicit_sdk_include_flags
            "/I\"${_MSVC_INCLUDE_DIR}\" /I\"${_ATL_INCLUDE_DIR}\" /I\"${_artifact_windows_sdk_version_dir}/um\" /I\"${_artifact_windows_sdk_version_dir}/ucrt\" /I\"${_artifact_windows_sdk_version_dir}/shared\" /I\"${_artifact_windows_sdk_version_dir}/winrt\"")
        set(_artifact_malformed_sdk_include_flags
            "/I\"${_MSVC_INCLUDE_DIR}\" /I\"${_ATL_INCLUDE_DIR}\" /I\"/um\" /I\"/ucrt\" /I\"/shared\" /I\"/winrt\"")
        foreach(_artifact_language_flags CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
            string(REPLACE "${_artifact_explicit_sdk_include_flags}" "" ${_artifact_language_flags} "${${_artifact_language_flags}}")
            string(REPLACE "${_artifact_malformed_sdk_include_flags}" "" ${_artifact_language_flags} "${${_artifact_language_flags}}")
            set(${_artifact_language_flags} "${${_artifact_language_flags}}" CACHE STRING
                "${_artifact_language_flags} without explicit SDK includes" FORCE)
        endforeach()
        set(ENV{INCLUDE} "${_MSVC_INCLUDE_DIR};${_ATL_INCLUDE_DIR};${_artifact_windows_sdk_version_dir}/um;${_artifact_windows_sdk_version_dir}/ucrt;${_artifact_windows_sdk_version_dir}/shared;${_artifact_windows_sdk_version_dir}/winrt;$ENV{INCLUDE}")
        if(EXISTS "${_MSVC_TOOLCHAIN_DIR}/lib/x64")
            link_directories("${_MSVC_TOOLCHAIN_DIR}/lib/x64")
        endif()
        set(ENV{LIB} "${_MSVC_TOOLCHAIN_DIR}/lib/x64;${_artifact_windows_sdk_version_dir}/um/x64;${_artifact_windows_sdk_version_dir}/ucrt/x64;$ENV{LIB}")
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
        set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 backend available" FORCE)
        set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 backend available" FORCE)
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

if(WIN32 AND (MSVC OR ARTIFACT_COMPILER_CLANG_GNU))
    # The upstream clang++ MSVC-ABI toolchain discovers the same Windows SDK
    # without setting CMake's MSVC frontend flag. Reuse that discovery for
    # runtime deployment and Windows SDK metadata used below.
    if(ARTIFACT_COMPILER_CLANG_GNU)
        if(_artifact_sdk_root)
            set(_artifact_windows_sdk_root "${_artifact_sdk_root}")
        else()
            set(_artifact_windows_sdk_root "C:/Program Files (x86)/Windows Kits/10")
        endif()
    else()
        set(_artifact_windows_sdk_root "C:/Program Files (x86)/Windows Kits/10")
    endif()
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_C_FLAGS
        CMAKE_CXX_FLAGS
        CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
        CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION
        CMAKE_GENERATOR_PLATFORM
    )
    set(_artifact_windows_sdk_include_root "${_artifact_windows_sdk_root}/Include")
    if(EXISTS "${_artifact_windows_sdk_include_root}")
        file(GLOB _artifact_windows_sdk_include_versions LIST_DIRECTORIES true "${_artifact_windows_sdk_include_root}/*")
        list(SORT _artifact_windows_sdk_include_versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _artifact_windows_sdk_include_versions 0 _artifact_windows_sdk_include_version_dir)
        message(STATUS "Windows SDK include: ${_artifact_windows_sdk_include_version_dir}")
    endif()
    if(EXISTS "${_artifact_windows_sdk_root}/Lib")
        if(ARTIFACT_COMPILER_CLANG_GNU AND _artifact_sdk_version)
            set(_artifact_windows_sdk_version_name "${_artifact_sdk_version}")
            set(_artifact_windows_sdk_version_dir "${_artifact_windows_sdk_root}/Lib/${_artifact_windows_sdk_version_name}")
        else()
            file(GLOB _artifact_windows_sdk_versions LIST_DIRECTORIES true "${_artifact_windows_sdk_root}/Lib/*")
            list(SORT _artifact_windows_sdk_versions COMPARE NATURAL ORDER DESCENDING)
            list(GET _artifact_windows_sdk_versions 0 _artifact_windows_sdk_version_dir)
            get_filename_component(_artifact_windows_sdk_version_name "${_artifact_windows_sdk_version_dir}" NAME)
        endif()
        set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${_artifact_windows_sdk_version_name}" CACHE STRING "Windows SDK version for DiligentEngine" FORCE)
        set(ENV{WindowsSDKVersion} "${_artifact_windows_sdk_version_name}\\")
        # Source-file try_compile() probes do not inherit link_directories().
        # Add the SDK libraries after the resolved Lib version is known instead
        # of constructing /um and /ucrt paths from an unset/earlier variable.
        set(ENV{LIB}
            "${_MSVC_TOOLCHAIN_DIR}/lib/x64;${_ATL_LIB_DIR};${_artifact_windows_sdk_version_dir}/um/x64;${_artifact_windows_sdk_version_dir}/ucrt/x64;$ENV{LIB}")
        if(EXISTS "${_artifact_windows_sdk_version_dir}/um/x64")
            link_directories("${_artifact_windows_sdk_version_dir}/um/x64")
        endif()
        if(EXISTS "${_artifact_windows_sdk_version_dir}/ucrt/x64")
            link_directories("${_artifact_windows_sdk_version_dir}/ucrt/x64")
        endif()
        message(STATUS "Windows SDK lib: ${_artifact_windows_sdk_version_dir}")
    endif()
endif()

# Keep the Windows SDK headers available to third-party targets as well as the
# project targets.  DiligentEngine is configured in a nested subdirectory and
# does not inherit ArtifactCore's target-local SDK include directories.
if(WIN32 AND MSVC AND _artifact_windows_sdk_include_version_dir)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
        "${_artifact_windows_sdk_include_version_dir}/ucrt"
        "${_artifact_windows_sdk_include_version_dir}/shared"
        "${_artifact_windows_sdk_include_version_dir}/um"
        "${_artifact_windows_sdk_include_version_dir}/winrt"
        CACHE STRING "Windows SDK include directories for all MSVC targets" FORCE)
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
        "${_artifact_windows_sdk_include_version_dir}/ucrt"
        "${_artifact_windows_sdk_include_version_dir}/shared"
        "${_artifact_windows_sdk_include_version_dir}/um"
        "${_artifact_windows_sdk_include_version_dir}/winrt"
        CACHE STRING "Windows SDK include directories for all MSVC C targets" FORCE)
    # Diligent probes D3D12/ATL with source-file try_compile() before targets
    # inherit CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES. Ensure those probes see
    # the actual SDK Include tree; the earlier ATL bootstrap runs before the
    # SDK Lib version is discovered and cannot supply valid /um or /ucrt paths.
    set(ENV{INCLUDE}
        "${_MSVC_INCLUDE_DIR};${_ATL_INCLUDE_DIR};${_artifact_windows_sdk_include_version_dir}/ucrt;${_artifact_windows_sdk_include_version_dir}/shared;${_artifact_windows_sdk_include_version_dir}/um;${_artifact_windows_sdk_include_version_dir}/winrt;$ENV{INCLUDE}")
endif()

# ---------------------------------------------------------------------------
# DiligentEngine (Submodule)
# ---------------------------------------------------------------------------
set(_ARTIFACT_LOCAL_AGILITY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/temp/agilitysdk-1.619.4-new")
if(EXISTS "${_ARTIFACT_LOCAL_AGILITY_SDK_DIR}/build/native/bin/x64/D3D12Core.dll")
    set(_ARTIFACT_AGILITY_SDK_DEFAULT "${_ARTIFACT_LOCAL_AGILITY_SDK_DIR}")
else()
    set(_ARTIFACT_AGILITY_SDK_DEFAULT "")
endif()
set(ARTIFACT_AGILITY_SDK_DIR "${_ARTIFACT_AGILITY_SDK_DEFAULT}" CACHE PATH
    "Unpacked Microsoft.Direct3D.D3D12 retail SDK directory (contains build/native/bin/x64/D3D12Core.dll)")
if(NOT ARTIFACT_AGILITY_SDK_DIR AND _ARTIFACT_AGILITY_SDK_DEFAULT)
    set(ARTIFACT_AGILITY_SDK_DIR "${_ARTIFACT_AGILITY_SDK_DEFAULT}" CACHE PATH
        "Unpacked Microsoft.Direct3D.D3D12 retail SDK directory (contains build/native/bin/x64/D3D12Core.dll)"
        FORCE)
endif()
if(ARTIFACT_AGILITY_SDK_DIR)
    set(_ARTIFACT_AGILITY_SDK_INCLUDE
        "${ARTIFACT_AGILITY_SDK_DIR}/build/native/include")
    if(NOT EXISTS "${_ARTIFACT_AGILITY_SDK_INCLUDE}/d3d12.h")
        message(FATAL_ERROR
            "Agility SDK D3D12 headers not found: ${_ARTIFACT_AGILITY_SDK_INCLUDE}")
    endif()
    # Agility headers must precede the Windows SDK headers so the compile-time
    # interfaces match the D3D12Core.dll selected by D3D12SDKVersion.
    include_directories(BEFORE SYSTEM "${_ARTIFACT_AGILITY_SDK_INCLUDE}")
    message(STATUS "Agility SDK headers: ${_ARTIFACT_AGILITY_SDK_INCLUDE}")
endif()
option(ARTIFACT_ENABLE_DIRECTSTORAGE
    "Enable DirectStorage as the preferred Windows asynchronous file reader"
    ${WIN32})
set(_ARTIFACT_DIRECTSTORAGE_SDK_VERSION "1.3.0")
set(ARTIFACT_DIRECTSTORAGE_SDK_DIR "" CACHE PATH
    "Optional unpacked Microsoft.Direct3D.DirectStorage SDK directory (contains native/include/dstorage.h and native/bin/x64/dstorage.dll)")

# Keep Windows builds reproducible without relying on a developer-specific
# temporary directory. An explicitly supplied SDK directory remains the first
# choice; otherwise acquire the pinned official NuGet package into the build
# tree and verify its archive before extraction.
if(WIN32 AND ARTIFACT_ENABLE_DIRECTSTORAGE AND
   NOT ARTIFACT_DIRECTSTORAGE_SDK_DIR)
    include(FetchContent)
    FetchContent_Declare(artifact_directstorage_sdk
        URL
            "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.DirectStorage/${_ARTIFACT_DIRECTSTORAGE_SDK_VERSION}"
        URL_HASH
            "SHA256=8ab6c1082537565388b79050d94fdd7f5f437cd0f1d909e7b19f786042b97177"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(artifact_directstorage_sdk)
    set(ARTIFACT_DIRECTSTORAGE_SDK_DIR
        "${artifact_directstorage_sdk_SOURCE_DIR}" CACHE PATH
        "Optional unpacked Microsoft.Direct3D.DirectStorage SDK directory (contains native/include/dstorage.h and native/bin/x64/dstorage.dll)"
        FORCE)
    message(STATUS
        "DirectStorage SDK ${_ARTIFACT_DIRECTSTORAGE_SDK_VERSION} acquired from official NuGet package")
endif()
set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_FX      OFF CACHE BOOL "" FORCE)

# Graphics backend selection
set(DILIGENT_NO_DIRECT3D11 ON  CACHE BOOL "" FORCE)  # DX11 is not supported in the development configuration
set(DILIGENT_NO_DIRECT3D12 OFF CACHE BOOL "" FORCE)  # DX12 enabled for Agility SDK retail runtime
set(DILIGENT_NO_VULKAN     OFF CACHE BOOL "" FORCE)  # Vulkan enabled
set(DILIGENT_NO_GLSLANG    OFF CACHE BOOL "" FORCE)  # Vulkan runtime shader compilation requires glslang for the current HLSL path
set(DILIGENT_NO_OPENGL     ON  CACHE BOOL "" FORCE)  # OpenGL disabled
set(DILIGENT_NO_WEBGPU     ON  CACHE BOOL "" FORCE)  # WebGPU disabled
add_compile_definitions(QUERY_H_RESTRICTION_PERMISSIVE=1)

# Third-party trees are non-modular and occasionally emit malformed .ddi during
# MSVC dependency scanning. Disable module scan in this subtree only.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
# Diligent's platform probes use source-file try_compile projects. CMake's
# import-std integration injects an empty __cmake_cxx_std_23 target into those
# probes, so keep it disabled while configuring the third-party subtree.
set(_artifact_cxx_module_std_for_local_targets OFF)
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "Disable std module during Diligent probes" FORCE)
add_subdirectory(libs/DiligentEngine EXCLUDE_FROM_ALL)
set(CMAKE_CXX_MODULE_STD ${_artifact_cxx_module_std_for_local_targets} CACHE BOOL "Generate the C++ standard library module" FORCE)
# Diligent's nested configure may leave the cache with its older import-std
# gate value. Reassert the gate used by the active CMake before configuring
# the remaining third-party and project targets.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_uuid}")
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_uuid}"
    CACHE STRING "Restore the version-specific import std gate" FORCE)
# ufbx is a non-module third-party target and does not consume import std.
# Hide the experimental gate while its standalone CMakeLists is evaluated;
# otherwise CMake 4.3 rejects the older gate value used by that subtree.
set(_artifact_import_std_gate_for_ufbx "${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD}")
set(_artifact_cxx_module_std_for_ufbx "${CMAKE_CXX_MODULE_STD}")
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "" CACHE STRING "Disable import std for ufbx" FORCE)
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "Disable import std for ufbx" FORCE)
add_subdirectory(third_party/ufbx EXCLUDE_FROM_ALL)
if(TARGET ufbx AND WIN32 AND MSVC AND _artifact_windows_sdk_include_version_dir)
    target_include_directories(ufbx PRIVATE
        "${_artifact_windows_sdk_include_version_dir}/ucrt"
        "${_artifact_windows_sdk_include_version_dir}/shared"
        "${_artifact_windows_sdk_include_version_dir}/um")
endif()
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_gate_for_ufbx}" CACHE STRING "Restore import std gate" FORCE)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_gate_for_ufbx}")
set(CMAKE_CXX_MODULE_STD "${_artifact_cxx_module_std_for_ufbx}" CACHE BOOL "Restore import std setting" FORCE)
# OpenUSD is another non-modular third-party tree. Keep the same isolation
# Diligent and ufbx use so its source-file try_compile probes and generated
# headers cannot mix with the project's import-std / module-scan settings.
set(_artifact_import_std_gate_for_usd "${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD}")
set(_artifact_cxx_module_std_for_usd "${CMAKE_CXX_MODULE_STD}")
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "" CACHE STRING "Disable import std for openusd" FORCE)
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "Disable import std for openusd" FORCE)
set(PXR_BUILD_TESTS OFF CACHE BOOL "Disable OpenUSD tests" FORCE)
set(PXR_BUILD_DOCUMENTATION OFF CACHE BOOL "Disable OpenUSD documentation" FORCE)
set(PXR_ENABLE_PYTHON_SUPPORT OFF CACHE BOOL "Disable OpenUSD python support" FORCE)
set(PXR_BUILD_USD_TOOLS OFF CACHE BOOL "Disable OpenUSD command line tools" FORCE)
# The importer consumes usd / usdGeom / usdShade; rendering stays with Diligent.
# Imaging defaults to ON upstream and requires OpenSubdiv even with USD tools off.
set(PXR_BUILD_IMAGING OFF CACHE BOOL "Disable unused OpenUSD imaging components" FORCE)
set(PXR_BUILD_USD_IMAGING OFF CACHE BOOL "Disable unused OpenUSD scene imaging" FORCE)
set(PXR_BUILD_USDVIEW OFF CACHE BOOL "Disable unused OpenUSD viewer" FORCE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
    # OpenUSD's Clang compiler ID branch currently precedes its Windows OS
    # branch in attributes.h. On Windows Clang this hides the Windows
    # constructor-section declarations that attributes.cpp requires. Create a
    # one-condition compatibility copy in the build tree; never patch the
    # OpenUSD submodule itself.
    set(_artifact_openusd_compat_include_dir
        "${CMAKE_CURRENT_BINARY_DIR}/openusd-clang-compat")
    set(_artifact_openusd_attributes_source
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/openusd/pxr/base/arch/attributes.h")
    set(_artifact_openusd_attributes_compat
        "${_artifact_openusd_compat_include_dir}/pxr/base/arch/attributes.h")
    file(READ "${_artifact_openusd_attributes_source}"
        _artifact_openusd_attributes_contents)
    set(_artifact_openusd_clang_branch
        "#elif defined(ARCH_COMPILER_GCC) || defined(ARCH_COMPILER_CLANG)")
    string(FIND "${_artifact_openusd_attributes_contents}"
        "${_artifact_openusd_clang_branch}" _artifact_openusd_clang_branch_pos)
    string(FIND "${_artifact_openusd_attributes_contents}"
        "${_artifact_openusd_clang_branch}" _artifact_openusd_constructor_branch_pos REVERSE)
    if(_artifact_openusd_clang_branch_pos LESS 0 OR
       _artifact_openusd_constructor_branch_pos EQUAL _artifact_openusd_clang_branch_pos)
        message(FATAL_ERROR
            "OpenUSD arch attributes.h no longer has the expected compiler and "
            "constructor branches; "
            "review the Windows Clang compatibility overlay.")
    endif()
    string(SUBSTRING "${_artifact_openusd_attributes_contents}"
        0 ${_artifact_openusd_constructor_branch_pos}
        _artifact_openusd_attributes_prefix)
    string(SUBSTRING "${_artifact_openusd_attributes_contents}"
        ${_artifact_openusd_constructor_branch_pos} -1
        _artifact_openusd_attributes_suffix)
    string(REPLACE "${_artifact_openusd_clang_branch}"
        "#elif (defined(ARCH_COMPILER_GCC) || defined(ARCH_COMPILER_CLANG)) && !defined(ARCH_OS_WINDOWS)"
        _artifact_openusd_attributes_suffix
        "${_artifact_openusd_attributes_suffix}")
    string(CONCAT _artifact_openusd_attributes_contents
        "${_artifact_openusd_attributes_prefix}"
        "${_artifact_openusd_attributes_suffix}")
    file(MAKE_DIRECTORY "${_artifact_openusd_compat_include_dir}/pxr/base/arch")
    set(_artifact_openusd_attributes_write TRUE)
    if(EXISTS "${_artifact_openusd_attributes_compat}")
        file(READ "${_artifact_openusd_attributes_compat}"
            _artifact_openusd_attributes_existing)
        if(_artifact_openusd_attributes_existing STREQUAL
           _artifact_openusd_attributes_contents)
            set(_artifact_openusd_attributes_write FALSE)
        endif()
    endif()
    if(_artifact_openusd_attributes_write)
        file(WRITE "${_artifact_openusd_attributes_compat}"
            "${_artifact_openusd_attributes_contents}")
    endif()

    get_property(_artifact_openusd_parent_include_dirs DIRECTORY
        PROPERTY INCLUDE_DIRECTORIES)
    set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES
        "${_artifact_openusd_compat_include_dir};${_artifact_openusd_parent_include_dirs}")
    get_property(_artifact_openusd_parent_compile_definitions DIRECTORY
        PROPERTY COMPILE_DEFINITIONS)
    set(_artifact_openusd_clang_compile_definitions
        ${_artifact_openusd_parent_compile_definitions})
    list(APPEND _artifact_openusd_clang_compile_definitions NOMINMAX)
    set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS
        "${_artifact_openusd_clang_compile_definitions}")

    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # OpenUSD's shared warning helper still sees MSVC=TRUE for clang-cl and
        # emits `/wd<name>`, which clang-cl rejects. Override only that helper.
        set(_artifact_openusd_module_path "${CMAKE_MODULE_PATH}")
        list(PREPEND CMAKE_MODULE_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openusd-clangcl-compat")
    endif()

    add_subdirectory(libs/openusd EXCLUDE_FROM_ALL)

    if(TARGET arch)
        set(_artifact_openusd_arch_compat_source_dir
            "${CMAKE_CURRENT_BINARY_DIR}/openusd-clang-compat-src/pxr/base/arch")
        file(MAKE_DIRECTORY "${_artifact_openusd_arch_compat_source_dir}")

        set(_artifact_openusd_demangle_source
            "${CMAKE_CURRENT_SOURCE_DIR}/libs/openusd/pxr/base/arch/demangle.cpp")
        set(_artifact_openusd_demangle_compat
            "${_artifact_openusd_arch_compat_source_dir}/demangle.cpp")
        file(READ "${_artifact_openusd_demangle_source}"
            _artifact_openusd_demangle_contents)
        set(_artifact_openusd_itanium_branch
            "ARCH_COMPILER_GCC_MAJOR > 3 || defined(ARCH_COMPILER_CLANG)")
        string(FIND "${_artifact_openusd_demangle_contents}"
            "${_artifact_openusd_itanium_branch}" _artifact_openusd_itanium_branch_pos)
        if(_artifact_openusd_itanium_branch_pos LESS 0)
            message(FATAL_ERROR
                "OpenUSD arch demangle.cpp no longer has the expected Clang branch; "
                "review the Windows Clang compatibility overlay.")
        endif()
        string(REPLACE "${_artifact_openusd_itanium_branch}"
            "ARCH_COMPILER_GCC_MAJOR > 3 || (defined(ARCH_COMPILER_CLANG) && !defined(ARCH_OS_WINDOWS))"
            _artifact_openusd_demangle_contents
            "${_artifact_openusd_demangle_contents}")
        set(_artifact_openusd_demangle_write TRUE)
        if(EXISTS "${_artifact_openusd_demangle_compat}")
            file(READ "${_artifact_openusd_demangle_compat}"
                _artifact_openusd_demangle_existing)
            if(_artifact_openusd_demangle_existing STREQUAL
               _artifact_openusd_demangle_contents)
                set(_artifact_openusd_demangle_write FALSE)
            endif()
        endif()
        if(_artifact_openusd_demangle_write)
            file(WRITE "${_artifact_openusd_demangle_compat}"
                "${_artifact_openusd_demangle_contents}")
        endif()

        set(_artifact_openusd_errno_source
            "${CMAKE_CURRENT_SOURCE_DIR}/libs/openusd/pxr/base/arch/errno.cpp")
        set(_artifact_openusd_errno_compat
            "${_artifact_openusd_arch_compat_source_dir}/errno.cpp")
        file(READ "${_artifact_openusd_errno_source}"
            _artifact_openusd_errno_contents)
        set(_artifact_openusd_errno_branch "#elif !defined(ARCH_COMPILER_MSVC)")
        string(FIND "${_artifact_openusd_errno_contents}"
            "${_artifact_openusd_errno_branch}" _artifact_openusd_errno_branch_pos)
        if(_artifact_openusd_errno_branch_pos LESS 0)
            message(FATAL_ERROR
                "OpenUSD arch errno.cpp no longer has the expected compiler branch; "
                "review the Windows Clang compatibility overlay.")
        endif()
        string(REPLACE "${_artifact_openusd_errno_branch}"
            "#elif !defined(ARCH_COMPILER_MSVC) && !defined(ARCH_OS_WINDOWS)"
            _artifact_openusd_errno_contents
            "${_artifact_openusd_errno_contents}")
        set(_artifact_openusd_errno_write TRUE)
        if(EXISTS "${_artifact_openusd_errno_compat}")
            file(READ "${_artifact_openusd_errno_compat}"
                _artifact_openusd_errno_existing)
            if(_artifact_openusd_errno_existing STREQUAL
               _artifact_openusd_errno_contents)
                set(_artifact_openusd_errno_write FALSE)
            endif()
        endif()
        if(_artifact_openusd_errno_write)
            file(WRITE "${_artifact_openusd_errno_compat}"
                "${_artifact_openusd_errno_contents}")
        endif()

        get_target_property(_artifact_openusd_arch_sources arch SOURCES)
        set(_artifact_openusd_arch_compat_sources)
        set(_artifact_openusd_arch_source_replacements 0)
        foreach(_artifact_openusd_arch_source IN LISTS _artifact_openusd_arch_sources)
            get_filename_component(_artifact_openusd_arch_source_name
                "${_artifact_openusd_arch_source}" NAME)
            if(_artifact_openusd_arch_source_name STREQUAL "demangle.cpp")
                list(APPEND _artifact_openusd_arch_compat_sources
                    "${_artifact_openusd_demangle_compat}")
                math(EXPR _artifact_openusd_arch_source_replacements
                    "${_artifact_openusd_arch_source_replacements} + 1")
            elseif(_artifact_openusd_arch_source_name STREQUAL "errno.cpp")
                list(APPEND _artifact_openusd_arch_compat_sources
                    "${_artifact_openusd_errno_compat}")
                math(EXPR _artifact_openusd_arch_source_replacements
                    "${_artifact_openusd_arch_source_replacements} + 1")
            else()
                list(APPEND _artifact_openusd_arch_compat_sources
                    "${_artifact_openusd_arch_source}")
            endif()
        endforeach()
        if(NOT _artifact_openusd_arch_source_replacements EQUAL 2)
            message(FATAL_ERROR
                "Expected to replace OpenUSD arch demangle.cpp and errno.cpp; "
                "found ${_artifact_openusd_arch_source_replacements} sources.")
        endif()
        set_property(TARGET arch PROPERTY SOURCES
            "${_artifact_openusd_arch_compat_sources}")
    endif()

    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(CMAKE_MODULE_PATH "${_artifact_openusd_module_path}")
    endif()
    set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES
        "${_artifact_openusd_parent_include_dirs}")
    set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS
        "${_artifact_openusd_parent_compile_definitions}")
else()
    add_subdirectory(libs/openusd EXCLUDE_FROM_ALL)
endif()
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_gate_for_usd}" CACHE STRING "Restore import std gate" FORCE)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_artifact_import_std_gate_for_usd}")
set(CMAKE_CXX_MODULE_STD "${_artifact_cxx_module_std_for_usd}" CACHE BOOL "Restore import std setting" FORCE)
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
# Project targets use C++ modules heavily; enable scanning for local code.
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
# Math.Vec and other application modules use GLM GTX extensions. Set the
# opt-in consistently for local targets, including C++ module dependency scans.
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
add_subdirectory(ArtifactCore)
# Artifact's standalone text GPU runtimes still link the historical
# ArtifactCoreTextRuntime name. Keep that dependency mapped to the current
# ArtifactCoreText module owner at the parent integration boundary.
if(TARGET ArtifactCoreText AND NOT TARGET ArtifactCoreTextRuntime)
    add_library(ArtifactCoreTextRuntime ALIAS ArtifactCoreText)
endif()
if(ARTIFACT_COMPILER_CLANG_GNU)
    # ArtifactCore's module targets disable GNU language extensions. Preserve
    # that setting for subsequent local subprojects so their BMIs can import
    # ArtifactCore's BMIs without a C++ language-mode mismatch.
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()
if(ARTIFACT_COMPILER_CLANG_GNU AND WIN32)
    # FindOpenMP may cache `-Xclang -fopenmp` for upstream Clang with the
    # Windows MSVC ABI. CMake's P1689 scan rule appends `-x c++` after target
    # flags, so the dangling `-Xclang` swallows that language flag. The GNU
    # driver accepts `-fopenmp` directly and still links the discovered libomp.
    set(OpenMP_C_FLAGS "-fopenmp" CACHE STRING "OpenMP C flags for upstream Clang" FORCE)
    set(OpenMP_CXX_FLAGS "-fopenmp" CACHE STRING "OpenMP CXX flags for upstream Clang" FORCE)
endif()
if(ARTIFACT_COMPILER_CLANG_GNU AND WIN32)
    # FindOpenMP identifies the -fopenmp compile flag but cannot reliably infer
    # the MSVC-ABI LLVM runtime library name on Windows. Resolve libomp beside
    # the selected LLVM toolchain and provide the same explicit runtime mapping
    # used by the clang-cl lane.
    get_filename_component(_artifact_clang_gnu_bin
        "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_library(_artifact_clang_gnu_libomp
        NAMES libomp
        HINTS "${_artifact_clang_gnu_bin}/../lib"
        NO_DEFAULT_PATH)
    if(_artifact_clang_gnu_libomp)
        set(OpenMP_C_LIB_NAMES libomp CACHE STRING
            "OpenMP C runtime for upstream Clang" FORCE)
        set(OpenMP_CXX_LIB_NAMES libomp CACHE STRING
            "OpenMP CXX runtime for upstream Clang" FORCE)
        set(OpenMP_libomp_LIBRARY "${_artifact_clang_gnu_libomp}" CACHE FILEPATH
            "LLVM OpenMP runtime for upstream Clang" FORCE)
    endif()
endif()
if(ARTIFACT_COMPILER_CLANG_CL AND WIN32)
    # CMake's FindOpenMP does not recognize clang-cl's MSVC-style /openmp
    # option. Use the compiler's bundled LLVM OpenMP runtime when present.
    get_filename_component(_artifact_clangcl_bin
        "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_library(_artifact_clangcl_libomp
        NAMES libomp
        HINTS "${_artifact_clangcl_bin}/../lib"
        NO_DEFAULT_PATH)
    if(_artifact_clangcl_libomp)
        set(OpenMP_C_FLAGS "/openmp" CACHE STRING
            "OpenMP C flags for clang-cl" FORCE)
        set(OpenMP_CXX_FLAGS "/openmp" CACHE STRING
            "OpenMP CXX flags for clang-cl" FORCE)
        set(OpenMP_C_LIB_NAMES libomp CACHE STRING
            "OpenMP C runtime for clang-cl" FORCE)
        set(OpenMP_CXX_LIB_NAMES libomp CACHE STRING
            "OpenMP CXX runtime for clang-cl" FORCE)
        set(OpenMP_libomp_LIBRARY "${_artifact_clangcl_libomp}" CACHE FILEPATH
            "LLVM OpenMP runtime for clang-cl" FORCE)
    endif()
endif()
add_subdirectory(ArtifactWidgets)
add_subdirectory(Artifact)
add_subdirectory(tools/Particle2DPlayground)
add_subdirectory(tools/Particle3DPlayground)
add_subdirectory(tools/Light3DPlayground)
add_subdirectory(tools/VideoPreviewBench)
add_subdirectory(ArtifactRenderer)
add_subdirectory(artifacts-plugin-runner)
# ArtifactPr is an opt-in target: it is a supported application, but building
# both editors in one configure pass widens the build. Enable it explicitly
# with -DARTIFACT_BUILD_PR=ON.
# ArtifactPr no longer depends on QtAdvancedDocking; it shares the
# Artifact.DockManager contract and Artifact.NativeDockSurface from the
# ArtifactDockFoundation target. The vcpkg "qt-advanced-docking-system"
# dependency remains only for the opt-in ARTIFACT_QADS_COMPAT_* backends
# in Artifact.
if(ARTIFACT_BUILD_PR)
    add_subdirectory(ArtifactPr)
endif()

# The explicit MSVC std.ifc is needed only by translation units that directly
# import the named standard-library module.  Do not place it on an entire
# target: modules that include STL headers in their global module fragment
# would then mix two representations of the same standard-library entities
# when importing their IFCs (MSVC C1116, commonly reported in stop_token).
if(MSVC AND ARTIFACT_USE_EXPLICIT_STD_IFC AND _artifact_std_ifc)
    get_property(_artifact_all_child_directories DIRECTORY PROPERTY SUBDIRECTORIES)
    foreach(_artifact_child_directory IN LISTS _artifact_all_child_directories)
        get_property(_artifact_child_targets
            DIRECTORY "${_artifact_child_directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_artifact_target IN LISTS _artifact_child_targets)
            if(NOT _artifact_target MATCHES "^Artifact")
                continue()
            endif()
            get_target_property(_artifact_target_type ${_artifact_target} TYPE)
            if(NOT _artifact_target_type MATCHES
               "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
                continue()
            endif()
            get_target_property(_artifact_target_sources ${_artifact_target} SOURCES)
            foreach(_artifact_source IN LISTS _artifact_target_sources)
                if(NOT IS_ABSOLUTE "${_artifact_source}")
                    get_filename_component(_artifact_source
                        "${CMAKE_CURRENT_SOURCE_DIR}/${_artifact_source}" ABSOLUTE)
                endif()
                if(NOT EXISTS "${_artifact_source}")
                    continue()
                endif()
                file(STRINGS "${_artifact_source}" _artifact_import_std_lines
                    REGEX "^[ \t]*import[ \t]+std[ \t]*;")
                if(NOT _artifact_import_std_lines)
                    continue()
                endif()
                set_property(SOURCE "${_artifact_source}" APPEND PROPERTY
                    COMPILE_OPTIONS "/reference;std=${_artifact_std_ifc}")
                set_property(SOURCE "${_artifact_source}" APPEND PROPERTY
                    OBJECT_DEPENDS "${_artifact_std_ifc}")
            endforeach()
        endforeach()
    endforeach()
endif()

# CMake's native __CMAKE::CXX23 provider and the project-owned MSVC std.ifc
# cannot be enabled on the same target: MSVC reports an ambiguous `std` IFC.
# Child projects add their targets before this point, so normalize all local
# Artifact targets here when the explicit provider is active.
if(MSVC AND ARTIFACT_USE_EXPLICIT_STD_IFC)
    get_property(_artifact_child_directories DIRECTORY PROPERTY SUBDIRECTORIES)
    foreach(_artifact_child_directory IN LISTS _artifact_child_directories)
        get_property(_artifact_child_targets
            DIRECTORY "${_artifact_child_directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_artifact_child_target IN LISTS _artifact_child_targets)
            if(_artifact_child_target MATCHES "^Artifact")
                set_property(TARGET "${_artifact_child_target}" PROPERTY CXX_MODULE_STD OFF)
            endif()
        endforeach()
    endforeach()
endif()

# ArtifactPr is added above with the other child directories (Verdigris-based,
# no Qt MOC pass required).

add_custom_target(check_module_hygiene
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUTF8=1
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_module_hygiene.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking C++ module hygiene rules")

add_custom_target(check_source_manifests
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUTF8=1
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_source_manifests.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking explicit source manifests and focused pack ownership")

# ---------------------------------------------------------------------------
# テスト設定
# ---------------------------------------------------------------------------
option(ARTIFACT_BUILD_TESTS "Build unit tests" OFF)
option(ARTIFACT_ENABLE_TEXT_LAYER_ANIMATOR_TEST
    "Register the Artifact TextLayer animator integration test with CTest"
    OFF)
option(ARTIFACT_ENABLE_LAYER_TEST
    "Register isolated Artifact layer integration tests with CTest"
    OFF)
option(ARTIFACT_ENABLE_OFFLINE_RENDER_TEST
    "Register standalone and composition-level headless render tests with CTest"
    OFF)
option(ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS
    "Build only the standalone CPU image effect contract tests"
    OFF)
option(ARTIFACT_ENABLE_RAY_TRACING_TEST
    "Build the standalone ArtifactCore Diligent ray tracing acceptance test"
    OFF)
option(ARTIFACT_ENABLE_TRACKING_VIDEO_TEST
    "Build the standalone MotionTracker video-input test"
    OFF)
option(ARTIFACT_ENABLE_KEYING_VIDEO_TEST
    "Build the standalone ChromaKey video-input test"
    OFF)
option(ARTIFACT_BUILD_FBX_ANIMATION_VIEWER
    "Build the independent FBX animation preview executable"
    OFF)
option(ARTIFACT_BUILD_CAMERA_FRUSTUM_VIEWER
    "Build the independent Composition Viewport camera frustum test executable"
    OFF)
option(ARTIFACT_BUILD_LAYER_PERSISTENCE_ROUNDTRIP_TEST
    "Build the independent layer project save/reload round-trip test"
    OFF)
option(ARTIFACT_BUILD_MPM_SAND_PERSISTENCE_REPLAY_TEST
    "Build the independent MPM sand persistence and deterministic replay test"
    OFF)
option(ARTIFACT_BUILD_PLUGIN_HOST_SMOKE_TEST
    "Build the standalone VST3/CLAP real-plugin host smoke test"
    OFF)
option(ARTIFACT_BUILD_AUDIO_BUS_VIEWER
    "Build the independent audio bus routing and mixer test executable"
    OFF)
option(ARTIFACT_BUILD_PRECOMPOSE_VIEWER
    "Build the independent precompose GUI and MCP regression executable"
    OFF)
option(ARTIFACT_BUILD_COMPOSITION_PLAYBACK_STRESS_TEST
    "Build the independent composition playback stress test executable"
    OFF)
option(ARTIFACT_BUILD_COMPOSITION_LAYER_SURFACE_CACHE_TEST
    "Build the independent Composition layer surface cache test executable"
    OFF)
option(ARTIFACT_BUILD_COMPOSITION_VIEWPORT_CHANNEL_TEST
    "Build the independent Composition Viewport channel display test executable"
    OFF)

option(ARTIFACT_BUILD_COMPOSITION_MOTION_PATH_TEST
    "Build the independent Composition motion path and Transform integration test executable"
    OFF)
option(ARTIFACT_BUILD_COMPOSITION_VIEWPORT_GRID_TEST
    "Build the independent Composition Viewport grid and overlay capture test executable"
    OFF)

option(ARTIFACT_ENABLE_FBX_IMPORT_TEST
    "Build the FBX animation and embedded-texture importer test"
    OFF)
option(ARTIFACT_ENABLE_SCRIPT_LAYER_INTEGRATION_TEST
    "Build the ArtifactScript-to-layer integration test"
    OFF)
option(ARTIFACT_ENABLE_IMAGE_IO_TEST
    "Build the standalone image import/export contract test"
    OFF)
set(ARTIFACT_FBX_TEST_FILE "" CACHE FILEPATH
    "FBX fixture containing animation and embedded base-color/normal image textures")
set(ARTIFACT_FBX_ANIMATION_TEST_FILE "" CACHE FILEPATH
    "FBX animation fixture (overrides ARTIFACT_FBX_TEST_FILE for animation checks)")
set(ARTIFACT_FBX_BASE_COLOR_TEST_FILE "" CACHE FILEPATH
    "FBX embedded base-color fixture (defaults to ARTIFACT_FBX_TEXTURE_TEST_FILE)")
set(ARTIFACT_FBX_TEXTURE_TEST_FILE "" CACHE FILEPATH
    "FBX embedded-texture fixture (overrides ARTIFACT_FBX_TEST_FILE for texture checks)")
set(ARTIFACT_SCRIPT_LAYER_TEST_FILE "" CACHE FILEPATH
    "ArtifactScript layer integration fixture (defaults to the repository fixture)")

function(artifact_add_cppwinrt_include target_name)
    if(NOT WIN32)
        return()
    endif()

    foreach(_artifact_sdk_include IN LISTS CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES)
        if(_artifact_sdk_include MATCHES "[/\\\\]winrt$")
            get_filename_component(_artifact_sdk_version_root
                "${_artifact_sdk_include}" DIRECTORY)
            set(_artifact_cppwinrt_include
                "${_artifact_sdk_version_root}/cppwinrt")
            if(EXISTS "${_artifact_cppwinrt_include}/winrt/base.h")
                target_include_directories(${target_name} PRIVATE
                    "${_artifact_cppwinrt_include}")
                return()
            endif()
        endif()
    endforeach()

    message(FATAL_ERROR
        "Could not locate the Windows SDK C++/WinRT include directory for ${target_name}")
endfunction()

if(ARTIFACT_BUILD_PRECOMPOSE_VIEWER)
    if(NOT WIN32)
        message(FATAL_ERROR "ArtifactPrecomposeViewer stdio transport currently requires Windows")
    endif()
    add_executable(ArtifactPrecomposeViewer tests/Artifact/PrecomposeViewerMain.cpp)
    target_link_libraries(ArtifactPrecomposeViewer PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactPrecomposeViewer PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src")
    set_target_properties(ArtifactPrecomposeViewer PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
    target_compile_options(ArtifactPrecomposeViewer PRIVATE /utf-8 /EHsc)
    add_custom_command(TARGET ArtifactPrecomposeViewer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactPrecomposeViewer>
            "$<TARGET_FILE_DIR:ArtifactPrecomposeViewer>/"
        COMMAND_EXPAND_LISTS VERBATIM)
endif()

if(ARTIFACT_BUILD_COMPOSITION_MOTION_PATH_TEST AND ARTIFACT_FRONTEND_MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/EnvironmentVariable/EnvironmentVariable.cpp"
        TARGET_DIRECTORY ArtifactCoreEnvironment
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;EnvironmentVariable=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCoreEnvironment.dir/EnvironmentVariable.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Animation/AnimatableTransform2D.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Animation.Transform2D=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Animation.Transform2D.ifc"
        "/reference;Animation.Value=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Animation.Value.ifc"
        "/reference;Frame.Position=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
        "/reference;Frame.Rate=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
        "/reference;Math.Interpolate=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
        "/reference;Time.Rational=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Animation/AnimatableTransform3D.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Animation.Transform3D=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Animation.Transform3D.ifc"
        "/reference;Animation.Value=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Animation.Value.ifc"
        "/reference;Frame.Position=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
        "/reference;Property.Types=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Property.Types.ifc"
        "/reference;Math.Interpolate=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
        "/reference;Container.NamedVector=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
        "/reference;Property.Abstract=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Property.Abstract.ifc"
        "/reference;Time.Rational=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
        "/reference;Frame.Rate=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
        "/reference;Container.Debug=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
        "/reference;Container.Debug.Registry=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
        "/reference;Core.ArtifactArray=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.ArtifactArray.ifc"
        "/reference;Core.ArtifactOptional=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.ArtifactOptional.ifc"
        "/reference;Script.Expression.Evaluator=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
        "/reference;Utils.String.UniString=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Animation/AnimationDynamics.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Animation.Dynamics=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Animation.Dynamics.ifc"
        "/reference;Core.ArtifactString=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Image/ImageF32x4_With_Cache.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Image.GpuImageUpload=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc")
    foreach(_artifact_motionpath_gpu_module IN ITEMS
            "GPUAlgorithms|Graphics.GPUAlgorithms"
            "GPUThreadPool|Graphics.GPUThreadPool"
            "GPUCapabilities|Graphics.GPUCapabilities"
            "GPUAppendConsume|Graphics.GPUAppendConsume")
        string(REPLACE "|" ";" _artifact_motionpath_gpu_fields "${_artifact_motionpath_gpu_module}")
        list(GET _artifact_motionpath_gpu_fields 0 _artifact_motionpath_gpu_name)
        list(GET _artifact_motionpath_gpu_fields 1 _artifact_motionpath_gpu_logical)
        set_property(SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Graphics/${_artifact_motionpath_gpu_name}.cpp"
            TARGET_DIRECTORY ArtifactCore
            PROPERTY COMPILE_OPTIONS
            "/reference;${_artifact_motionpath_gpu_logical}=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/${_artifact_motionpath_gpu_logical}.ifc")
    endforeach()
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/ImageProcessing/AbstractImageEffect.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Graphics.SurfaceColorContract=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
        "/reference;Color.TransferFunction=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Codec/GStreamerDecoder.cpp"
        TARGET_DIRECTORY ArtifactCoreVideo
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Video.GStreamerDecoder=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCoreVideo.dir/Video.GStreamerDecoder.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Codec/EncoderSetting.cpp"
        TARGET_DIRECTORY ArtifactCoreVideo
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Codec.Encoder.Setting=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCoreVideo.dir/Codec.Encoder.Setting.ifc"
        "/reference;Utils.String.UniString=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Animation/AnimatableTransform2D.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Container.NamedVector=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
        "/reference;Container.Debug=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
        "/reference;Container.Debug.Registry=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
        "/reference;Core.ArtifactArray=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.ArtifactArray.ifc"
        "/reference;Core.ArtifactOptional=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.ArtifactOptional.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Codec/MFFrameExtractor.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Container.Debug=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
        "/reference;Container.Debug.Registry=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Codec/MFFrameExtractor.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Codec.MFFrameExtractor=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Codec.MFFrameExtractor.ifc"
        "/reference;Core.Parallel=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
        "/reference;Memory.TrackedPtr=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Channel/Channel.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Channel=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Channel.ifc"
        "/reference;Core.ArtifactString=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/src/Codec/MFEncoder.cpp"
        TARGET_DIRECTORY ArtifactCore
        APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Codec.MFEncoder=${CMAKE_BINARY_DIR}/ArtifactCore/CMakeFiles/ArtifactCore.dir/Codec.MFEncoder.ifc")
endif()
if(ARTIFACT_BUILD_COMPOSITION_MOTION_PATH_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR
            "ArtifactCompositionMotionPathTest currently requires Windows window capture")
    endif()
    add_executable(ArtifactCompositionMotionPathTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/CompositionMotionPathMain.cpp"
    )
    target_link_libraries(ArtifactCompositionMotionPathTest PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactCompositionMotionPathTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    artifact_add_cppwinrt_include(ArtifactCompositionMotionPathTest)
    set_target_properties(ArtifactCompositionMotionPathTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    add_custom_command(TARGET ArtifactCompositionMotionPathTest POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactCompositionMotionPathTest>
            "$<TARGET_FILE_DIR:ArtifactCompositionMotionPathTest>/"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactCompositionMotionPathTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

# FxStudio FX animation test targets (ArtifactFxStudioRenderContractTest and,
# with ARTIFACT_BUILD_FX_STUDIO_TEST=ON, ArtifactFxStudioMcpTest) are defined in
# tests/Artifact/CMakeLists.txt. They compile the FxStudio module units directly
# so their dependency boundary is ArtifactCore + Qt rather than
# ArtifactAppRuntime.

if(ARTIFACT_BUILD_COMPOSITION_VIEWPORT_GRID_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR
            "ArtifactCompositionViewportGridTest currently requires Windows window capture")
    endif()
    add_executable(ArtifactCompositionViewportGridTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/CompositionViewportGridTest/main.cpp"
    )
    target_link_libraries(ArtifactCompositionViewportGridTest PRIVATE ArtifactRender)
    target_include_directories(ArtifactCompositionViewportGridTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    artifact_add_cppwinrt_include(ArtifactCompositionViewportGridTest)
    set_target_properties(ArtifactCompositionViewportGridTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    add_custom_command(TARGET ArtifactCompositionViewportGridTest POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactCompositionViewportGridTest>
            "$<TARGET_FILE_DIR:ArtifactCompositionViewportGridTest>/"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactCompositionViewportGridTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_COMPOSITION_PLAYBACK_STRESS_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR
            "ArtifactCompositionPlaybackStressTest currently requires Windows window capture")
    endif()
    add_executable(ArtifactCompositionPlaybackStressTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/CompositionPlaybackStressMain.cpp"
    )
    target_link_libraries(ArtifactCompositionPlaybackStressTest PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactCompositionPlaybackStressTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
    )
    artifact_add_cppwinrt_include(ArtifactCompositionPlaybackStressTest)
    set_target_properties(ArtifactCompositionPlaybackStressTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    add_custom_command(TARGET ArtifactCompositionPlaybackStressTest POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactCompositionPlaybackStressTest>
            "$<TARGET_FILE_DIR:ArtifactCompositionPlaybackStressTest>/"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactCompositionPlaybackStressTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_COMPOSITION_LAYER_SURFACE_CACHE_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR
            "ArtifactCompositionLayerSurfaceCacheTest currently requires Windows window capture")
    endif()
    add_executable(ArtifactCompositionLayerSurfaceCacheTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/CompositionLayerSurfaceCacheMain.cpp"
    )
    target_link_libraries(ArtifactCompositionLayerSurfaceCacheTest PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactCompositionLayerSurfaceCacheTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    artifact_add_cppwinrt_include(ArtifactCompositionLayerSurfaceCacheTest)
    set_target_properties(ArtifactCompositionLayerSurfaceCacheTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    add_custom_command(TARGET ArtifactCompositionLayerSurfaceCacheTest POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactCompositionLayerSurfaceCacheTest>
            "$<TARGET_FILE_DIR:ArtifactCompositionLayerSurfaceCacheTest>/"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactCompositionLayerSurfaceCacheTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_COMPOSITION_VIEWPORT_CHANNEL_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR
            "ArtifactCompositionViewportChannelTest currently requires Windows window capture")
    endif()
    add_executable(ArtifactCompositionViewportChannelTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/CompositionViewportChannelTestMain.cpp"
    )
    target_link_libraries(ArtifactCompositionViewportChannelTest PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactCompositionViewportChannelTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_compile_definitions(ArtifactCompositionViewportChannelTest PRIVATE
        ARTIFACT_CHANNEL_TEST_MODEL_OBJ_PATH="${CMAKE_SOURCE_DIR}/tests/models/light_composition_icosphere.obj"
    )
    artifact_add_cppwinrt_include(ArtifactCompositionViewportChannelTest)
    set_target_properties(ArtifactCompositionViewportChannelTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    add_custom_command(TARGET ArtifactCompositionViewportChannelTest POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactCompositionViewportChannelTest>
            "$<TARGET_FILE_DIR:ArtifactCompositionViewportChannelTest>/"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactCompositionViewportChannelTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_AUDIO_BUS_VIEWER)
    add_executable(ArtifactAudioBusViewer
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/AudioBusViewerMain.cpp"
    )
    target_link_libraries(ArtifactAudioBusViewer PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactAudioBusViewer PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    set_target_properties(ArtifactAudioBusViewer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactAudioBusViewer POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactAudioBusViewer>
                "$<TARGET_FILE_DIR:ArtifactAudioBusViewer>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )

        set(_artifact_audio_bus_vcpkg_installed "${VCPKG_INSTALLED}")
        if(NOT EXISTS "${_artifact_audio_bus_vcpkg_installed}/debug/bin"
           AND DEFINED VCPKG_INSTALLED_DIR
           AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows")
            set(_artifact_audio_bus_vcpkg_installed "${VCPKG_INSTALLED_DIR}/x64-windows")
        endif()
        if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
            set(_artifact_audio_bus_vcpkg_installed
                "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        endif()

        find_program(ARTIFACT_AUDIO_BUS_WINDEPLOYQT_EXECUTABLE NAMES windeployqt)
        if(ARTIFACT_AUDIO_BUS_WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ArtifactAudioBusViewer POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DWINDEPLOYQT_EXECUTABLE=${ARTIFACT_AUDIO_BUS_WINDEPLOYQT_EXECUTABLE}"
                    "-DTARGET_FILE=$<TARGET_FILE:ArtifactAudioBusViewer>"
                    "-DVCPKG_INSTALLED=${_artifact_audio_bus_vcpkg_installed}"
                    "-DCONFIG=$<CONFIG>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/cmake/RunWindeployqt.cmake"
                VERBATIM
            )
        endif()

        add_custom_command(TARGET ArtifactAudioBusViewer POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactAudioBusViewer>"
                "-DVCPKG_INSTALLED=${_artifact_audio_bus_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/cmake/CopyQtPlatformPlugin.cmake"
            VERBATIM
        )
        add_custom_command(TARGET ArtifactAudioBusViewer POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactAudioBusViewer>"
                "-DVCPKG_INSTALLED=${_artifact_audio_bus_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/cmake/CopyVcpkgRuntimeDeps.cmake"
            VERBATIM
        )
    endif()
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactAudioBusViewer PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_FBX_ANIMATION_VIEWER)
    add_executable(ArtifactFbxAnimationViewer
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/FbxAnimationViewerMain.cpp"
    )
    target_link_libraries(ArtifactFbxAnimationViewer PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactFbxAnimationViewer PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
    )
    artifact_add_cppwinrt_include(ArtifactFbxAnimationViewer)
    set_target_properties(ArtifactFbxAnimationViewer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactFbxAnimationViewer POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactFbxAnimationViewer>
                "$<TARGET_FILE_DIR:ArtifactFbxAnimationViewer>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactFbxAnimationViewer PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_CAMERA_FRUSTUM_VIEWER)
    add_executable(ArtifactCameraFrustumViewer
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/CameraFrustumViewerMain.cpp"
    )
    target_link_libraries(ArtifactCameraFrustumViewer PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactCameraFrustumViewer PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
    )
    artifact_add_cppwinrt_include(ArtifactCameraFrustumViewer)
    set_target_properties(ArtifactCameraFrustumViewer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactCameraFrustumViewer POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactCameraFrustumViewer>
                "$<TARGET_FILE_DIR:ArtifactCameraFrustumViewer>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactCameraFrustumViewer PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_LAYER_PERSISTENCE_ROUNDTRIP_TEST)
    add_executable(ArtifactLayerPersistenceRoundTripTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/ArtifactLayerPersistenceRoundTripMain.cpp"
    )
    target_link_libraries(ArtifactLayerPersistenceRoundTripTest PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactLayerPersistenceRoundTripTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
    )
    artifact_add_cppwinrt_include(ArtifactLayerPersistenceRoundTripTest)
    set_target_properties(ArtifactLayerPersistenceRoundTripTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactLayerPersistenceRoundTripTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactLayerPersistenceRoundTripTest>
                "$<TARGET_FILE_DIR:ArtifactLayerPersistenceRoundTripTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactLayerPersistenceRoundTripTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_MPM_SAND_PERSISTENCE_REPLAY_TEST)
    enable_testing()
    add_executable(ArtifactMpmSandPersistenceReplayTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/MpmSandPersistenceReplayMain.cpp"
    )
    target_link_libraries(ArtifactMpmSandPersistenceReplayTest PRIVATE ArtifactAppRuntime)
    target_include_directories(ArtifactMpmSandPersistenceReplayTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/verdigris/src"
    )
    artifact_add_cppwinrt_include(ArtifactMpmSandPersistenceReplayTest)
    set_target_properties(ArtifactMpmSandPersistenceReplayTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactMpmSandPersistenceReplayTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactMpmSandPersistenceReplayTest>
                "$<TARGET_FILE_DIR:ArtifactMpmSandPersistenceReplayTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactMpmSandPersistenceReplayTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /EHsc
        )
    endif()
    add_test(NAME ArtifactMpmSandPersistenceReplayTest
        COMMAND $<TARGET_FILE:ArtifactMpmSandPersistenceReplayTest>
    )
    set_tests_properties(ArtifactMpmSandPersistenceReplayTest PROPERTIES
        LABELS "mpm;constitutive;determinism;persistence"
        TIMEOUT 120
    )
endif()

if(ARTIFACT_BUILD_PLUGIN_HOST_SMOKE_TEST)
    add_executable(ArtifactPluginHostSmoke
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/PluginHostSmokeMain.cpp"
    )
    target_link_libraries(ArtifactPluginHostSmoke PRIVATE
        ArtifactCoreAudio ArtifactCoreVST3
    )
    if(ARTIFACT_CXX23_MODULE_STD_AVAILABLE AND TARGET __CMAKE::CXX23)
        set_property(TARGET ArtifactPluginHostSmoke PROPERTY CXX_MODULE_STD ON)
    else()
        set_property(TARGET ArtifactPluginHostSmoke PROPERTY CXX_MODULE_STD OFF)
    endif()
    set_target_properties(ArtifactPluginHostSmoke PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactPluginHostSmoke POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactPluginHostSmoke>
                "$<TARGET_FILE_DIR:ArtifactPluginHostSmoke>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    add_library(ArtifactClapHostTestPlugin MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/FakeClapPlugin.cpp"
    )
    set_target_properties(ArtifactClapHostTestPlugin PROPERTIES
        PREFIX ""
        SUFFIX ".clap"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/clap-test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/clap-test"
    )
    add_library(ArtifactVst3HostTestPlugin MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Artifact/FakeVst3Plugin.cpp"
    )
    target_link_libraries(ArtifactVst3HostTestPlugin PRIVATE ArtifactCoreVST3)
    set_target_properties(ArtifactVst3HostTestPlugin PROPERTIES
        PREFIX ""
        SUFFIX ".vst3"
        OUTPUT_NAME "ArtifactVst3AudioBus"
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/vst3-test/ArtifactVst3AudioBus/Contents/x86_64-win"
        LIBRARY_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/vst3-test/ArtifactVst3AudioBus/Contents/x86_64-win"
    )
    add_dependencies(ArtifactPluginHostSmoke
        ArtifactClapHostTestPlugin ArtifactVst3HostTestPlugin)
    enable_testing()
    add_test(NAME ArtifactClapPluginHostSmoke
        COMMAND $<TARGET_FILE:ArtifactPluginHostSmoke> clap
                $<TARGET_FILE:ArtifactClapHostTestPlugin>
    )
    set_tests_properties(ArtifactClapPluginHostSmoke PROPERTIES
        LABELS "audio;audio-bus;clap;plugin-host;sidechain"
        TIMEOUT 30
    )
    add_test(NAME ArtifactVst3AudioBusHostSmoke
        COMMAND $<TARGET_FILE:ArtifactPluginHostSmoke> vst3-bus
                $<TARGET_FILE:ArtifactVst3HostTestPlugin>
    )
    set_tests_properties(ArtifactVst3AudioBusHostSmoke PROPERTIES
        LABELS "audio;vst3;plugin-host;audio-bus;sidechain"
        TIMEOUT 30
    )
    add_test(NAME ArtifactVst3InvalidBusRejected
        COMMAND $<TARGET_FILE:ArtifactPluginHostSmoke> vst3-invalid-bus
                $<TARGET_FILE:ArtifactVst3HostTestPlugin>
    )
    set_tests_properties(ArtifactVst3InvalidBusRejected PROPERTIES
        LABELS "audio;vst3;plugin-host;validation"
        TIMEOUT 30
    )
    add_test(NAME ArtifactAudioBusClapVst3ChainSmoke
        COMMAND $<TARGET_FILE:ArtifactPluginHostSmoke> audio-bus-combined
                $<TARGET_FILE:ArtifactVst3HostTestPlugin>
                $<TARGET_FILE:ArtifactClapHostTestPlugin>
    )
    set_tests_properties(ArtifactAudioBusClapVst3ChainSmoke PROPERTIES
        LABELS "audio;audio-bus;clap;vst3;plugin-host;sidechain;integration"
        TIMEOUT 30
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactPluginHostSmoke PRIVATE
            /Zf $<$<BOOL:${ARTIFACT_COMPILER_MSVC_CL}>:/std:c++latest> $<$<BOOL:${ARTIFACT_COMPILER_MSVC_CL}>:/experimental:module> /Zc:__cplusplus
            /permissive- /utf-8 /EHsc
        )
    endif()
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_TEXT_LAYER_ANIMATOR_TEST
   OR ARTIFACT_ENABLE_LAYER_TEST OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST
   OR ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS OR ARTIFACT_ENABLE_TRACKING_VIDEO_TEST
   OR ARTIFACT_ENABLE_KEYING_VIDEO_TEST OR ARTIFACT_ENABLE_RAY_TRACING_TEST
   OR ARTIFACT_ENABLE_IMAGE_IO_TEST)
    enable_testing()

    if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_TEXT_LAYER_ANIMATOR_TEST)
        # The app-owned TextLayer integration test has no GTest dependency.
        add_test(NAME ArtifactTextLayerAnimatorIntegrationTest
            COMMAND $<TARGET_FILE:Artifact>
        )
        set_tests_properties(ArtifactTextLayerAnimatorIntegrationTest PROPERTIES
            ENVIRONMENT "ARTIFACT_RUN_TEXT_LAYER_ANIMATOR_TESTS=1"
            LABELS "animation;text;layer;integration"
            TIMEOUT 120
        )
    endif()

    if(ARTIFACT_ENABLE_LAYER_TEST)
        add_test(NAME ArtifactAdjustmentLayerTest
            COMMAND $<TARGET_FILE:Artifact>
        )
        set_tests_properties(ArtifactAdjustmentLayerTest PROPERTIES
            ENVIRONMENT "ARTIFACT_RUN_ADJUSTMENT_LAYER_TESTS=1"
            LABELS "layer;integration"
            TIMEOUT 120
        )

        add_test(NAME ArtifactLayerGroupTest
            COMMAND $<TARGET_FILE:Artifact>
        )
        set_tests_properties(ArtifactLayerGroupTest PROPERTIES
            ENVIRONMENT "ARTIFACT_RUN_LAYER_GROUP_TESTS=1"
            LABELS "layer;integration"
            TIMEOUT 120
        )

        add_test(NAME ArtifactShapeLayerTest
            COMMAND $<TARGET_FILE:Artifact>
        )
        set_tests_properties(ArtifactShapeLayerTest PROPERTIES
            ENVIRONMENT "ARTIFACT_RUN_SHAPE_LAYER_TESTS=1"
            LABELS "layer;integration"
            TIMEOUT 120
        )

        add_test(NAME ArtifactSolidLayerTest
            COMMAND $<TARGET_FILE:Artifact>
        )
        set_tests_properties(ArtifactSolidLayerTest PROPERTIES
            ENVIRONMENT "ARTIFACT_RUN_SOLID_LAYER_TESTS=1"
            LABELS "layer;integration"
            TIMEOUT 120
        )
    endif()

    if(ARTIFACT_ENABLE_OFFLINE_RENDER_TEST)
        add_test(NAME ArtifactOfflineRenderTest
            COMMAND $<TARGET_FILE:Artifact>
        )
        set_tests_properties(ArtifactOfflineRenderTest PROPERTIES
            ENVIRONMENT "ARTIFACT_RUN_OFFLINE_RENDER_TESTS=1"
            LABELS "renderer;gpu;offline;integration"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
    endif()
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS
   OR ARTIFACT_ENABLE_TRACKING_VIDEO_TEST OR ARTIFACT_ENABLE_KEYING_VIDEO_TEST
   OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST OR ARTIFACT_ENABLE_RAY_TRACING_TEST
   OR ARTIFACT_ENABLE_FBX_IMPORT_TEST OR ARTIFACT_ENABLE_LAYER_TEST
   OR ARTIFACT_ENABLE_SCRIPT_LAYER_INTEGRATION_TEST
   OR ARTIFACT_ENABLE_IMAGE_IO_TEST OR ARTIFACT_BUILD_PR_TESTS
   OR ARTIFACT_BUILD_FX_STUDIO_TEST)
    find_package(GTest CONFIG QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    elseif(ARTIFACT_ENABLE_FBX_IMPORT_TEST OR ARTIFACT_ENABLE_IMAGE_IO_TEST
           OR ARTIFACT_ENABLE_SCRIPT_LAYER_INTEGRATION_TEST
           OR ARTIFACT_BUILD_PR_TESTS OR ARTIFACT_BUILD_FX_STUDIO_TEST)
        message(FATAL_ERROR
            "An explicitly enabled standalone image/FBX/script-layer/ArtifactPr/FxStudio test requires GTest, but GTest was not found.")
    else()
        message(WARNING "GTest was not found; skipping GTest-based tests.")
    endif()
endif()

# Keep each long-running edit-sequence fuzz seed independently selectable and
# usable without enabling or finding GoogleTest. Every seed gets its own
# Artifact process so retained project state cannot leak between seeds.
option(ARTIFACT_ENABLE_EDIT_SEQUENCE_FUZZ_TEST
    "Register the standalone edit-sequence fuzz integration test with CTest"
    OFF)
if(ARTIFACT_ENABLE_EDIT_SEQUENCE_FUZZ_TEST)
    if(NOT ARTIFACT_BUILD_TESTS)
        enable_testing()
    endif()
    foreach(_artifact_edit_sequence_fuzz_seed IN ITEMS 4A17C0DE 51A7E123 BADC0FFEE)
        set(_artifact_edit_sequence_fuzz_test
            "ArtifactEditSequenceFuzzTest_${_artifact_edit_sequence_fuzz_seed}")
        add_test(NAME "${_artifact_edit_sequence_fuzz_test}"
            COMMAND ${CMAKE_COMMAND} -E env
                "ARTIFACT_RUN_EDIT_SEQUENCE_FUZZ=1"
                "ARTIFACT_EDIT_SEQUENCE_FUZZ_SEED=0x${_artifact_edit_sequence_fuzz_seed}"
                $<TARGET_FILE:Artifact>
        )
        set_tests_properties("${_artifact_edit_sequence_fuzz_test}" PROPERTIES
            LABELS "stress;integration;edit-sequence"
            TIMEOUT 180
        )
    endforeach()
endif()
