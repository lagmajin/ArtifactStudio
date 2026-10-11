cmake_minimum_required(VERSION 3.30)

project(ArtifactCorePathUtilsStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(PathUtilsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringLike.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Path.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCorePathUtilsContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../PathUtilsContractTest.cpp"
    LINK_LIBS PathUtilsModuleInterfaces Qt6::Core
    LABELS "core;path;filesystem;cpu;contract;standalone"
)

enable_testing()
