set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")

# Find cl.exe for MSVC v143
set(_v143_found_cl "")
foreach(_ed IN ITEMS Enterprise Professional Community Preview)
    file(GLOB _v143_cands
        "C:/Program Files/Microsoft Visual Studio/2022/${_ed}/VC/Tools/MSVC/14.*/bin/Hostx64/x64/cl.exe"
        "C:/Program Files/Microsoft Visual Studio/2022/${_ed}/VC/Tools/MSVC/14.*/bin/HostX64/x64/cl.exe")
    if(_v143_cands)
        list(SORT _v143_cands ORDER DESCENDING)
        list(GET _v143_cands 0 _v143_found_cl)
        break()
    endif()
endforeach()

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
endif()

set(_v143_vcpkg "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${_v143_vcpkg}" AND DEFINED ENV{VCPKG_ROOT})
    set(_v143_vcpkg "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()
if(EXISTS "${_v143_vcpkg}")
    include("${_v143_vcpkg}")
endif()