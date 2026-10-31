cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAssetDatabaseStandalone LANGUAGES CXX)

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

artifact_add_standalone_module_interface(AssetDatabaseModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Asset/AssetDatabase.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS ArtifactCoreAssetModuleInterfaces Qt6::Core
)

artifact_add_standalone_module_implementation(AssetDatabaseUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Asset/AssetDatabase.cpp"
    LINK_LIBS AssetDatabaseModuleInterfaces ArtifactCoreAssetModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AssetDatabaseUnderTest AssetDatabaseModuleInterfaces)
    target_link_options(AssetDatabaseUnderTest PRIVATE
        "/reference AssetType=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAssetModuleInterfaces.dir/AssetType.ifc"
        "/reference Asset.Database=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/AssetDatabaseModuleInterfaces.dir/Asset.Database.ifc"
    )
endif()

artifact_add_standalone_gtest(ArtifactCoreAssetDatabaseContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AssetDatabaseContractTest.cpp"
    LINK_LIBS AssetDatabaseUnderTest AssetDatabaseModuleInterfaces ArtifactCoreAssetModuleInterfaces Qt6::Core
    LABELS "asset;database;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
