cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreSpatialGridStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(SpatialGridModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Particle/Particle.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Math/SpatialGrid.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreSpatialGridStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SpatialGridStandaloneTest.cpp"
    LINK_LIBS SpatialGridModuleInterfaces
    LABELS "core;math;spatial-grid;cpu;contract;standalone"
)

enable_testing()
