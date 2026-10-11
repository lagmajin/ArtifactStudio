cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreArtifactThreadStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(ArtifactThreadModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactThread.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Threads::Threads
)

artifact_add_standalone_gtest(ArtifactCoreArtifactThreadContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ArtifactThreadContractTest.cpp"
    LINK_LIBS ArtifactThreadModuleInterfaces Threads::Threads
    LABELS "core;thread;concurrency;cpu;contract;standalone"
)

enable_testing()
