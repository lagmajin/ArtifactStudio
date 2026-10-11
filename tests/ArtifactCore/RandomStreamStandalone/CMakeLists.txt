cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreRandomStreamStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(RandomStreamModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Math/Random.ixx"
)

artifact_add_standalone_gtest(ArtifactCoreRandomStreamStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../RandomStreamStandaloneTest.cpp"
    LINK_LIBS RandomStreamModuleInterfaces
    LABELS "core;math;random-stream;cpu;contract;standalone"
)

enable_testing()
