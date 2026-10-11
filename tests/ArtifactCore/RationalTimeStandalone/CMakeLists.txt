cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreRationalTimeStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(RationalTimeModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Time/RationalTime.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(RationalTimeUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Time/RationalTime.cppm"
    LINK_LIBS RationalTimeModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(RationalTimeUnderTest RationalTimeModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreRationalTimeContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../RationalTimeContractTest.cpp"
    LINK_LIBS RationalTimeUnderTest RationalTimeModuleInterfaces Qt6::Core
    LABELS "time;rational;cpu;contract;standalone"
)

enable_testing()
