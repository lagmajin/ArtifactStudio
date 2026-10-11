cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreUtilityConceptsStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(UtilityConceptsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Utils/VectorLike.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/PointLike.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringLike.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/NumericLike.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreUtilityConceptsContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../UtilityConceptsContractTest.cpp"
    LINK_LIBS UtilityConceptsModuleInterfaces Qt6::Core
    LABELS "core;concepts;cpu;contract;standalone"
)

enable_testing()
