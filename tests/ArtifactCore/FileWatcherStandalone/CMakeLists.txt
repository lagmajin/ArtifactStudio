cmake_minimum_required(VERSION 3.28)
project(ArtifactCoreFileWatcherStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(artifact_core_include "${artifact_core_root}/include")

artifact_add_standalone_module_interface(FileWatcherModuleInterfaces
    BASE_DIRS "${artifact_core_root}"
    INCLUDE_DIRS "${artifact_core_include}"
    MODULES
        "${artifact_core_include}/Container/ContainerDebug.ixx"
        "${artifact_core_include}/Container/ContainerDebugRegistry.ixx"
        "${artifact_core_root}/src/Core/ArtifactOptional.cppm"
        "${artifact_core_include}/Core/ArtifactArray.ixx"
        "${artifact_core_include}/Core/ArtifactString.ixx"
        "${artifact_core_include}/Container/NamedVector.ixx"
        "${artifact_core_include}/Data/FileWatcher.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreFileWatcherStandaloneTest
    SOURCES ../FileWatcherStandaloneTest.cpp
    LINK_LIBS FileWatcherModuleInterfaces
    LABELS "core;data;file-watcher;cpu;contract;standalone"
)

enable_testing()
