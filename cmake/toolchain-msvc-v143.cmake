set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING "Required for import std" FORCE)
set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "Enable import std support during toolchain detection" FORCE)
set(CMAKE_CXX_STANDARD 23 CACHE STRING "Required for import std detection" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")
set(_artifact_vcpkg_installed_dir "${CMAKE_SOURCE_DIR}/out/vcpkg_installed")
set(_artifact_shared_vcpkg_installed_dir "C:/vcpkg/installed")
if(EXISTS "${_artifact_vcpkg_installed_dir}")
    set(VCPKG_INSTALLED_DIR "${_artifact_vcpkg_installed_dir}" CACHE STRING "vcpkg installed dir" FORCE)
elseif(EXISTS "${_artifact_shared_vcpkg_installed_dir}")
    set(VCPKG_INSTALLED_DIR "${_artifact_shared_vcpkg_installed_dir}" CACHE STRING "vcpkg installed dir" FORCE)
else()
    set(VCPKG_INSTALLED_DIR "${_artifact_vcpkg_installed_dir}" CACHE STRING "vcpkg installed dir" FORCE)
endif()

# Find cl.exe for MSVC v143
set(_v143_found_cl "")
if(DEFINED CMAKE_C_COMPILER AND EXISTS "${CMAKE_C_COMPILER}")
    set(_v143_found_cl "${CMAKE_C_COMPILER}")
endif()

if(NOT _v143_found_cl)
    foreach(_vs_major IN ITEMS 18 2022)
        foreach(_ed IN ITEMS Community Enterprise Professional Preview Insiders)
            file(GLOB _v143_cands
                "C:/Program Files/Microsoft Visual Studio/${_vs_major}/${_ed}/VC/Tools/MSVC/14.*/bin/Hostx64/x64/cl.exe"
                "C:/Program Files/Microsoft Visual Studio/${_vs_major}/${_ed}/VC/Tools/MSVC/14.*/bin/HostX64/x64/cl.exe")
            if(_v143_cands)
                list(SORT _v143_cands ORDER DESCENDING)
                list(GET _v143_cands 0 _v143_found_cl)
                break()
            endif()
        endforeach()
        if(_v143_found_cl)
            break()
        endif()
    endforeach()
endif()

if(NOT _v143_found_cl)
    file(GLOB _v143_cands
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.*/bin/Hostx64/x64/cl.exe"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.*/bin/HostX64/x64/cl.exe")
    if(_v143_cands)
        list(SORT _v143_cands ORDER DESCENDING)
        list(GET _v143_cands 0 _v143_found_cl)
    endif()
endif()

if(_v143_found_cl)
    set(CMAKE_C_COMPILER "${_v143_found_cl}" CACHE STRING "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER "${_v143_found_cl}" CACHE STRING "C++ compiler" FORCE)

    get_filename_component(_v143_cl_bin_dir "${_v143_found_cl}" DIRECTORY)
    get_filename_component(_v143_cl_host_dir "${_v143_cl_bin_dir}" DIRECTORY)
    get_filename_component(_v143_cl_bin_root "${_v143_cl_host_dir}" DIRECTORY)
    get_filename_component(_v143_msvc_root "${_v143_cl_bin_root}" DIRECTORY)

    # Ensure the MSVC debug runtime directory is available to the linker.
    # Some environments do not propagate LIB to the Ninja build step, which
    # leaves MSVCRTD.lib unresolved even though the file exists locally.
    set(_v143_msvc_lib_dir "${_v143_msvc_root}/lib/x64")
    set(_v143_msvc_atlmfc_lib_dir "${_v143_msvc_root}/atlmfc/lib/x64")
    if(EXISTS "${_v143_msvc_lib_dir}")
        link_directories("${_v143_msvc_lib_dir}")
    endif()
    if(EXISTS "${_v143_msvc_atlmfc_lib_dir}")
        link_directories("${_v143_msvc_atlmfc_lib_dir}")
    endif()

    # ATL fallback: the active MSVC toolset may not have atlmfc installed
    # (common with VS Insiders/Preview builds). Search all known VS
    # installations for atlbase.h and use the first one found.
    set(_v143_atl_include_dir "${_v143_msvc_root}/atlmfc/include")
    set(_v143_atl_lib_dir     "${_v143_msvc_root}/atlmfc/lib/x64")
    if(NOT EXISTS "${_v143_atl_include_dir}/atlbase.h")
        set(_v143_atl_include_dir "")
        set(_v143_atl_lib_dir     "")
        foreach(_atl_vs_major IN ITEMS 18 2022 17 16)
        foreach(_atl_ed IN ITEMS Community Enterprise Professional Preview BuildTools Insiders)
                file(GLOB _atl_header_cands
                    "C:/Program Files/Microsoft Visual Studio/${_atl_vs_major}/${_atl_ed}/VC/Tools/MSVC/14.*/atlmfc/include/atlbase.h"
                    "C:/Program Files (x86)/Microsoft Visual Studio/${_atl_vs_major}/${_atl_ed}/VC/Tools/MSVC/14.*/atlmfc/include/atlbase.h")
                if(_atl_header_cands)
                    list(SORT _atl_header_cands ORDER DESCENDING)
                    list(GET _atl_header_cands 0 _atl_found_h)
                    get_filename_component(_v143_atl_include_dir "${_atl_found_h}" DIRECTORY)
                    # Derive the matching lib dir (prefer x64)
                    get_filename_component(_atl_msvc_ver_root "${_v143_atl_include_dir}" DIRECTORY)
                    set(_v143_atl_lib_dir "${_atl_msvc_ver_root}/lib/x64")
                    if(NOT EXISTS "${_v143_atl_lib_dir}")
                        set(_v143_atl_lib_dir "")
                    endif()
                    break()
                endif()
            endforeach()
            if(_v143_atl_include_dir)
                break()
            endif()
        endforeach()
        if(_v143_atl_include_dir)
            message(STATUS "ATL not in active MSVC toolset; using fallback: ${_v143_atl_include_dir}")
        else()
            message(WARNING "atlbase.h not found in any VS installation. D3D12/D3D11 backend in DiligentEngine will be disabled.")
        endif()
    endif()

    # Make the compiler include and library search paths explicit for build
    # steps launched by Ninja, which may not inherit the IDE environment.
    file(GLOB _v143_windows_sdk_include_versions LIST_DIRECTORIES TRUE
        "C:/Program Files (x86)/Windows Kits/10/Include/10.*")
    if(_v143_windows_sdk_include_versions)
        list(SORT _v143_windows_sdk_include_versions ORDER DESCENDING)
        list(GET _v143_windows_sdk_include_versions 0 _v143_windows_sdk_include_root)
        get_filename_component(_v143_windows_sdk_root "${_v143_windows_sdk_include_root}" DIRECTORY)
        get_filename_component(_v143_windows_sdk_version "${_v143_windows_sdk_include_root}" NAME)

        set(_v143_windows_sdk_ucrt_include "${_v143_windows_sdk_root}/Include/${_v143_windows_sdk_version}/ucrt")
        set(_v143_windows_sdk_shared_include "${_v143_windows_sdk_root}/Include/${_v143_windows_sdk_version}/shared")
        set(_v143_windows_sdk_um_include "${_v143_windows_sdk_root}/Include/${_v143_windows_sdk_version}/um")
        set(_v143_windows_sdk_winrt_include "${_v143_windows_sdk_root}/Include/${_v143_windows_sdk_version}/winrt")
        set(_v143_windows_sdk_ucrt_lib "${_v143_windows_sdk_root}/Lib/${_v143_windows_sdk_version}/ucrt/x64")
        set(_v143_windows_sdk_um_lib "${_v143_windows_sdk_root}/Lib/${_v143_windows_sdk_version}/um/x64")
        set(_v143_windows_sdk_bin "${_v143_windows_sdk_root}/bin/${_v143_windows_sdk_version}/x64")

        if(EXISTS "${_v143_msvc_root}/include" AND EXISTS "${_v143_windows_sdk_ucrt_include}")
            # Build INCLUDE env and compiler flags. Add ATL only when found.
            set(_v143_env_include
                "${_v143_msvc_root}/include;${_v143_windows_sdk_ucrt_include};${_v143_windows_sdk_shared_include};${_v143_windows_sdk_um_include};${_v143_windows_sdk_winrt_include}")
            set(_v143_flag_include
                "/I\"${_v143_msvc_root}/include\" /I\"${_v143_windows_sdk_ucrt_include}\" /I\"${_v143_windows_sdk_shared_include}\" /I\"${_v143_windows_sdk_um_include}\" /I\"${_v143_windows_sdk_winrt_include}\"")
            if(_v143_atl_include_dir)
                set(_v143_env_include  "${_v143_atl_include_dir};${_v143_env_include}")
                set(_v143_flag_include "/I\"${_v143_atl_include_dir}\" ${_v143_flag_include}")
                if(_v143_atl_lib_dir AND EXISTS "${_v143_atl_lib_dir}")
                    link_directories("${_v143_atl_lib_dir}")
                endif()
            endif()

            set(ENV{INCLUDE} "${_v143_env_include};$ENV{INCLUDE}")
            set(CMAKE_C_FLAGS_INIT   "${CMAKE_C_FLAGS_INIT}   ${_v143_flag_include}")
            set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} ${_v143_flag_include}")
        endif()
        if(EXISTS "${_v143_msvc_lib_dir}" AND EXISTS "${_v143_windows_sdk_ucrt_lib}" AND EXISTS "${_v143_windows_sdk_um_lib}")
            set(_v143_env_lib
                "${_v143_msvc_lib_dir};${_v143_windows_sdk_ucrt_lib};${_v143_windows_sdk_um_lib}")
            set(_v143_link_flags
                "/LIBPATH:\"${_v143_msvc_lib_dir}\" /LIBPATH:\"${_v143_windows_sdk_ucrt_lib}\" /LIBPATH:\"${_v143_windows_sdk_um_lib}\"")
            if(_v143_atl_lib_dir AND EXISTS "${_v143_atl_lib_dir}")
                set(_v143_env_lib    "${_v143_atl_lib_dir};${_v143_env_lib}")
                set(_v143_link_flags "/LIBPATH:\"${_v143_atl_lib_dir}\" ${_v143_link_flags}")
            endif()

            set(ENV{LIB} "${_v143_env_lib};$ENV{LIB}")
            set(CMAKE_EXE_LINKER_FLAGS_INIT    "${CMAKE_EXE_LINKER_FLAGS_INIT}    ${_v143_link_flags}")
            set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} ${_v143_link_flags}")
            set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} ${_v143_link_flags}")
        endif()

        if(EXISTS "${_v143_windows_sdk_bin}")
            set(ENV{PATH} "${_v143_windows_sdk_bin};$ENV{PATH}")
            if(EXISTS "${_v143_windows_sdk_bin}/rc.exe")
                set(CMAKE_RC_COMPILER "${_v143_windows_sdk_bin}/rc.exe" CACHE FILEPATH "Resource compiler" FORCE)
            endif()
            if(EXISTS "${_v143_windows_sdk_bin}/mt.exe")
                set(CMAKE_MT "${_v143_windows_sdk_bin}/mt.exe" CACHE FILEPATH "Manifest tool" FORCE)
            endif()
        endif()
    endif()
endif()

set(_v143_vcpkg "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${_v143_vcpkg}" AND DEFINED ENV{VCPKG_ROOT})
    set(_v143_vcpkg "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()
if(EXISTS "${_v143_vcpkg}")
    include("${_v143_vcpkg}")
endif()
