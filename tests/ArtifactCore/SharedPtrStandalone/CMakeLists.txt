cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreSharedPtrStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(SharedPtrModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreSharedPtrContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SharedPtrContractTest.cpp"
    LINK_LIBS SharedPtrModuleInterfaces
    LABELS "core;memory;shared-ptr;contract;standalone"
)

enable_testing()
