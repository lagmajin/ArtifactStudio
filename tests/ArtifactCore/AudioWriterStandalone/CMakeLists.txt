cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioWriterStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioWriterModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioWriter.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioWriterUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioWriter.cppm"
    LINK_LIBS AudioWriterModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioWriterUnderTest AudioWriterModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioWriterStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioWriterStandaloneTest.cpp"
    LINK_LIBS AudioWriterUnderTest AudioWriterModuleInterfaces Qt6::Core
    LABELS "audio;writer;wav;io;cpu;contract;standalone"
)

enable_testing()
