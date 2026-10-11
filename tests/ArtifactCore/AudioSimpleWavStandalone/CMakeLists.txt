cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioSimpleWavStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(AudioSimpleWavModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
        "${ARTIFACT_ROOT}/ArtifactCore/include/Audio/SimpleWav.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core Qt6::Multimedia
)

artifact_add_standalone_module_implementation(AudioSimpleWavUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Audio/SimpleWav.cppm"
    LINK_LIBS AudioSimpleWavModuleInterfaces Qt6::Core Qt6::Multimedia
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioSimpleWavUnderTest AudioSimpleWavModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioSimpleWavStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSimpleWavStandaloneTest.cpp"
    LINK_LIBS AudioSimpleWavUnderTest AudioSimpleWavModuleInterfaces Qt6::Core Qt6::Multimedia
    LABELS "audio;wav;io;cpu;contract;standalone"
)

enable_testing()
