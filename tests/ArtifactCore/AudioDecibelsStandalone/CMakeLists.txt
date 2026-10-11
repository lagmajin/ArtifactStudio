cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioDecibelsStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(AudioDecibelsModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioDecibels.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioDecibelsUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDecibels.cppm"
    LINK_LIBS AudioDecibelsModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioDecibelsUnderTest AudioDecibelsModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioDecibelsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioDecibelsStandaloneTest.cpp"
    LINK_LIBS AudioDecibelsUnderTest AudioDecibelsModuleInterfaces Qt6::Core
    LABELS "audio;decibels;value-type;cpu;contract;standalone"
)

enable_testing()
