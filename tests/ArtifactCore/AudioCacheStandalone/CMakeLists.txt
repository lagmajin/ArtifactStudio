cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioCacheStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioCacheModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioCache.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioCacheUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioCache.cppm"
    LINK_LIBS AudioCacheModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioCacheUnderTest AudioCacheModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioCacheStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioCacheStandaloneTest.cpp"
    LINK_LIBS AudioCacheUnderTest AudioCacheModuleInterfaces Qt6::Core
    LABELS "audio;cache;cpu;contract;standalone"
)

enable_testing()
