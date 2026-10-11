cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioAnalyzerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioAnalyzerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioAnalyzer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioAnalyzerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioAnalyzer.cppm"
    LINK_LIBS AudioAnalyzerModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioAnalyzerUnderTest AudioAnalyzerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioAnalyzerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioAnalyzerStandaloneTest.cpp"
    LINK_LIBS AudioAnalyzerUnderTest AudioAnalyzerModuleInterfaces Qt6::Core
    LABELS "audio;analyzer;fft;cpu;contract;standalone"
)

enable_testing()
