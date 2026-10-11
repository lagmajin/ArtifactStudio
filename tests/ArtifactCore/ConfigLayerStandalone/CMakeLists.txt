cmake_minimum_required(VERSION 3.30)
project(ArtifactCoreConfigLayerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ConfigLayerModuleInterfaces
    BASE_DIRS "${artifact_core_root}/include"
    MODULES "${artifact_core_root}/include/Configuration/ConfigLayer.ixx"
    INCLUDE_DIRS "${artifact_core_root}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreConfigLayerStandaloneTest
    SOURCES ../ConfigLayerStandaloneTest.cpp
    LINK_LIBS ConfigLayerModuleInterfaces Qt6::Core
    LABELS "core;configuration;config-layer;cpu;contract;standalone"
)

enable_testing()
