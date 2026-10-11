cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreIdStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Boost REQUIRED)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(IdModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Utils/Id.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core Boost::boost
)

artifact_add_standalone_module_implementation(IdUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/Id.cppm"
    LINK_LIBS IdModuleInterfaces Qt6::Core Boost::boost
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
)

if(MSVC)
    add_dependencies(IdUnderTest IdModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreIdContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../IdContractTest.cpp"
    LINK_LIBS IdUnderTest IdModuleInterfaces Qt6::Core Boost::boost
    LABELS "core;id;uuid;cpu;contract;standalone"
)

enable_testing()
