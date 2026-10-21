# CMake's Windows-Clang platform file selects GNU/MinGW link rules for the
# upstream Clang frontend. This language override replaces the final COFF link
# command without changing compiler detection or Clang's P1689 module rules.
# CMake has completed Clang frontend and P1689 scanner setup before loading
# this override. Correct the MinGW defaults to the MSVC ABI libraries and use
# Clang's native COFF linker selection.
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT
    "-lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 -loldnames")
set(CMAKE_CXX_USING_LINKER_SYSTEM "-fuse-ld=link")
set(CMAKE_CXX_USING_LINKER_DEFAULT "-fuse-ld=lld-link")
set(CMAKE_CXX_CREATE_WIN32_EXE "-Xlinker /subsystem:windows")
set(CMAKE_CXX_CREATE_CONSOLE_EXE "-Xlinker /subsystem:console")
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_CREATE_SHARED_LIBRARY
    "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> -o <TARGET> -Xlinker /implib:<TARGET_IMPLIB> <OBJECTS> <LINK_LIBRARIES> <MANIFESTS>")
set(CMAKE_CXX_CREATE_SHARED_MODULE "${CMAKE_CXX_CREATE_SHARED_LIBRARY}")
