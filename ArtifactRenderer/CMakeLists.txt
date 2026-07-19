cmake_minimum_required(VERSION 3.30)
project(ArtifactRenderer)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Svg)

add_executable(ArtifactRenderer
    src/ExternalFrameRenderer.cpp
    src/ExternalRenderJobSchema.cpp
    src/main.cpp
)

set_target_properties(ArtifactRenderer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_SCAN_FOR_MODULES OFF
    OUTPUT_NAME "artifact-renderer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_include_directories(ArtifactRenderer
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ArtifactRenderer PRIVATE Qt6::Core Qt6::Gui Qt6::Svg)

add_custom_command(TARGET ArtifactRenderer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/artifact_blender_cycles.py"
        "$<TARGET_FILE_DIR:ArtifactRenderer>/artifact_blender_cycles.py"
)

if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(ArtifactRenderer PRIVATE /Zc:__cplusplus /utf-8 /WX-)
    target_compile_definitions(ArtifactRenderer PRIVATE _WIN32_WINNT=0x0A00)
endif()
