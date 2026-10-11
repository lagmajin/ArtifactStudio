cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAssetFingerprintStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AssetFingerprintModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Utils/AssetFingerprint.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AssetFingerprintUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/AssetFingerprint.cppm"
    LINK_LIBS AssetFingerprintModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AssetFingerprintUnderTest AssetFingerprintModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAssetFingerprintContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AssetFingerprintContractTest.cpp"
    LINK_LIBS AssetFingerprintUnderTest AssetFingerprintModuleInterfaces Qt6::Core
    LABELS "asset;fingerprint;cpu;contract;standalone"
)

enable_testing()
