cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactVariantStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactVariantModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactVariant.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactVariantContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactVariantContractTest.cpp"
    LINK_LIBS ArtifactVariantModuleInterface
    LABELS "core;variant;value-type;cpu;contract;standalone"
)

enable_testing()
