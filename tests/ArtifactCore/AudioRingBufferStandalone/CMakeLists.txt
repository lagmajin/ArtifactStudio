cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioRingBufferStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioRingBufferModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioRingBuffer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioRingBufferUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioRingBuffer.cppm"
    LINK_LIBS AudioRingBufferModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioRingBufferUnderTest AudioRingBufferModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioRingBufferStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioRingBufferStandaloneTest.cpp"
    LINK_LIBS AudioRingBufferUnderTest AudioRingBufferModuleInterfaces Qt6::Core
    LABELS "audio;ring-buffer;spsc;cpu;contract;standalone"
)

enable_testing()
