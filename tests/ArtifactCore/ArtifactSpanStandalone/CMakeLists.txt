cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactSpanStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactSpanModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactSpan.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactSpanContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactSpanContractTest.cpp"
    LINK_LIBS ArtifactSpanModuleInterface
    LABELS "core;span;container;cpu;contract;standalone"
)

enable_testing()
