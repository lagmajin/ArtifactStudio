cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioStereoWidenerStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioStereoWidenerModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioStereoWidener.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioStereoWidenerUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioStereoWidener.cpp"
    LINK_LIBS AudioStereoWidenerModuleInterfaces Qt6::Core
    # MSVC は .cppm をバイナリ dir へコピーして .cpp として compile するため、
    # 実装内の "../Define/..." が原文位置で解決しない。include/Audio を
    # 追加すると include/Audio/../Define として解決する(既存表記は不変)。
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${ARTIFACT_CORE_ROOT}/include/Audio"
)

if(MSVC)
    add_dependencies(AudioStereoWidenerUnderTest AudioStereoWidenerModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioStereoWidenerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioStereoWidenerStandaloneTest.cpp"
    LINK_LIBS AudioStereoWidenerUnderTest AudioStereoWidenerModuleInterfaces Qt6::Core
    LABELS "audio;stereo-widener;effect;cpu;contract;standalone"
)

enable_testing()
