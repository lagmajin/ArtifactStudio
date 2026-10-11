cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFloat3Standalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(Float3ModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Geometry/Float3.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreFloat3StandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Float3StandaloneTest.cpp"
    LINK_LIBS Float3ModuleInterfaces
    LABELS "core;geometry;float3;math;cpu;contract;standalone"
)

enable_testing()
