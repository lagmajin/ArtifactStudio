cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreContainerDebugStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ContainerDebugModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreContainerDebugStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ContainerDebugStandaloneTest.cpp"
    LINK_LIBS ContainerDebugModuleInterface
    LABELS "container;debug;metadata;contract;standalone"
)

enable_testing()
