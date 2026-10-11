cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactRandomStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactRandomModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactRandom.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreArtifactRandomContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactRandomContractTest.cpp"
    LINK_LIBS ArtifactRandomModuleInterface
    LABELS "core;random;numeric;cpu;contract;standalone"
)

enable_testing()
