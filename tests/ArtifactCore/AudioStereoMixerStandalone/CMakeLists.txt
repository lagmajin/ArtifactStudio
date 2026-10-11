cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioStereoMixerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioStereoMixerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioStereoMixer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioStereoMixerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioStereoMixer.cppm"
    LINK_LIBS AudioStereoMixerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioStereoMixerUnderTest AudioStereoMixerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioStereoMixerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioStereoMixerStandaloneTest.cpp"
    LINK_LIBS AudioStereoMixerUnderTest AudioStereoMixerModuleInterfaces Qt6::Core
    LABELS "audio;stereo-mixer;effect;cpu;contract;standalone"
)

enable_testing()
