cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioHighLowPassStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioHighLowPassModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioHighLowPass.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioHighLowPassUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioHighLowPass.cppm"
    LINK_LIBS AudioHighLowPassModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioHighLowPassUnderTest AudioHighLowPassModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioHighLowPassStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioHighLowPassStandaloneTest.cpp"
    LINK_LIBS AudioHighLowPassUnderTest AudioHighLowPassModuleInterfaces Qt6::Core
    LABELS "audio;filter;high-low-pass;cpu;contract;standalone"
)

enable_testing()
