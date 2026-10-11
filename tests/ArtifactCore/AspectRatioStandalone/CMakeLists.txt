cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAspectRatioStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(AspectRatioModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/AspectRatio.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AspectRatioUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Core/AspectRatio.cppm"
    LINK_LIBS AspectRatioModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AspectRatioUnderTest AspectRatioModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAspectRatioContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AspectRatioContractTest.cpp"
    LINK_LIBS AspectRatioUnderTest AspectRatioModuleInterfaces Qt6::Core
    LABELS "core;geometry;aspect-ratio;cpu;contract;standalone"
)

enable_testing()
