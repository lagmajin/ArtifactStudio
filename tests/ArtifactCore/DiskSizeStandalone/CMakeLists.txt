cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDiskSizeStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(DiskSizeModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Channel/DiskSize.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreDiskSizeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DiskSizeStandaloneTest.cpp"
    LINK_LIBS DiskSizeModuleInterfaces
    LABELS "core;filesystem;disk-size;cpu;contract;standalone"
)

enable_testing()
