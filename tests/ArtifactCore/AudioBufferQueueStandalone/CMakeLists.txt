cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioBufferQueueStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioBufferQueueModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioBufferQueue.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreAudioBufferQueueStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioBufferQueueStandaloneTest.cpp"
    LINK_LIBS AudioBufferQueueModuleInterfaces Qt6::Core
    LABELS "audio;queue;cpu;contract;standalone"
)

enable_testing()
