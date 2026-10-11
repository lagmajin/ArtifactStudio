cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreHashValueStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(HashValueModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Utils/HashValue.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(HashValueUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/HashValue.cppm"
    LINK_LIBS HashValueModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(HashValueUnderTest HashValueModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreHashValueContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../HashValueContractTest.cpp"
    LINK_LIBS HashValueUnderTest HashValueModuleInterfaces Qt6::Core
    LABELS "core;hash;cpu;contract;standalone"
)

enable_testing()
