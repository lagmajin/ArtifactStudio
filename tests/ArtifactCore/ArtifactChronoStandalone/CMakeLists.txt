cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactChronoStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactChronoModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactChrono.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactChronoContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactChronoContractTest.cpp"
    LINK_LIBS ArtifactChronoModuleInterface
    LABELS "core;chrono;duration;stopwatch;cpu;contract;standalone"
)

enable_testing()
