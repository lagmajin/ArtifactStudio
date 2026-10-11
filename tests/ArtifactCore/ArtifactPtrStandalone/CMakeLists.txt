cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactPtrStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactPtrModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactUtility.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactPtr.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactPtrContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactPtrContractTest.cpp"
    LINK_LIBS ArtifactPtrModuleInterfaces
    LABELS "core;memory;artifact-ptr;contract;standalone"
)

enable_testing()
