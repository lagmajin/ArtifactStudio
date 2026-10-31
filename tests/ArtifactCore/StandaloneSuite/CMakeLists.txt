cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreStandaloneSuite LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")
artifact_ensure_shared_string_modules()

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")

artifact_add_standalone_module_interface(ArtifactCoreStandaloneFoundationModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Memory/TrackedPtr.cppm"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

artifact_add_standalone_module_interface(ArtifactCoreAssetModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    MODULES "${ARTIFACT_CORE_INCLUDE}/Asset/AssetType.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS Qt6::Core
)

enable_testing()

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreCoreUtilityFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../CoreUtilityStandalone" core-utility)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreCoreValueFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../CoreValueStandalone" core-values)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreColorFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../ColorSpaceStandalone" color-space)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreFrameRateFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../FrameRateStandaloneSuite" frame-rate)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreGeometryFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../GeometryStandaloneSuite" geometry)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreRawImageFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../RawImageStandalone" raw-image)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreDataFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../DataStandalone" data)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreAudioFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../AudioStandalone" audio)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreAssetDatabaseFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../AssetDatabaseStandalone" asset-database)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreAssetMetaFileFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../AssetMetaFileStandalone" asset-meta-file)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreAssetFingerprintFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../AssetFingerprintStandalone" asset-fingerprint)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreDiagnosticFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../DiagnosticEngineStandalone" diagnostic-engine)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)

set(ARTIFACT_STANDALONE_GTEST_GROUP ArtifactCoreOpenExrFeatureTests)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../OpenExrStandalone" open-exr)
unset(ARTIFACT_STANDALONE_GTEST_GROUP)
