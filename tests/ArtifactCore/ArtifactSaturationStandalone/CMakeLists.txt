cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactSaturationStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactSaturationModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactSaturation.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactSaturationContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactSaturationContractTest.cpp"
    LINK_LIBS ArtifactSaturationModuleInterface
    LABELS "core;saturation;numeric;cpu;contract;standalone"
)

enable_testing()
