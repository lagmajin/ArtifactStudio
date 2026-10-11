cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreColumnTypeStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ColumnTypeModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/ColumnType.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreColumnTypeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ColumnTypeStandaloneTest.cpp"
    LINK_LIBS ColumnTypeModuleInterfaces
    LABELS "core;data;column-type;cpu;contract;standalone"
)

enable_testing()
