cmake_minimum_required(VERSION 3.30)
project(ArtifactPluginRunner)

add_executable(artifacts-plugin-runner WIN32
    src/main.cpp
)

set_target_properties(artifacts-plugin-runner PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    # PluginLoader looks for ArtifactPluginRunner.exe next to the app binary.
    # Without this the target's own name (artifacts-plugin-runner) is used and
    # the subprocess loader can never find the runner.
    OUTPUT_NAME "ArtifactPluginRunner"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(artifacts-plugin-runner PRIVATE Qt6::Core)

target_compile_definitions(artifacts-plugin-runner PRIVATE
    PLATFORM_WIN32
    _GUARDOVERFLOW_CRT_ALLOCATORS=1
)
