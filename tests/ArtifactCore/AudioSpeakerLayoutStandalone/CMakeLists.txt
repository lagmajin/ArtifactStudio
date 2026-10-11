cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioSpeakerLayoutStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioSpeakerLayoutModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialParams.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialMath.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpeakerLayout.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioSpeakerLayoutUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/Spatial/SpeakerLayout.cppm"
    LINK_LIBS AudioSpeakerLayoutModuleInterfaces Qt6::Core
)

if(MSVC)
    add_dependencies(AudioSpeakerLayoutUnderTest AudioSpeakerLayoutModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioSpeakerLayoutStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSpeakerLayoutStandaloneTest.cpp"
    LINK_LIBS AudioSpeakerLayoutUnderTest AudioSpeakerLayoutModuleInterfaces Qt6::Core
    LABELS "core;audio;spatial;speaker-layout;cpu;contract;standalone"
)

enable_testing()
