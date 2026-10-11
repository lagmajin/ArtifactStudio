cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreCppLinqStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(CppLinqModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Result.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Optional.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/CppLinq.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreCppLinqContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CppLinqContractTest.cpp"
    LINK_LIBS CppLinqModuleInterfaces
    LABELS "core;linq;ranges;cpu;contract;standalone"
)

enable_testing()
