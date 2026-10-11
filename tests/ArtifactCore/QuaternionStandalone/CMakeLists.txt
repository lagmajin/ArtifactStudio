cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreQuaternionStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(QuaternionModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Math/Quaternion.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreQuaternionStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../QuaternionStandaloneTest.cpp"
    LINK_LIBS QuaternionModuleInterfaces
    LABELS "core;math;quaternion;cpu;contract;standalone"
)

enable_testing()
