cmake_minimum_required(VERSION 3.30)

project(ArtifactCorePosition2DStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(Position2DModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/Position2D.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(Position2DUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/Position2D.cppm"
    LINK_LIBS Position2DModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(Position2DUnderTest Position2DModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCorePosition2DContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Position2DContractTest.cpp"
    LINK_LIBS Position2DUnderTest Position2DModuleInterfaces Qt6::Core
    LABELS "core;geometry;position;cpu;contract;standalone"
)

enable_testing()
