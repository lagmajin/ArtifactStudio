cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioDownMixerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioDownMixerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioDownMixer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioDownMixerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDownMixer.cppm"
    LINK_LIBS AudioDownMixerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioDownMixerUnderTest AudioDownMixerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioDownMixerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioDownMixerStandaloneTest.cpp"
    LINK_LIBS AudioDownMixerUnderTest AudioDownMixerModuleInterfaces Qt6::Core
    LABELS "audio;down-mixer;cpu;contract;standalone"
)

enable_testing()
