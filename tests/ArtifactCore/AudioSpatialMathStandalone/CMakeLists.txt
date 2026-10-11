cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioSpatialMathStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioSpatialMathModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialParams.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialMath.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreAudioSpatialMathStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSpatialMathStandaloneTest.cpp"
    LINK_LIBS AudioSpatialMathModuleInterfaces Qt6::Core
    LABELS "core;audio;spatial;math;cpu;contract;standalone"
)

enable_testing()
