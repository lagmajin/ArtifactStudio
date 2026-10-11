cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreRotationTurnsStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(RotationTurnsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Geometry/RotationTurns.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(RotationTurnsUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Math/RotationTurns.cppm"
    LINK_LIBS RotationTurnsModuleInterfaces
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(RotationTurnsUnderTest RotationTurnsModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreRotationTurnsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../RotationTurnsStandaloneTest.cpp"
    LINK_LIBS RotationTurnsUnderTest RotationTurnsModuleInterfaces
    LABELS "core;geometry;rotation;cpu;contract;standalone"
)

enable_testing()
