cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactFunctionStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactFunctionModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactFunction.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactFunctionContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactFunctionContractTest.cpp"
    LINK_LIBS ArtifactFunctionModuleInterfaces
    LABELS "core;function;cpu;contract;standalone"
)

enable_testing()
