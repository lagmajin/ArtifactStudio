cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreOptionalStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(OptionalModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Result.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Optional.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreOptionalContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../OptionalContractTest.cpp"
    LINK_LIBS OptionalModuleInterfaces
    LABELS "core;optional;cpu;contract;standalone"
)

enable_testing()
