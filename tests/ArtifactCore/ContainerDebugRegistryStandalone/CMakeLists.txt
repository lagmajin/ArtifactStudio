cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreContainerDebugRegistryStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ContainerDebugRegistryModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreContainerDebugRegistryStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ContainerDebugRegistryStandaloneTest.cpp"
    LINK_LIBS ContainerDebugRegistryModuleInterfaces
    LABELS "container;debug-registry;lifecycle;contract;standalone"
)

enable_testing()
