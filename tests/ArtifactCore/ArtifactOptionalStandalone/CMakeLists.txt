cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactOptionalStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactOptionalModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactOptionalContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactOptionalContractTest.cpp"
    LINK_LIBS ArtifactOptionalModuleInterface
    LABELS "core;artifact-optional;value-type;cpu;contract;standalone"
)

enable_testing()
