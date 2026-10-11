cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreMultipleTagStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(MultipleTagModuleInterface
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Utils/MultipleTag.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(MultipleTagModuleImplementation
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/MulitpleTag.cppm"
    LINK_LIBS MultipleTagModuleInterface Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreMultipleTagContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../MultipleTagContractTest.cpp"
    LINK_LIBS MultipleTagModuleInterface MultipleTagModuleImplementation Qt6::Core
    LABELS "core;tags;cpu;contract;standalone"
)

enable_testing()
