cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactMinMaxStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactMinMaxModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Math/ArtifactMinMax.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactMinMaxStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactMinMaxStandaloneTest.cpp"
    LINK_LIBS ArtifactMinMaxModuleInterfaces
    LABELS "core;math;minmax;numeric;cpu;contract;standalone"
)

enable_testing()
