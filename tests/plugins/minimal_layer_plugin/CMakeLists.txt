cmake_minimum_required(VERSION 3.30)
project(MinimalLayerPlugin)

add_library(MinimalLayerPlugin SHARED MinimalLayerPlugin.cpp)

target_include_directories(MinimalLayerPlugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../ArtifactCore/include/Plugin"
)

target_compile_definitions(MinimalLayerPlugin PRIVATE ARTIFACT_PLUGIN_DLL_EXPORT)
set_target_properties(MinimalLayerPlugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
