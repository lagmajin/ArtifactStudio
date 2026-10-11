cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioPannerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioPannerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioPanner.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioPannerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPanner.cppm"
    LINK_LIBS AudioPannerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioPannerUnderTest AudioPannerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioPannerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioPannerStandaloneTest.cpp"
    LINK_LIBS AudioPannerUnderTest AudioPannerModuleInterfaces Qt6::Core
    LABELS "audio;panner;cpu;contract;standalone"
)

enable_testing()
