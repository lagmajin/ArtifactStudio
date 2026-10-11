cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreTypedColumnStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(TypedColumnModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/ColumnType.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/TypeInference.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/TypedColumn.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreTypedColumnStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../TypedColumnStandaloneTest.cpp"
    LINK_LIBS TypedColumnModuleInterfaces
    LABELS "core;data;typed-column;cpu;contract;standalone"
)

enable_testing()
