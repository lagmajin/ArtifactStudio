cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioSpectrumStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioSpectrumModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSpectrum.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioSpectrumUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioSpectrum.cppm"
    LINK_LIBS AudioSpectrumModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioSpectrumUnderTest AudioSpectrumModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioSpectrumStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSpectrumStandaloneTest.cpp"
    LINK_LIBS AudioSpectrumUnderTest AudioSpectrumModuleInterfaces Qt6::Core
    LABELS "audio;spectrum;loudness;analysis;cpu;contract;standalone"
)

enable_testing()
