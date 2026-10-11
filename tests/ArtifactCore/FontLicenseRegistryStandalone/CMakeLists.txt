cmake_minimum_required(VERSION 3.30)
project(ArtifactCoreFontLicenseRegistryStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(FontLicenseRegistryModuleInterfaces
    BASE_DIRS "${artifact_core_root}/include"
    MODULES
        "${artifact_core_root}/include/Font/FontDescriptor.ixx"
        "${artifact_core_root}/include/Font/FontLicenseRegistry.ixx"
    INCLUDE_DIRS "${artifact_core_root}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreFontLicenseRegistryStandaloneTest
    SOURCES ../FontLicenseRegistryStandaloneTest.cpp
    LINK_LIBS FontLicenseRegistryModuleInterfaces Qt6::Core
    LABELS "core;font;license-registry;cpu;contract;standalone"
)

enable_testing()
