cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioSpatialRendererStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioSpatialRendererModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioPanner.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialParams.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialMath.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpeakerLayout.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialRenderer.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioSpatialRendererUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPanner.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Audio/Spatial/SpeakerLayout.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Audio/Spatial/SpatialRenderer.cppm"
    LINK_LIBS AudioSpatialRendererModuleInterfaces Qt6::Core
)

if(MSVC)
    add_dependencies(AudioSpatialRendererUnderTest AudioSpatialRendererModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioSpatialRendererStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSpatialRendererStandaloneTest.cpp"
    LINK_LIBS AudioSpatialRendererUnderTest AudioSpatialRendererModuleInterfaces Qt6::Core
    LABELS "core;audio;spatial;renderer;cpu;contract;standalone"
)

enable_testing()
