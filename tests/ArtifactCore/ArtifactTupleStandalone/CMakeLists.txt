cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactTupleStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactTupleModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactUtility.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactTuple.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreArtifactTupleContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactTupleContractTest.cpp"
    LINK_LIBS ArtifactTupleModuleInterfaces
    LABELS "core;tuple;utility;cpu;contract;standalone"
)

enable_testing()
