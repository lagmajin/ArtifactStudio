cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFormantExtractorStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(FormantExtractorModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioAnalyzer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/FormantExtractor.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FormantExtractorUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioAnalyzer.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Audio/FormantExtractor.cppm"
    LINK_LIBS FormantExtractorModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(FormantExtractorUnderTest FormantExtractorModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFormantExtractorStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FormantExtractorStandaloneTest.cpp"
    LINK_LIBS FormantExtractorUnderTest FormantExtractorModuleInterfaces Qt6::Core
    LABELS "audio;formant;phoneme;cpu;contract;standalone"
)

enable_testing()
