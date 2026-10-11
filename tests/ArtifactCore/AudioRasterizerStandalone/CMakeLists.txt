cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioRasterizerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioRasterizerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioRasterizer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core Qt6::Gui
)

artifact_add_standalone_module_implementation(AudioRasterizerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioRasterizer.cppm"
    LINK_LIBS AudioRasterizerModuleInterfaces Qt6::Core Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(AudioRasterizerUnderTest AudioRasterizerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioRasterizerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioRasterizerStandaloneTest.cpp"
    LINK_LIBS AudioRasterizerUnderTest AudioRasterizerModuleInterfaces Qt6::Core Qt6::Gui
    LABELS "audio;rasterizer;waveform;cpu;contract;standalone"
)

enable_testing()
