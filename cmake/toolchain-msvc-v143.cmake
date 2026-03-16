# cmake/toolchain-msvc-v143.cmake
# Compiler toolchain: forces MSVC v143 (Visual Studio 2022)
#
# Uses CACHE FORCE to override any compiler injected by the IDE (e.g. VS 18
# injecting v145 via -DCMAKE_CXX_COMPILER). Relies on the VS developer
# environment (inheritEnvironments) for LIB, INCLUDE, PATH, rc.exe, mt.exe.
# Loads vcpkg.cmake for package management.

# Ensure vcpkg uses the correct triplet.
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")
endif()

# ---------------------------------------------------------------------------
# Locate VS 2022 (v143, 14.x toolset) cl.exe
# ---------------------------------------------------------------------------
if(WIN32)
    set(_v143_found_cl "")

    # Search standard VS 2022 paths
    foreach(_ed IN ITEMS Enterprise Professional Community Preview)
        file(GLOB _v143_cands
            "C:/Program Files/Microsoft Visual Studio/2022/${_ed}/VC/Tools/MSVC/14.*/bin/HostX64/x64/cl.exe")
        if(_v143_cands)
            list(SORT _v143_cands ORDER DESCENDING)
            list(GET _v143_cands 0 _v143_found_cl)
            break()
        endif()
    endforeach()

    # Fallback: VS 2022 Build Tools
    if(NOT _v143_found_cl)
        file(GLOB _v143_cands
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.*/bin/HostX64/x64/cl.exe")
        if(_v143_cands)
            list(SORT _v143_cands ORDER DESCENDING)
            list(GET _v143_cands 0 _v143_found_cl)
        endif()
    endif()

    if(_v143_found_cl)
        # FORCE is required to override the compiler injected by Visual Studio
        # via inheritEnvironments (e.g. -DCMAKE_CXX_COMPILER=v145 on cmdline).
        set(CMAKE_C_COMPILER   "${_v143_found_cl}" CACHE STRING "C compiler" FORCE)
        set(CMAKE_CXX_COMPILER "${_v143_found_cl}" CACHE STRING "C++ compiler" FORCE)
        message(STATUS "[toolchain-msvc-v143] Selected compiler: ${_v143_found_cl}")

        # Derive v143 MSVC root: compiler is at <root>/bin/HostX64/x64/cl.exe
        get_filename_component(_v143_x64   "${_v143_found_cl}" DIRECTORY)
        get_filename_component(_v143_host  "${_v143_x64}"      DIRECTORY)
        get_filename_component(_v143_bin   "${_v143_host}"     DIRECTORY)
        get_filename_component(_v143_root  "${_v143_bin}"      DIRECTORY)

        # -------------------------------------------------------------------
        # Override v145 standard library / ATL headers with v143
        # -------------------------------------------------------------------
        # VS 18 dev env (architecture:external) sets ENV{INCLUDE} and ENV{LIB}
        # pointing to v145 (14.50.x) headers and libraries.  Since we forced
        # v143 cl.exe, we must ensure that v143 headers are found FIRST so
        # that `import std;` compiles std.ixx against matching headers.
        set(_v143_stl_include "${_v143_root}/include")
        set(_v143_atl_include "${_v143_root}/atlmfc/include")

        # include_directories(BEFORE ...) generates -I flags placed before
        # any other includes in every Ninja compile command, including the
        # compilation of std.ixx into its BMI.
        if(EXISTS "${_v143_stl_include}")
            include_directories(BEFORE "${_v143_stl_include}")
            message(STATUS "[toolchain-msvc-v143] STL includes: ${_v143_stl_include}")
        else()
            message(WARNING "[toolchain-msvc-v143] STL headers not found at ${_v143_stl_include}")
        endif()

        if(EXISTS "${_v143_atl_include}")
            include_directories(BEFORE "${_v143_atl_include}")
            message(STATUS "[toolchain-msvc-v143] ATL/MFC includes: ${_v143_atl_include}")
        else()
            message(WARNING
                "[toolchain-msvc-v143] ATL/MFC headers not found at ${_v143_atl_include}. "
                "Run the VS 2022 installer and add "
                "'C++ ATL for latest v143 build tools (x86 & x64)'.")
        endif()

        # ENV{INCLUDE}: prepend v143 paths so try_compile and cl.exe's
        # fallback header search also find v143 headers before v145.
        set(_v143_inc_prepend "")
        if(EXISTS "${_v143_stl_include}")
            list(APPEND _v143_inc_prepend "${_v143_stl_include}")
        endif()
        if(EXISTS "${_v143_atl_include}")
            list(APPEND _v143_inc_prepend "${_v143_atl_include}")
        endif()
        if(_v143_inc_prepend)
            list(JOIN _v143_inc_prepend ";" _v143_inc_str)
            set(ENV{INCLUDE} "${_v143_inc_str};$ENV{INCLUDE}")
        endif()

        # ENV{LIB}: prepend v143 library paths to prevent linking against
        # v145 CRT / standard library / ATL libs.
        set(_v143_lib_x64     "${_v143_root}/lib/x64")
        set(_v143_atl_lib_x64 "${_v143_root}/atlmfc/lib/x64")
        set(_v143_lib_prepend "")
        if(EXISTS "${_v143_lib_x64}")
            list(APPEND _v143_lib_prepend "${_v143_lib_x64}")
        endif()
        if(EXISTS "${_v143_atl_lib_x64}")
            list(APPEND _v143_lib_prepend "${_v143_atl_lib_x64}")
        endif()
        if(_v143_lib_prepend)
            list(JOIN _v143_lib_prepend ";" _v143_lib_str)
            set(ENV{LIB} "${_v143_lib_str};$ENV{LIB}")
            message(STATUS "[toolchain-msvc-v143] LIB paths prepended: ${_v143_lib_str}")
        endif()

        # link_directories() emits /LIBPATH: flags into Ninja link commands so the
        # linker subprocess (which does NOT inherit ENV{LIB} set above) can resolve
        # v143 runtime and ATL libraries such as atls.lib.
        if(EXISTS "${_v143_lib_x64}")
            link_directories("${_v143_lib_x64}")
        endif()
        if(EXISTS "${_v143_atl_lib_x64}")
            link_directories("${_v143_atl_lib_x64}")
        endif()

        # Log std.ixx path (actual metadata is set post-project in CMakeLists.txt)
        set(_v143_std_ixx "${_v143_root}/modules/std.ixx")
        if(EXISTS "${_v143_std_ixx}")
            message(STATUS "[toolchain-msvc-v143] std.ixx path: ${_v143_std_ixx}")
        endif()
    else()
        message(WARNING
            "[toolchain-msvc-v143] VS 2022 (v143, 14.x) was not found. "
            "Falling back to cl.exe from PATH. Please install Visual Studio 2022.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Load vcpkg package manager
# ---------------------------------------------------------------------------
set(_v143_vcpkg "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${_v143_vcpkg}" AND DEFINED ENV{VCPKG_ROOT})
    set(_v143_vcpkg "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()
if(EXISTS "${_v143_vcpkg}")
    include("${_v143_vcpkg}")
else()
    message(WARNING "[toolchain-msvc-v143] vcpkg.cmake not found at ${_v143_vcpkg}")
endif()
