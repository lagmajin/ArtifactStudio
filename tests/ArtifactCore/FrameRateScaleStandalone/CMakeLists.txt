cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFrameRateScaleStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(FrameRateScaleModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Frame/FrameRate.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(UniStringUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cppm"
    LINK_LIBS FrameRateScaleModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(UniStringUnderTest FrameRateScaleModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFrameRateStorageScaleTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameRateStorageScaleTest.cpp"
    LINK_LIBS UniStringUnderTest FrameRateScaleModuleInterfaces Qt6::Core
    LABELS "time;frame-rate;cpu;contract;standalone"
)

enable_testing()
