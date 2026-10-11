cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactFunctionRefStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactFunctionRefModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactFunctionRef.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactFunctionRefContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactFunctionRefContractTest.cpp"
    LINK_LIBS ArtifactFunctionRefModuleInterface
    LABELS "core;function-ref;callable;cpu;contract;standalone"
)

enable_testing()
