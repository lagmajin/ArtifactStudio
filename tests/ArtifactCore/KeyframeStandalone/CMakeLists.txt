cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreKeyframeStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(KeyframeModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Geometry/Interpolate.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreKeyframeContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../KeyframeContractTest.cpp"
    LINK_LIBS KeyframeModuleInterfaces
    LABELS "animation;keyframe;cpu;contract;standalone"
)

enable_testing()
