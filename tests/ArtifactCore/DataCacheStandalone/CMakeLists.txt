cmake_minimum_required(VERSION 3.28)
project(ArtifactCoreDataCacheStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
include(../StandaloneTestHelpers.cmake)

set(artifact_core_root "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(artifact_core_include "${artifact_core_root}/include")

artifact_add_standalone_module_interface(DataCacheModuleInterfaces
    BASE_DIRS "${artifact_core_root}"
    INCLUDE_DIRS "${artifact_core_include}"
    MODULES
        "${artifact_core_include}/Container/ContainerDebug.ixx"
        "${artifact_core_include}/Container/ContainerDebugRegistry.ixx"
        "${artifact_core_root}/src/Memory/SharedPtr.cppm"
        "${artifact_core_root}/src/Core/ArtifactOptional.cppm"
        "${artifact_core_include}/Core/ArtifactArray.ixx"
        "${artifact_core_include}/Core/ArtifactString.ixx"
        "${artifact_core_include}/Container/NamedVector.ixx"
        "${artifact_core_include}/Data/ColumnType.ixx"
        "${artifact_core_include}/Data/CsvParser.ixx"
        "${artifact_core_include}/Data/DataTable.ixx"
        "${artifact_core_include}/Data/DataSource.ixx"
        "${artifact_core_include}/Data/CsvDataSource.ixx"
        "${artifact_core_include}/Data/DataCache.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreDataCacheStandaloneTest
    SOURCES ../DataCacheStandaloneTest.cpp
    LINK_LIBS DataCacheModuleInterfaces
    LABELS "core;data;data-cache;cpu;contract;standalone"
)

enable_testing()
