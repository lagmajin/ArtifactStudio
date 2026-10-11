cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioSpatialParamsStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioSpatialParamsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialParams.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreAudioSpatialParamsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSpatialParamsStandaloneTest.cpp"
    LINK_LIBS AudioSpatialParamsModuleInterfaces Qt6::Core
    LABELS "core;audio;spatial;params;cpu;contract;standalone"
)

enable_testing()
