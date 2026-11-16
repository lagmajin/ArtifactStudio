cmake_minimum_required(VERSION 3.30)

if(NOT TARGET ArtifactAppRuntime)
    message(FATAL_ERROR
        "ArtifactTimelineClipBarPlayground must be configured from the ArtifactStudio root project")
endif()

add_executable(ArtifactTimelineClipBarPlayground EXCLUDE_FROM_ALL main.cpp)
set_target_properties(ArtifactTimelineClipBarPlayground PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
target_link_libraries(ArtifactTimelineClipBarPlayground PRIVATE ArtifactAppRuntime)

if(MSVC)
    target_compile_options(ArtifactTimelineClipBarPlayground PRIVATE
        /Zc:__cplusplus /utf-8 /permissive- /wd5050)
endif()
