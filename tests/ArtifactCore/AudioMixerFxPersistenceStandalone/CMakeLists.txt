cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioMixerFxPersistenceStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(AudioMixerFxPersistenceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioMixer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioBus.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioDownMixer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioPanner.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioCompressor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioTremolo.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioPingPongDelay.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Id.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringLike.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Memory/TrackedPtr.cppm"
    # Utils/Id.ixx 系は W_OBJECT (third_party/wobjectdefs.h) と Boost を要する。
    # Boost は vcpkg toolchain が解決する。wobject は main と同じく third_party を足す。
    # Id.ixx の <boost/...> は toolchain の自動 -I 対象外のため repo 内 vcpkg を明示する。
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${ARTIFACT_CORE_ROOT}/../third_party"
        "C:/dev/ArtifactStudio/out/vcpkg_installed/x64-windows/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(AudioMixerFxPersistenceUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioMixer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioBus.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioCompressor.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDownMixer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPanner.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioTremolo.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPingPongDelay.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Utils/Id.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cpp"
    LINK_LIBS AudioMixerFxPersistenceModuleInterfaces Qt6::Core
    # MSVC は .cppm をバイナリ dir へコピーして .cpp として compile するため、
    # 実装内の "../Define/..." が原文位置で解決しない。include/Audio を
    # 追加すると include/Audio/../Define として解決する(既存表記は不変)。
    # Utils/Id.cppm は <boost/...> を要するため repo 内 vcpkg も足す。
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include" "${ARTIFACT_CORE_ROOT}/include/Audio"
        "C:/dev/ArtifactStudio/out/vcpkg_installed/x64-windows/include"
)

if(MSVC)
    add_dependencies(AudioMixerFxPersistenceUnderTest AudioMixerFxPersistenceModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioMixerFxPersistenceStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioMixerFxPersistenceStandaloneTest.cpp"
    LINK_LIBS AudioMixerFxPersistenceUnderTest AudioMixerFxPersistenceModuleInterfaces Qt6::Core
    LABELS "audio;mixer;persistence;cpu;contract;standalone"
)

enable_testing()
