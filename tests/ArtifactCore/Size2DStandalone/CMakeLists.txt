cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreSize2DStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(Size2DModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Common/Size.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreSize2DStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Size2DStandaloneTest.cpp"
    LINK_LIBS Size2DModuleInterfaces Qt6::Core
    LABELS "core;size;concepts;cpu;contract;standalone"
)

enable_testing()
