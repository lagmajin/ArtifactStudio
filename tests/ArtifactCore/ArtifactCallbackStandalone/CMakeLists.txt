cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactCallbackStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactCallbackModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactCallback.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactCallbackContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactCallbackContractTest.cpp"
    LINK_LIBS ArtifactCallbackModuleInterface
    LABELS "core;callback;function;cpu;contract;standalone"
)

enable_testing()
