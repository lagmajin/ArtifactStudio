cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioVibratoStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioVibratoModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioVibrato.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioVibratoUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioVibrato.cpp"
    LINK_LIBS AudioVibratoModuleInterfaces Qt6::Core
    # MSVC は .cppm をバイナリ dir へコピーして .cpp として compile するため、
    # 実装内の "../Define/..." が原文位置で解決しない。include/Audio を
    # 追加すると include/Audio/../Define として解決する(既存表記は不変)。
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${ARTIFACT_CORE_ROOT}/include/Audio"
)

if(MSVC)
    add_dependencies(AudioVibratoUnderTest AudioVibratoModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioVibratoStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioVibratoStandaloneTest.cpp"
    LINK_LIBS AudioVibratoUnderTest AudioVibratoModuleInterfaces Qt6::Core
    LABELS "audio;vibrato;effect;cpu;contract;standalone"
)

enable_testing()
