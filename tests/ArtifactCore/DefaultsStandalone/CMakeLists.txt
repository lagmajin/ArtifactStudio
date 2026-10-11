cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDefaultsStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(DefaultsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/Defaults.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreDefaultsContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DefaultsContractTest.cpp"
    LINK_LIBS DefaultsModuleInterfaces
    LABELS "core;defaults;cpu;contract;standalone"
)

enable_testing()
