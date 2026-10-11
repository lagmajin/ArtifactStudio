cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDepthMapStandalone LANGUAGES CXX)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(DepthMapModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Image/DepthMap.ixx"
    LINK_LIBS Qt6::Core Qt6::Gui
)

artifact_add_standalone_gtest(ArtifactCoreDepthMapStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DepthMapContractTest.cpp"
    LINK_LIBS DepthMapModuleInterfaces Qt6::Core Qt6::Gui
    LABELS "core;image;depth-map;cpu;contract;standalone"
)

enable_testing()
