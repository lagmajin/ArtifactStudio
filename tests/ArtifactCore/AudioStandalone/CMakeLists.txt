cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreAudioStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(ARTIFACT_VCPKG_INCLUDE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
artifact_ensure_shared_string_modules()

# Shared audio base closure used by the segment contract and effect contracts.
# Every production module source is registered in exactly one target here.
if(NOT TARGET ArtifactCoreStandaloneFoundationModules)
    artifact_add_standalone_module_interface(ArtifactCoreStandaloneFoundationModules
        BASE_DIRS "${ARTIFACT_CORE_ROOT}"
        MODULES
            "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
            "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
            "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
            "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
            "${ARTIFACT_CORE_ROOT}/src/Memory/TrackedPtr.cppm"
        INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    )
endif()

artifact_add_standalone_module_interface(AudioStandaloneCoreModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSegment.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioEffect.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioBackend.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioBufferQueue.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioCache.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioDownMixer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioRingBuffer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioWriter.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/DelayLine.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/AllPassFilter.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/DSP/LFO.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Modulation/Modulator.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Modulation/Router.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialParams.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialMath.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpeakerLayout.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules Qt6::Core
)

# Audio value types build alongside the shared utility string modules.
artifact_add_standalone_module_interface(AudioStandaloneValueModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Utils/Tag.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioDecibels.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioVolume.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}" "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS AudioStandaloneCoreModules ArtifactCoreSharedStringModules Qt6::Core
)

artifact_add_standalone_module_implementation(AudioStandaloneValueImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/Tag.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDecibels.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioVolume.cpp"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS AudioStandaloneValueModules ArtifactCoreSharedStringImplementations AudioStandaloneCoreModules Qt6::Core
)
artifact_add_standalone_module_interface(AudioStandaloneEffectModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioBassTreble.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioBitCrusher.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioChorus.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioCompressor.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioDCBlocker.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioDeEsser.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioDelay.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioExpander.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioHighLowPass.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioParametricEQ.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioPhaser.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioPingPongDelay.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioPlateReverb.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioReverb.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioSpringReverb.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioTremolo.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioStereoWidener.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioStereoMixer.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioTapeSaturation.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioTransientShaper.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioTone.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioVibrato.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Audio/AudioWaveform.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS AudioStandaloneCoreModules AudioStandaloneAnalysisModules Qt6::Core
)

artifact_add_standalone_module_implementation(AudioStandaloneEffectImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioBassTreble.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioBitCrusher.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioChorus.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioCompressor.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDCBlocker.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDeEsser.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDelay.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioExpander.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioHighLowPass.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioParametricEQ.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPhaser.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPingPongDelay.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPlateReverb.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioReverb.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioSpringReverb.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioTremolo.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioStereoWidener.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioStereoMixer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioTapeSaturation.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioTransientShaper.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioTone.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioVibrato.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioWaveform.cpp"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}" "${ARTIFACT_CORE_INCLUDE}/Audio"
    LINK_LIBS AudioStandaloneEffectModules AudioStandaloneCoreModules AudioStandaloneAnalysisModules Qt6::Core
)

artifact_add_standalone_module_interface(AudioStandaloneAnalysisModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Utils/Id.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioSpectrum.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioAnalyzer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioPanner.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioBus.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/AudioMixer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Audio/Spatial/SpatialRenderer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}" "${ARTIFACT_ROOT}/third_party" "${ARTIFACT_VCPKG_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules AudioStandaloneCoreModules AudioStandaloneValueModules Qt6::Core
)

artifact_add_standalone_module_implementation(AudioStandaloneUtilityImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioCache.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioDownMixer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioRingBuffer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/Spatial/SpeakerLayout.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioWriter.cpp"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS AudioStandaloneCoreModules Qt6::Core
)

artifact_add_standalone_module_implementation(AudioStandaloneAnalysisImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioSpectrum.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioAnalyzer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioPanner.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/Spatial/SpatialRenderer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioMixer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Audio/AudioBus.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Utils/Id.cpp"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}" "${ARTIFACT_CORE_INCLUDE}/Audio" "${ARTIFACT_ROOT}/third_party" "${ARTIFACT_VCPKG_INCLUDE}"
    LINK_LIBS AudioStandaloneAnalysisModules AudioStandaloneEffectModules AudioStandaloneCoreModules Qt6::Core
)
if(MSVC)
    add_dependencies(AudioStandaloneValueImplementations AudioStandaloneValueModules)
    get_target_property(_audio_shared_string_ifc_dir ArtifactCoreSharedStringModules ARTIFACT_STANDALONE_IFC_DIR)
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Utils/Tag.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.String.UniString=${_audio_shared_string_ifc_dir}/Utils.String.UniString.ifc")
    add_dependencies(AudioStandaloneEffectImplementations AudioStandaloneEffectModules)
    add_dependencies(AudioStandaloneAnalysisImplementations AudioStandaloneAnalysisModules)
    add_dependencies(AudioStandaloneAnalysisImplementations AudioStandaloneEffectModules)
    add_dependencies(AudioStandaloneUtilityImplementations AudioStandaloneCoreModules)
    unset(_audio_shared_string_ifc_dir)
endif()

artifact_add_standalone_module_interface(AudioStandaloneSimpleWavModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_INCLUDE}/Audio/SimpleWav.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}" "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS AudioStandaloneValueModules Qt6::Core Qt6::Multimedia
)
artifact_add_standalone_module_implementation(AudioStandaloneSimpleWavImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/SimpleWav.cpp"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS AudioStandaloneSimpleWavModules AudioStandaloneValueImplementations Qt6::Core Qt6::Multimedia
)
if(MSVC)
    add_dependencies(AudioStandaloneSimpleWavImplementations AudioStandaloneSimpleWavModules)
