cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreCoreValueStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")

if(NOT TARGET ArtifactCoreStandaloneFoundationModules)
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
        INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    )
endif()

if(TARGET CoreUtilityFeatureModules)
    set(_core_value_feature_target CoreUtilityFeatureModules)
else()
    artifact_add_standalone_module_interface(CoreValueFeatureModules
        BASE_DIRS "${ARTIFACT_CORE_ROOT}"
        MODULES
            "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactExpected.ixx"
            "${ARTIFACT_CORE_INCLUDE}/Channel/DiskSize.ixx"
        INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
        LINK_LIBS ArtifactCoreStandaloneFoundationModules
    )
    set(_core_value_feature_target CoreValueFeatureModules)
endif()

artifact_add_standalone_gtest(ArtifactCoreArtifactOptionalStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactOptionalContractTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "core;optional;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactStringStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactStringContractTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "core;string;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactArrayStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactArrayContractTest.cpp"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
    LABELS "core;array;container;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactExpectedStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactExpectedContractTest.cpp"
    LINK_LIBS ${_core_value_feature_target} ArtifactCoreStandaloneFoundationModules
    LABELS "core;expected;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDiskSizeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DiskSizeStandaloneTest.cpp"
    LINK_LIBS ${_core_value_feature_target} ArtifactCoreStandaloneFoundationModules
    LABELS "core;filesystem;disk-size;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
