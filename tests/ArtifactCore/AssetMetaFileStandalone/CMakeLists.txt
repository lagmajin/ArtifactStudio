cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAssetMetaFileStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

if(NOT TARGET ArtifactCoreAssetModuleInterfaces)
    artifact_add_standalone_module_interface(ArtifactCoreAssetModuleInterfaces
        BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
        MODULES "${ARTIFACT_CORE_ROOT}/include/Asset/AssetType.ixx"
        INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
        LINK_LIBS Qt6::Core
    )
endif()

artifact_add_standalone_module_interface(AssetMetaFileModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Asset/AssetMetaFile.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS ArtifactCoreAssetModuleInterfaces Qt6::Core
)

artifact_add_standalone_module_implementation(AssetMetaFileUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Asset/AssetMetaFile.cpp"
    LINK_LIBS AssetMetaFileModuleInterfaces ArtifactCoreAssetModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AssetMetaFileUnderTest AssetMetaFileModuleInterfaces)
    target_link_options(AssetMetaFileUnderTest PRIVATE
        "/reference AssetType=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAssetModuleInterfaces.dir/AssetType.ifc"
        "/reference Asset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/AssetMetaFileModuleInterfaces.dir/Asset.ifc"
    )
endif()

artifact_add_standalone_gtest(ArtifactCoreAssetMetaFileContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AssetMetaFileContractTest.cpp"
    LINK_LIBS AssetMetaFileUnderTest AssetMetaFileModuleInterfaces ArtifactCoreAssetModuleInterfaces Qt6::Core
    LABELS "asset;metadata;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
