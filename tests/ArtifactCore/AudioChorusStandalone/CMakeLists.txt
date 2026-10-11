cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioChorusStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioChorusModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioChorus.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioChorusUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioChorus.cppm"
    LINK_LIBS AudioChorusModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioChorusUnderTest AudioChorusModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioChorusStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioChorusStandaloneTest.cpp"
    LINK_LIBS AudioChorusUnderTest AudioChorusModuleInterfaces Qt6::Core
    LABELS "audio;chorus;effect;cpu;contract;standalone"
)

enable_testing()
