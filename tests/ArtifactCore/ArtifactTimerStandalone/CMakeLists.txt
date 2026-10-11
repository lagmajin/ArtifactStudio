cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactTimerStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactTimerModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactTimer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreArtifactTimerContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactTimerContractTest.cpp"
    LINK_LIBS ArtifactTimerModuleInterface
    LABELS "core;timer;time;cpu;contract;standalone"
)

enable_testing()
