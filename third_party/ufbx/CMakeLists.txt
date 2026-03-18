# ufbx: Lightweight FBX importer
# https://github.com/ufbx/ufbx

add_library(ufbx STATIC ufbx.c ufbx.h)

target_include_directories(ufbx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# ufbx is C code, ensure it's compiled correctly in C++ projects
set_target_properties(ufbx PROPERTIES LINKER_LANGUAGE CXX)

if(MSVC)
    target_compile_options(ufbx PRIVATE /W3)
else()
    target_compile_options(ufbx PRIVATE -W3)
endif()
