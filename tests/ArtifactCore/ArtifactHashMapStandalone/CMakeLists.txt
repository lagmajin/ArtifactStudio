cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactHashMapStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactHashMapModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactHashMap.cppm"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactHashMapContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactHashMapContractTest.cpp"
    LINK_LIBS ArtifactHashMapModuleInterfaces
    LABELS "core;hash-map;container;cpu;contract;standalone"
)

enable_testing()
