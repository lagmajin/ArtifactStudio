cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreEasingStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(EasingModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Geometry/Interpolate.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Animation/EasingCurveUtil.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreEasingContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../EasingContractTest.cpp"
    LINK_LIBS EasingModuleInterfaces Qt6::Core
    LABELS "animation;easing;cpu;contract;standalone"
)

enable_testing()
