cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactAlgorithmsStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactAlgorithmsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactUtility.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactAlgorithms.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactAlgorithmsContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactAlgorithmsContractTest.cpp"
    LINK_LIBS ArtifactAlgorithmsModuleInterfaces
    LABELS "core;algorithms;container;cpu;contract;standalone"
)

enable_testing()