endif()

artifact_add_standalone_gtest(ArtifactCoreAudioSegmentStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSegmentStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneCoreModules Qt6::Core
    LABELS "audio;segment;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAudioEffectBaseStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioEffectBaseStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneCoreModules Qt6::Core
    LABELS "core;audio;effect;base;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAllPassFilterStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AllPassFilterStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneCoreModules Qt6::Core
    LABELS "audio;dsp;allpass;filter;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAudioLFOSoloTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioLFOSoloTest.cpp"
    LINK_LIBS AudioStandaloneCoreModules Qt6::Core
    LABELS "audio;dsp;lfo;cpu;contract;standalone"
)

set(_audio_header_suites
    AudioAnalyzer
    AudioBackend
    AudioBufferQueue
    AudioCache
    AudioDelayLine
    AudioDownMixer
    AudioModulators
    AudioModulationRouter
    AudioRingBuffer
    AudioSpatialMath
    AudioSpatialParams
    AudioPanner
    AudioSpatialRenderer
    AudioSpeakerLayout
    AudioWriter
)
foreach(_suite IN LISTS _audio_header_suites)
    artifact_add_standalone_gtest(ArtifactCore${_suite}StandaloneTest
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/../${_suite}StandaloneTest.cpp"
        LINK_LIBS AudioStandaloneUtilityImplementations AudioStandaloneAnalysisImplementations AudioStandaloneCoreModules AudioStandaloneAnalysisModules Qt6::Core
        LABELS "audio;contract;standalone"
    )
endforeach()

artifact_add_standalone_gtest(ArtifactCoreAudioDecibelsStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioDecibelsStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneValueImplementations AudioStandaloneValueModules Qt6::Core
    LABELS "audio;decibels;value-type;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreTagContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../TagContractTest.cpp"
    LINK_LIBS AudioStandaloneValueImplementations AudioStandaloneValueModules Qt6::Core
    LABELS "core;tag;string;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreIdContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../IdContractTest.cpp"
    LINK_LIBS AudioStandaloneAnalysisImplementations AudioStandaloneAnalysisModules Qt6::Core
    LABELS "core;id;uuid;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAudioSimpleWavStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSimpleWavStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneSimpleWavImplementations AudioStandaloneSimpleWavModules AudioStandaloneValueImplementations AudioStandaloneValueModules Qt6::Core Qt6::Multimedia
    LABELS "audio;wav;io;cpu;contract;standalone"
)

artifact_add_standalone_module_interface(AudioStandaloneRasterizerModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_INCLUDE}/Audio/AudioRasterizer.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS Qt6::Core Qt6::Gui
)
artifact_add_standalone_module_implementation(AudioStandaloneRasterizerImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Audio/AudioRasterizer.cpp"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS AudioStandaloneRasterizerModules Qt6::Core Qt6::Gui
)
if(MSVC)
    add_dependencies(AudioStandaloneRasterizerImplementations AudioStandaloneRasterizerModules)
endif()
artifact_add_standalone_gtest(ArtifactCoreAudioRasterizerStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioRasterizerStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneRasterizerImplementations AudioStandaloneRasterizerModules Qt6::Core Qt6::Gui
    LABELS "audio;rasterizer;waveform;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAudioVolumeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioVolumeStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneValueImplementations AudioStandaloneValueModules Qt6::Core
    LABELS "audio;volume;value-type;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAudioVibratoStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioVibratoStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneEffectImplementations AudioStandaloneEffectModules AudioStandaloneCoreModules Qt6::Core
    LABELS "audio;vibrato;effect;cpu;contract;standalone"
)

set(_audio_effect_suites
    AudioBassTreble
    AudioBitCrusher
    AudioChorus
    AudioCompressor
    AudioDCBlocker
    AudioDeEsser
    AudioDelay
    AudioExpander
    AudioHighLowPass
    AudioParametricEQ
    AudioPhaser
    AudioPingPongDelay
    AudioPlateReverb
    AudioReverb
    AudioSpringReverb
    AudioTremolo
    AudioStereoWidener
    AudioStereoMixer
    AudioTapeSaturation
    AudioTransientShaper
    AudioTone
    AudioWaveform
)
foreach(_suite IN LISTS _audio_effect_suites)
    artifact_add_standalone_gtest(ArtifactCore${_suite}StandaloneTest
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/../${_suite}StandaloneTest.cpp"
        LINK_LIBS AudioStandaloneEffectImplementations AudioStandaloneEffectModules AudioStandaloneCoreModules AudioStandaloneAnalysisModules Qt6::Core
        LABELS "audio;effect;contract;standalone"
    )
endforeach()

artifact_add_standalone_gtest(ArtifactCoreAudioSpectrumStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioSpectrumStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneAnalysisImplementations AudioStandaloneAnalysisModules AudioStandaloneCoreModules Qt6::Core
    LABELS "audio;spectrum;loudness;analysis;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreAudioMixerFxPersistenceStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AudioMixerFxPersistenceStandaloneTest.cpp"
    LINK_LIBS AudioStandaloneAnalysisImplementations AudioStandaloneAnalysisModules AudioStandaloneEffectImplementations AudioStandaloneValueImplementations AudioStandaloneUtilityImplementations AudioStandaloneCoreModules Qt6::Core
    LABELS "audio;mixer;persistence;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
