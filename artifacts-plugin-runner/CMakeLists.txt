cmake_minimum_required(VERSION 3.30)
project(ArtifactPluginRunner)

add_executable(artifacts-plugin-runner WIN32
    src/main.cpp
)

set_target_properties(artifacts-plugin-runner PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(artifacts-plugin-runner PRIVATE Qt6::Core)

target_compile_definitions(artifacts-plugin-runner PRIVATE
    PLATFORM_WIN32
    _GUARDOVERFLOW_CRT_ALLOCATORS=1
)
