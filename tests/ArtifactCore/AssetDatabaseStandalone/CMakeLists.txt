cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAssetDatabaseStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AssetDatabaseModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Asset/AssetType.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Asset/AssetDatabase.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AssetDatabaseUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Asset/AssetDatabase.cppm"
    LINK_LIBS AssetDatabaseModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AssetDatabaseUnderTest AssetDatabaseModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAssetDatabaseContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AssetDatabaseContractTest.cpp"
    LINK_LIBS AssetDatabaseUnderTest AssetDatabaseModuleInterfaces Qt6::Core
    LABELS "asset;database;cpu;contract;standalone"
)

enable_testing()
