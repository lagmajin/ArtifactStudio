cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreNumericStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(NumericModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Utils/Numeric.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreNumericContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../NumericContractTest.cpp"
    LINK_LIBS NumericModuleInterfaces
    LABELS "core;numeric;cpu;contract;standalone"
)

enable_testing()
