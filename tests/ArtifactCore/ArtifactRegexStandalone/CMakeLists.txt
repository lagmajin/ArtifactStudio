cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactRegexStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactRegexModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactUtility.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactRegex.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactRegexContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactRegexContractTest.cpp"
    LINK_LIBS ArtifactRegexModuleInterfaces
    LABELS "core;regex;parser;cpu;contract;standalone"
)

enable_testing()
