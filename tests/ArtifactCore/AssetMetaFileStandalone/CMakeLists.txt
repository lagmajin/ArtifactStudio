cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAssetMetaFileStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AssetMetaFileModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Asset/AssetType.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Asset/AssetMetaFile.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AssetMetaFileUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Asset/AssetMetaFile.cppm"
    LINK_LIBS AssetMetaFileModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AssetMetaFileUnderTest AssetMetaFileModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAssetMetaFileContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AssetMetaFileContractTest.cpp"
    LINK_LIBS AssetMetaFileUnderTest AssetMetaFileModuleInterfaces Qt6::Core
    LABELS "asset;metadata;cpu;contract;standalone"
)

enable_testing()
