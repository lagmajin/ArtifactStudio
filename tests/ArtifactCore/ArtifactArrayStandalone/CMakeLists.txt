cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactArrayStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactArrayModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactArrayContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactArrayContractTest.cpp"
    LINK_LIBS ArtifactArrayModuleInterfaces
    LABELS "core;array;container;cpu;contract;standalone"
)

enable_testing()
