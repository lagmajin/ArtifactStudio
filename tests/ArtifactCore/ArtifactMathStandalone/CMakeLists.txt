cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactMathStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactMathModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactMath.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactMathContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactMathContractTest.cpp"
    LINK_LIBS ArtifactMathModuleInterface
    LABELS "core;math;numeric;cpu;contract;standalone"
)

enable_testing()
