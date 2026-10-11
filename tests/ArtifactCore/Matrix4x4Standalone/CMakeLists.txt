cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreMatrix4x4Standalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(Matrix4x4ModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Math/Matrix4x4.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreMatrix4x4StandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Matrix4x4StandaloneTest.cpp"
    LINK_LIBS Matrix4x4ModuleInterfaces
    LABELS "core;math;matrix;cpu;contract;standalone"
)

enable_testing()
