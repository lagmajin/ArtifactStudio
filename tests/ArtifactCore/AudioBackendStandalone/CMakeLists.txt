cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioBackendStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioBackendModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Audio/AudioBackend.ixx"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreAudioBackendStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioBackendStandaloneTest.cpp"
    LINK_LIBS AudioBackendModuleInterfaces Qt6::Core
    LABELS "audio;backend;api-contract;cpu;standalone"
)

enable_testing()
