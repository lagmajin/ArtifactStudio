# Upstream clang++ on Windows: keep the GNU driver frontend and explicitly
# select the Microsoft target/ABI. This file intentionally does not enable
# CMake's MSVC frontend variant; Clang's P1689 module map rules remain active.
if(NOT WIN32)
    message(FATAL_ERROR "toolchain-clangxx-msvc-abi.cmake is Windows-only.")
endif()

set(_artifact_clangxx_bin "" CACHE PATH "Directory containing upstream Clang tools")
if(NOT _artifact_clangxx_bin)
    find_program(_artifact_clangxx_driver NAMES clang++.exe clang++)
    if(_artifact_clangxx_driver)
        get_filename_component(_artifact_clangxx_bin "${_artifact_clangxx_driver}" DIRECTORY)
    endif()
endif()
if(NOT _artifact_clangxx_bin)
    message(FATAL_ERROR "Set _artifact_clangxx_bin to the directory containing clang++ and clang-scan-deps.")
endif()

set(_artifact_msvc_root "" CACHE PATH "MSVC toolset root used with upstream Clang")
if(NOT _artifact_msvc_root AND DEFINED ENV{VCToolsInstallDir})
    file(TO_CMAKE_PATH "$ENV{VCToolsInstallDir}" _artifact_msvc_root)
endif()
if(NOT _artifact_msvc_root)
    file(GLOB _artifact_msvc_candidates
        "$ENV{ProgramFiles}/Microsoft Visual Studio/18/*/VC/Tools/MSVC/14.*/")
    if(_artifact_msvc_candidates)
        list(SORT _artifact_msvc_candidates ORDER DESCENDING)
        list(GET _artifact_msvc_candidates 0 _artifact_msvc_root)
    endif()
endif()
set(_artifact_sdk_root "" CACHE PATH "Windows 10 SDK root")
if(NOT _artifact_sdk_root)
    set(_artifact_sdk_root "C:/Program Files (x86)/Windows Kits/10")
endif()
set(_artifact_sdk_version "" CACHE STRING "Windows SDK version")
if(NOT _artifact_sdk_version AND EXISTS "${_artifact_sdk_root}/Include")
    file(GLOB _artifact_sdk_candidates LIST_DIRECTORIES TRUE "${_artifact_sdk_root}/Include/10.*")
    if(_artifact_sdk_candidates)
        list(SORT _artifact_sdk_candidates ORDER DESCENDING)
        list(GET _artifact_sdk_candidates 0 _artifact_sdk_include)
        get_filename_component(_artifact_sdk_version "${_artifact_sdk_include}" NAME)
    endif()
endif()

foreach(_artifact_required
        "${_artifact_clangxx_bin}/clang.exe"
        "${_artifact_clangxx_bin}/clang++.exe"
        "${_artifact_clangxx_bin}/clang-scan-deps.exe"
        "${_artifact_clangxx_bin}/llvm-rc.exe"
        "${_artifact_msvc_root}/bin/Hostx64/x64/link.exe"
        "${_artifact_sdk_root}/bin/${_artifact_sdk_version}/x64/mt.exe")
    if(NOT EXISTS "${_artifact_required}")
        message(FATAL_ERROR "Required Windows Clang/MSVC tool was not found: ${_artifact_required}")
    endif()
endforeach()

set(CMAKE_C_COMPILER "${_artifact_clangxx_bin}/clang.exe" CACHE FILEPATH "Clang C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${_artifact_clangxx_bin}/clang++.exe" CACHE FILEPATH "Upstream Clang C++ compiler" FORCE)
set(CMAKE_CXX_COMPILER_TARGET "x86_64-pc-windows-msvc" CACHE STRING "MSVC ABI target for upstream Clang" FORCE)
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${_artifact_clangxx_bin}/clang-scan-deps.exe" CACHE FILEPATH "Clang module dependency scanner" FORCE)
set(CMAKE_AR "${_artifact_clangxx_bin}/llvm-ar.exe" CACHE FILEPATH "LLVM archiver" FORCE)
set(CMAKE_LINKER "${_artifact_clangxx_bin}/lld-link.exe" CACHE FILEPATH "LLVM COFF linker selected by clang++" FORCE)
set(CMAKE_RC_COMPILER "${_artifact_clangxx_bin}/llvm-rc.exe" CACHE FILEPATH "LLVM resource compiler for Clang's Windows RC preprocessing" FORCE)
set(CMAKE_MT "${_artifact_sdk_root}/bin/${_artifact_sdk_version}/x64/mt.exe" CACHE FILEPATH "Windows manifest tool" FORCE)

set(CMAKE_CXX_SCAN_FOR_MODULES ON CACHE BOOL "Enable Clang P1689 module scanning" FORCE)
set(CMAKE_CXX_STANDARD 23 CACHE STRING "Smoke C++ language level" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require smoke C++ language level" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC ABI runtime" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded" CACHE STRING "Emit CodeView in object files" FORCE)

set(CMAKE_C_FLAGS_INIT "--target=x86_64-pc-windows-msvc -fms-compatibility -fms-extensions")
set(CMAKE_CXX_FLAGS_INIT "--target=x86_64-pc-windows-msvc -fms-compatibility -fms-extensions")

# Windows-Clang.cmake selects MinGW-flavored rules for the GNU frontend. The
# language rules override preserves Clang's P1689 module scanner while fixing
# the MSVC ABI target's linker defaults.
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
    "${CMAKE_CURRENT_LIST_DIR}/clangxx-msvc-abi-rules.cmake")

set(ENV{VCToolsInstallDir} "${_artifact_msvc_root}/")
set(ENV{WindowsSdkDir} "${_artifact_sdk_root}/")
set(ENV{WindowsSDKVersion} "${_artifact_sdk_version}/")
set(ENV{INCLUDE} "${_artifact_msvc_root}/include;${_artifact_sdk_root}/Include/${_artifact_sdk_version}/ucrt;${_artifact_sdk_root}/Include/${_artifact_sdk_version}/shared;${_artifact_sdk_root}/Include/${_artifact_sdk_version}/um;${_artifact_sdk_root}/Include/${_artifact_sdk_version}/winrt")
set(ENV{LIB} "${_artifact_msvc_root}/lib/x64;${_artifact_sdk_root}/Lib/${_artifact_sdk_version}/um/x64;${_artifact_sdk_root}/Lib/${_artifact_sdk_version}/ucrt/x64")

set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version" FORCE)
