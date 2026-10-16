if(ARTIFACT_ENABLE_SCRIPT_LAYER_INTEGRATION_TEST)
    artifact_add_test(ArtifactScriptLayerIntegrationTest
        SOURCES
            ArtifactScriptLayerIntegrationTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_target_properties(ArtifactScriptLayerIntegrationTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactScriptLayerIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactScriptLayerIntegrationTest>
                "$<TARGET_FILE_DIR:ArtifactScriptLayerIntegrationTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )

        set(_artifact_script_test_vcpkg_installed "${VCPKG_INSTALLED}")
        if(NOT EXISTS "${_artifact_script_test_vcpkg_installed}/debug/bin"
           AND DEFINED VCPKG_INSTALLED_DIR
           AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows")
            set(_artifact_script_test_vcpkg_installed
                "${VCPKG_INSTALLED_DIR}/x64-windows")
        endif()
        if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
            set(_artifact_script_test_vcpkg_installed
                "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        endif()

        find_program(ARTIFACT_SCRIPT_LAYER_TEST_WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt)
        if(ARTIFACT_SCRIPT_LAYER_TEST_WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ArtifactScriptLayerIntegrationTest POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DWINDEPLOYQT_EXECUTABLE=${ARTIFACT_SCRIPT_LAYER_TEST_WINDEPLOYQT_EXECUTABLE}"
                    "-DTARGET_FILE=$<TARGET_FILE:ArtifactScriptLayerIntegrationTest>"
                    "-DVCPKG_INSTALLED=${_artifact_script_test_vcpkg_installed}"
                    "-DCONFIG=$<CONFIG>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Artifact/cmake/RunWindeployqt.cmake"
                VERBATIM
            )
        endif()

        add_custom_command(TARGET ArtifactScriptLayerIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactScriptLayerIntegrationTest>"
                "-DVCPKG_INSTALLED=${_artifact_script_test_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Artifact/cmake/CopyQtPlatformPlugin.cmake"
            VERBATIM
        )
        add_custom_command(TARGET ArtifactScriptLayerIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactScriptLayerIntegrationTest>"
                "-DVCPKG_INSTALLED=${_artifact_script_test_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Artifact/cmake/CopyVcpkgRuntimeDeps.cmake"
            VERBATIM
        )
    endif()
    set_tests_properties(ArtifactScriptLayerIntegrationTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;ARTIFACT_SCRIPT_LAYER_TEST_FILE=${_artifact_script_layer_fixture}"
        LABELS "script;layer;composition;integration;contract"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS)
artifact_add_test(ArtifactExposureEffectContractTest
    SOURCES
        ExposureEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactExposureEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactColorCorrectionEffectContractTest
    SOURCES
        ColorCorrectionEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactColorCorrectionEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactVibranceEffectContractTest
    SOURCES
        VibranceEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactVibranceEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactBlackAndWhiteEffectContractTest
    SOURCES
        BlackAndWhiteEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactBlackAndWhiteEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactColorTintEffectContractTest
    SOURCES
        ColorTintEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactColorTintEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactPosterizeEffectContractTest
    SOURCES
        PosterizeEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactPosterizeEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactThresholdEffectContractTest
    SOURCES
        ThresholdEffectContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactThresholdEffectContractTest PROPERTIES
    LABELS "effect;cpu;contract"
)

artifact_add_test(ArtifactEffectSequenceContractTest
    SOURCES
        EffectSequenceContractTest.cpp
    LIBS
        ArtifactEffectsColor
)
set_tests_properties(ArtifactEffectSequenceContractTest PROPERTIES
    LABELS "effect;cpu;integration;contract"
)
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS)
    artifact_add_test(ArtifactSpatialImageEffectContractTest
        SOURCES
            SpatialImageEffectContractTest.cpp
        LIBS
            ArtifactEffectsBlur
    )
    set_tests_properties(ArtifactSpatialImageEffectContractTest PROPERTIES
        LABELS "effect;image-effect;cpu;contract;spatial"
    )
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST)
    artifact_add_test(ArtifactOfflineRendererContractTest
        SOURCES
            OfflineRendererContractTest.cpp
        LIBS
            ArtifactRender
    )
    set_tests_properties(ArtifactOfflineRendererContractTest PROPERTIES
        LABELS "renderer;gpu;offline;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
endif()

if((ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST)
   AND (VULKAN_SUPPORTED OR D3D12_SUPPORTED))
    artifact_add_test(ArtifactDiligentDeviceManagerRecoveryContractTest
        SOURCES
            DiligentDeviceManagerRecoveryContractTest.cpp
        LIBS
            ArtifactRender
    )
    set_tests_properties(ArtifactDiligentDeviceManagerRecoveryContractTest PROPERTIES
        LABELS "renderer;gpu;headless;device-lifecycle;contract"
        TIMEOUT 120
        RUN_SERIAL TRUE
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    artifact_add_test(ArtifactClonerGpuRenderContractTest
        SOURCES
            ClonerGpuRenderContractTest.cpp
        LIBS
            ArtifactRender
    )
    set_tests_properties(ArtifactClonerGpuRenderContractTest PROPERTIES
        LABELS "cloner;renderer;gpu;headless;image;integration;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    if(VULKAN_SUPPORTED)
        set_tests_properties(ArtifactClonerGpuRenderContractTest PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ARTIFACT_RENDER_BACKEND=vulkan"
        )
    elseif(D3D12_SUPPORTED)
        set_tests_properties(ArtifactClonerGpuRenderContractTest PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ARTIFACT_RENDER_BACKEND=d3d12"
        )
    endif()
endif()

if((ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST)
   AND (VULKAN_SUPPORTED OR D3D12_SUPPORTED))
    artifact_add_test(ArtifactAudioLayerRenderIntegrationTest
        SOURCES
            AudioLayerRenderQueueIntegrationTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_target_properties(ArtifactAudioLayerRenderIntegrationTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactAudioLayerRenderIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactAudioLayerRenderIntegrationTest>
                "$<TARGET_FILE_DIR:ArtifactAudioLayerRenderIntegrationTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    if(WIN32)
        set(_artifact_audio_test_qpa_platform "windows")
    else()
        set(_artifact_audio_test_qpa_platform "offscreen")
    endif()
    set_tests_properties(ArtifactAudioLayerRenderIntegrationTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=${_artifact_audio_test_qpa_platform}"
        LABELS "audio;video;mux;sync;render-queue;renderer;waveform;integration;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )

    # Production Image/Solid layers with fracture.* enabled, rendered through
    # the headless offscreen viewport path (OffscreenCompositionRenderer +
    # production layer->draw() incl. drawFractureOverlay). Compares
    # pre/post-trigger frames.
    artifact_add_test(ArtifactFractureLayerRenderTest
        SOURCES
            ArtifactFractureLayerRenderTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactFractureLayerRenderTest PROPERTIES
        LABELS "fracture;layer;renderer;gpu;headless;image;integration;contract"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    if(VULKAN_SUPPORTED)
        set_tests_properties(ArtifactFractureLayerRenderTest PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ARTIFACT_RENDER_BACKEND=vulkan"
        )
    elseif(D3D12_SUPPORTED)
        set_tests_properties(ArtifactFractureLayerRenderTest PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ARTIFACT_RENDER_BACKEND=d3d12"
        )
    endif()
endif()

if((ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST)
   AND (VULKAN_SUPPORTED OR D3D12_SUPPORTED))
    artifact_add_test(ArtifactParticle3DRenderContractTest
        SOURCES
            Particle3DRenderContractTest.cpp
        LIBS
            ArtifactRender
    )
    set_target_properties(ArtifactParticle3DRenderContractTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    set_tests_properties(ArtifactParticle3DRenderContractTest PROPERTIES
        LABELS "particle;renderer;gpu;headless;3d;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )

    # Exercise the production ArtifactParticle3DLayer through its standalone
    # capture runner, including the paired depth-enabled/disabled image check.
    add_dependencies(ArtifactParticle3DRenderContractTest
        ArtifactParticle3DPlayground
    )
    add_test(NAME ArtifactParticle3DLayerImageCaptureTest
        COMMAND $<TARGET_FILE:ArtifactParticle3DPlayground>
            --capture
            "${CMAKE_BINARY_DIR}/particle_3d_capture_test/$<CONFIG>"
    )
    set_tests_properties(ArtifactParticle3DLayerImageCaptureTest PROPERTIES
        LABELS "particle;renderer;gpu;headless;3d;layer;image;integration;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )

    # Exercise the same render queue service used by Render Manager with real
    # production layers, then inspect the PNGs written by the queue worker.
    add_executable(ArtifactRenderQueueLayerImageIntegrationTest
        RenderQueueLayerImageIntegrationTest.cpp
    )
    target_link_libraries(ArtifactRenderQueueLayerImageIntegrationTest
        PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Widgets
                ArtifactAppRuntime
    )
    set_target_properties(ArtifactRenderQueueLayerImageIntegrationTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactRenderQueueLayerImageIntegrationTest
            PRIVATE /Zf /Zc:__cplusplus /utf-8 /MP
        )
    endif()
    set_target_properties(ArtifactRenderQueueLayerImageIntegrationTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactRenderQueueLayerImageIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactRenderQueueLayerImageIntegrationTest>
                "$<TARGET_FILE_DIR:ArtifactRenderQueueLayerImageIntegrationTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )

        find_program(ARTIFACT_TEST_WINDEPLOYQT_EXECUTABLE NAMES windeployqt)
        set(_artifact_test_vcpkg_installed "${VCPKG_INSTALLED}")
        if(NOT EXISTS "${_artifact_test_vcpkg_installed}/debug/bin"
           AND DEFINED VCPKG_INSTALLED_DIR
           AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows")
            set(_artifact_test_vcpkg_installed "${VCPKG_INSTALLED_DIR}/x64-windows")
        endif()
        if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
            set(_artifact_test_vcpkg_installed "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        endif()

        if(ARTIFACT_TEST_WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ArtifactRenderQueueLayerImageIntegrationTest POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DWINDEPLOYQT_EXECUTABLE=${ARTIFACT_TEST_WINDEPLOYQT_EXECUTABLE}"
                    "-DTARGET_FILE=$<TARGET_FILE:ArtifactRenderQueueLayerImageIntegrationTest>"
                    "-DVCPKG_INSTALLED=${_artifact_test_vcpkg_installed}"
                    "-DCONFIG=$<CONFIG>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Artifact/cmake/RunWindeployqt.cmake"
                VERBATIM
            )
        endif()

        add_custom_command(TARGET ArtifactRenderQueueLayerImageIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactRenderQueueLayerImageIntegrationTest>"
                "-DVCPKG_INSTALLED=${_artifact_test_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Artifact/cmake/CopyQtPlatformPlugin.cmake"
            VERBATIM
        )
        add_custom_command(TARGET ArtifactRenderQueueLayerImageIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactRenderQueueLayerImageIntegrationTest>"
                "-DVCPKG_INSTALLED=${_artifact_test_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Artifact/cmake/CopyVcpkgRuntimeDeps.cmake"
            VERBATIM
        )

        if(ARTIFACT_AGILITY_SDK_DIR)
            set(_artifact_test_d3d12core
                "${ARTIFACT_AGILITY_SDK_DIR}/build/native/bin/x64/D3D12Core.dll")
            if(EXISTS "${_artifact_test_d3d12core}")
                add_custom_command(TARGET ArtifactRenderQueueLayerImageIntegrationTest POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${_artifact_test_d3d12core}"
                        "$<TARGET_FILE_DIR:ArtifactRenderQueueLayerImageIntegrationTest>/"
                    VERBATIM
                )
            endif()
        endif()

        if(_artifact_windows_sdk_root AND _artifact_windows_sdk_version_name)
            set(_artifact_test_dxc_dir
                "${_artifact_windows_sdk_root}/bin/${_artifact_windows_sdk_version_name}/x64")
            if(EXISTS "${_artifact_test_dxc_dir}/dxcompiler.dll"
               AND EXISTS "${_artifact_test_dxc_dir}/dxil.dll")
                add_custom_command(TARGET ArtifactRenderQueueLayerImageIntegrationTest POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${_artifact_test_dxc_dir}/dxcompiler.dll"
                        "${_artifact_test_dxc_dir}/dxil.dll"
                        "$<TARGET_FILE_DIR:ArtifactRenderQueueLayerImageIntegrationTest>/"
                    VERBATIM
                )
            endif()
        endif()
    endif()
    add_test(NAME ArtifactRenderQueueLayerImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.Production2DAnd3DParticlesRenderThroughManagerQueue
    )
    add_test(NAME ArtifactShapeVisualTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=ShapeVisualTest.*
            --gtest_output=xml:shape-visual-results.xml
    )
    set_tests_properties(ArtifactShapeVisualTest PROPERTIES
        LABELS "shape;renderer;gpu;headless;image;integration"
        TIMEOUT 2100
        RUN_SERIAL TRUE
    )
    set_tests_properties(ArtifactRenderQueueLayerImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;particle;image;integration"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueue2DParticleBrightnessEffectIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.Production2DAnd3DParticlesRenderThroughManagerQueue
    )
    set_tests_properties(ArtifactRenderQueue2DParticleBrightnessEffectIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;particle;effect;image;integration;contract"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueExrImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.ExportKeepsGuiEventLoopResponsiveAndWritesReadableExr
    )
    set_tests_properties(ArtifactRenderQueueExrImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;gui;responsiveness;image;openexr;integration"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueAnimatedTransformGpuIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.AnimatedLayerTransformMovesPixelsAcrossGpuRenderedFrames
    )
    set_tests_properties(ArtifactRenderQueueAnimatedTransformGpuIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;transform;animation;integration"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueTextLayerImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.ProductionTextLayerRendersThroughManagerQueue
    )
    set_tests_properties(ArtifactRenderQueueTextLayerImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;text;image;integration"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    foreach(_artifact_render_queue_text_case IN ITEMS
            AnimatedSourceTextRendersBothFramesThroughManagerQueue
            ComplexArabicIndicTextRendersThroughManagerQueue
            EmojiSequencesRenderThroughManagerQueue)
        string(REGEX REPLACE "([a-z])([A-Z])" "\\1_\\2"
            _artifact_render_queue_text_test_suffix
            "${_artifact_render_queue_text_case}")
        string(TOLOWER "${_artifact_render_queue_text_test_suffix}"
            _artifact_render_queue_text_test_suffix)
        set(_artifact_render_queue_text_test_name
            "ArtifactRenderQueue_${_artifact_render_queue_text_test_suffix}")
        add_test(NAME "${_artifact_render_queue_text_test_name}"
            COMMAND ArtifactRenderQueueLayerImageIntegrationTest
                "--gtest_filter=RenderQueueLayerImageIntegrationTest.${_artifact_render_queue_text_case}"
        )
        set_tests_properties("${_artifact_render_queue_text_test_name}" PROPERTIES
            LABELS "render-queue;renderer;gpu;headless;text;image;integration"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
    endforeach()
    add_test(NAME ArtifactRenderQueueXpuMixedImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.XpuMixedCpuAndGpuFramesRenderThroughManagerQueue
    )
    set_tests_properties(ArtifactRenderQueueXpuMixedImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;xpu;image;integration"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueXpuParticleImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.XpuMixedTwoDAndThreeDParticlesThroughRenderQueue
    )
    set_tests_properties(ArtifactRenderQueueXpuParticleImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;xpu;particle;image;integration"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueXpuAllAdaptersIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.XpuMixedRunsEveryAvailableFullGpuAdapterAlongsideCpu
    )
    set_tests_properties(ArtifactRenderQueueXpuAllAdaptersIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;xpu;multi-gpu;integration"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueXpuVulkanD3D12MixIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.XpuMixedRunsEveryAvailableFullGpuAdapterAlongsideCpu
    )
    set_tests_properties(ArtifactRenderQueueXpuVulkanD3D12MixIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;xpu;vulkan;d3d12;integration"
        TIMEOUT 300
        RUN_SERIAL TRUE
        ENVIRONMENT "ARTIFACT_RENDER_BACKEND=vulkan"
    )
endif()

if(ARTIFACT_BUILD_TESTS AND (VULKAN_SUPPORTED OR D3D12_SUPPORTED))
    # The submitter implementation module has no primary interface in the
    # Artifact submodule. Both backend-specific test paths need the root-owned
    # test shim and Config provider when they link this runtime target.
    target_link_libraries(ArtifactTextGlyphSubmitterRuntime PUBLIC
        ArtifactGpuFoundation
    )
    target_sources(ArtifactTextGlyphSubmitterRuntime PRIVATE
        FILE_SET text_glyph_submitter_test_modules TYPE CXX_MODULES FILES
        "${CMAKE_CURRENT_LIST_DIR}/ArtifactTextGlyphSubmitterTestModule.ixx"
    )

    artifact_add_test(ArtifactTextRenderTargetContractTest
        SOURCES
            TextRenderTargetContractTest.cpp
        LIBS
            ArtifactTextRenderTargetRuntime
    )
    if(VULKAN_SUPPORTED)
        target_compile_definitions(ArtifactTextRenderTargetContractTest PRIVATE
            ARTIFACT_TEXT_TEST_USE_VULKAN=1
        )
        target_include_directories(ArtifactTextRenderTargetContractTest PRIVATE
            "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
            "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineVulkan/interface"
            "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Common/interface"
            "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Platforms/interface"
            "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Primitives/interface"
        )
    else()
        target_link_libraries(ArtifactTextRenderTargetContractTest PRIVATE
            ArtifactTextGlyphSubmitterRuntime
            ArtifactGpuFoundation
        )
    endif()
    set_tests_properties(ArtifactTextRenderTargetContractTest PROPERTIES
        LABELS "renderer;gpu;headless"
    )
endif()

# The offscreen fixture creates its device through Diligent's Vulkan backend so
# it can exercise glyph rendering on Vulkan-only hosts as well.
if(ARTIFACT_BUILD_TESTS AND VULKAN_SUPPORTED)
    artifact_add_test(ArtifactTextGlyphRenderContractTest
        SOURCES
            TextGlyphRenderContractTest.cpp
        LIBS
            ArtifactTextRenderTargetRuntime
            ArtifactTextGlyphSubmitterRuntime
            ArtifactCoreText
    )
    target_compile_definitions(ArtifactTextGlyphRenderContractTest PRIVATE
        ARTIFACT_TEXT_TEST_USE_VULKAN=1
    )
    target_include_directories(ArtifactTextGlyphRenderContractTest PRIVATE
        "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineVulkan/interface"
        "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Platforms/interface"
        "${CMAKE_SOURCE_DIR}/libs/DiligentEngine/DiligentCore/Primitives/interface"
    )
    set_tests_properties(ArtifactTextGlyphRenderContractTest PROPERTIES
        LABELS "animation;renderer;gpu;text;headless;contract"
    )
endif()

if(ARTIFACT_BUILD_TESTS)
    # Playback range service contract runs in its own process with a Core
    # event loop; it does not start playback or initialize a graphics device.
    artifact_add_test(ArtifactPlaybackRangeServiceContractTest
        SOURCES
            PlaybackRangeServiceContractTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactPlaybackRangeServiceContractTest PROPERTIES
        LABELS "playback;range;service;cpu;contract"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )

    # Frame cache contract tests use the renderer library's CPU cache model,
    # without creating a graphics device or starting the application.
    artifact_add_test(ArtifactFrameCacheContractTest
        SOURCES
            FrameCacheContractTest.cpp
        LIBS
            ArtifactRender
    )
    set_tests_properties(ArtifactFrameCacheContractTest PROPERTIES
        LABELS "render-cache;cpu;contract"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )

    # Standalone contract and CPU integration tests. They use separate
    # executables so none starts Artifact.exe or shares in-process test state.
    artifact_add_test(ArtifactInOutPointsContractTest
        SOURCES
            InOutPointsContractTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactInOutPointsContractTest PROPERTIES
        LABELS "playback;range;cpu;contract"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )

    artifact_add_test(ArtifactRasterizerMaskEffectIntegrationTest
        SOURCES
            RasterizerMaskEffectIntegrationTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactRasterizerMaskEffectIntegrationTest PROPERTIES
        LABELS "mask;effect;rasterizer;cpu;integration"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )

    artifact_add_test(ArtifactUndoManagerContractTest
        SOURCES
            UndoManagerContractTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactUndoManagerContractTest PROPERTIES
        LABELS "undo;cpu;contract"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )

    artifact_add_test(ArtifactCompositionNodeStoreContractTest
        SOURCES
            CompositionNodeStoreContractTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactCompositionNodeStoreContractTest PROPERTIES
        LABELS "composition;container;cpu;contract"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )

    # App-level project save/load round-trip through the production
    # ArtifactProjectManager. Needs no GPU; kept out of the GPU-gated blocks.
    # The manager is a process singleton, so this test must not run in parallel
    # with other Artifact integration tests.
    artifact_add_test(ArtifactProjectRoundTripTest
        SOURCES
            ArtifactProjectRoundTripTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactProjectRoundTripTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        LABELS "project;serialization;cpu;contract;integration"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )

    artifact_add_test(ArtifactStillImageLayerRoundTripTest
        SOURCES
            StillImageLayerRoundTripTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactStillImageLayerRoundTripTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        LABELS "standalone;project;layer;image;serialization;cpu;contract;integration"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )

    # Fracture settings save/load + undo. CPU only, no GPU.
    artifact_add_test(ArtifactFracturePropertyTest
        SOURCES
            ArtifactFracturePropertyTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_tests_properties(ArtifactFracturePropertyTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        LABELS "fracture;layer;serialization;undo;cpu;contract"
        TIMEOUT 120
        RUN_SERIAL TRUE
    )
endif()

# Independent app-runtime process for the production 3D model and light
# layers. CPU-only: it exercises creation, source import, properties, and JSON
# restoration without launching the Artifact UI or creating a GPU device.
if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_LAYER_TEST)
    if(WIN32)
        set(_artifact_model3d_light_qpa_platform "windows")
    else()
        set(_artifact_model3d_light_qpa_platform "offscreen")
    endif()

    artifact_add_test(ArtifactModel3DAndLightLayerTest
        SOURCES
            Model3DAndLightLayerTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    target_compile_definitions(ArtifactModel3DAndLightLayerTest PRIVATE
        ARTIFACT_TEST_MODEL_OBJ_PATH="${CMAKE_SOURCE_DIR}/tests/models/test.obj"
    )
    set_target_properties(ArtifactModel3DAndLightLayerTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/LayerTests/$<CONFIG>"
    )
    if(WIN32)
        set(_artifact_model3d_light_vcpkg_installed "${VCPKG_INSTALLED}")
        if(NOT EXISTS "${_artifact_model3d_light_vcpkg_installed}/debug/bin"
           AND DEFINED VCPKG_INSTALLED_DIR
           AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows")
            set(_artifact_model3d_light_vcpkg_installed "${VCPKG_INSTALLED_DIR}/x64-windows")
        endif()
        if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
            set(_artifact_model3d_light_vcpkg_installed "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        endif()
        add_custom_command(TARGET ArtifactModel3DAndLightLayerTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactModel3DAndLightLayerTest>
                "$<TARGET_FILE_DIR:ArtifactModel3DAndLightLayerTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        add_custom_command(TARGET ArtifactModel3DAndLightLayerTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactModel3DAndLightLayerTest>"
                "-DSOURCE_RUNTIME_DIR=${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                "-DVCPKG_INSTALLED=${_artifact_model3d_light_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/CopyLayerTestRuntime.cmake"
            VERBATIM
        )
    endif()
    set_tests_properties(ArtifactModel3DAndLightLayerTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DAndLightLayerTest>/platforms"
        LABELS "layer;model3d;light;serialization;cpu;standalone;contract"
        TIMEOUT 120
        RUN_SERIAL TRUE
    )
endif()

# GPU image contract in its own process. Captures are written under the build
# tree so a reviewer can inspect the production layer output without launching
# the editor or reusing its renderer/device state.
if((ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_LAYER_TEST)
   AND (VULKAN_SUPPORTED OR D3D12_SUPPORTED))
    artifact_add_test(ArtifactModel3DAndLightLayerImageTest
        SOURCES
            Model3DAndLightLayerImageTest.cpp
        LIBS
            ArtifactAppRuntime
    )
    set_target_properties(ArtifactModel3DAndLightLayerImageTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/LayerTests/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactModel3DAndLightLayerImageTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactModel3DAndLightLayerImageTest>
                "$<TARGET_FILE_DIR:ArtifactModel3DAndLightLayerImageTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        add_custom_command(TARGET ArtifactModel3DAndLightLayerImageTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactModel3DAndLightLayerImageTest>"
                "-DSOURCE_RUNTIME_DIR=${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                "-DVCPKG_INSTALLED=${_artifact_model3d_light_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/CopyLayerTestRuntime.cmake"
            VERBATIM
        )
    endif()
    if(VULKAN_SUPPORTED)
        set(_artifact_model3d_light_backend "vulkan")
    else()
        set(_artifact_model3d_light_backend "d3d12")
    endif()
    set_tests_properties(ArtifactModel3DAndLightLayerImageTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DAndLightLayerImageTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
        LABELS "layer;model3d;light;renderer;gpu;headless;image;integration;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )

    # Exercise the production CompositionRenderController viewport, including
    # its own scene-light collection and layer-to-GPU-light conversion.
    add_executable(ArtifactModel3DLightCompositionIntegrationTest
        Model3DLightCompositionIntegrationTest.cpp
    )
    target_link_libraries(ArtifactModel3DLightCompositionIntegrationTest
        PRIVATE
            GTest::gtest
            Qt6::Core
            Qt6::Widgets
            ArtifactAppRuntime
    )
    artifact_add_cppwinrt_include(ArtifactModel3DLightCompositionIntegrationTest)
    set_target_properties(ArtifactModel3DLightCompositionIntegrationTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    target_compile_definitions(ArtifactModel3DLightCompositionIntegrationTest PRIVATE
        ARTIFACT_TEST_MODEL_OBJ_PATH="${CMAKE_SOURCE_DIR}/tests/models/light_composition_icosphere.obj"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/fixtures/fbx-materials/separate-pbr.fbx")
        target_compile_definitions(ArtifactModel3DLightCompositionIntegrationTest PRIVATE
            ARTIFACT_TEST_FBX_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures/fbx-materials/separate-pbr.fbx"
        )
    endif()
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactModel3DLightCompositionIntegrationTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /MP
        )
    endif()
    set_target_properties(ArtifactModel3DLightCompositionIntegrationTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/LayerTests/$<CONFIG>"
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactModel3DLightCompositionIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactModel3DLightCompositionIntegrationTest>
                "$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        add_custom_command(TARGET ArtifactModel3DLightCompositionIntegrationTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>"
                "-DSOURCE_RUNTIME_DIR=${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                "-DVCPKG_INSTALLED=${_artifact_model3d_light_vcpkg_installed}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/CopyLayerTestRuntime.cmake"
            VERBATIM
        )
    endif()
    set(_artifact_model3d_light_integration_cases
        Point Spot Parallel Ambient Area
    )
    foreach(_light_case IN LISTS _artifact_model3d_light_integration_cases)
        string(TOLOWER "${_light_case}" _light_case_lower)
        set(_light_test_name
            "ArtifactModel3DLightComposition${_light_case}IntegrationTest")
        add_test(NAME "${_light_test_name}"
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerApplies${_light_case}LightLayer"
        )
        set_tests_properties("${_light_test_name}" PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
            LABELS "layer;model3d;light;renderer;gpu;composition;integration;contract;${_light_case_lower}"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
    endforeach()
    add_test(NAME ArtifactModel3DLightCompositionPointLinkingIntegrationTest
        COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
            "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerRespectsPointLightLinking"
    )
    set_tests_properties(ArtifactModel3DLightCompositionPointLinkingIntegrationTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
        LABELS "layer;model3d;light;light-linking;renderer;gpu;composition;integration;contract;point"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactModel3DLightCompositionMultiplePointLightsIntegrationTest
        COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
            "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerCombinesMultiplePointLights"
    )
    set_tests_properties(ArtifactModel3DLightCompositionMultiplePointLightsIntegrationTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
        LABELS "layer;model3d;light;multiple-lights;renderer;gpu;composition;integration;contract;point"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactModel3DLightCompositionVisibilityIntegrationTest
        COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
            "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerRespectsLightVisibilityAndModelOptOut"
    )
    set_tests_properties(ArtifactModel3DLightCompositionVisibilityIntegrationTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
        LABELS "layer;model3d;light;visibility;renderer;gpu;composition;integration;contract;point"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactModel3DLightCompositionOcclusionIntegrationTest
        COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
            "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerDepthOccludesLaterFarModel"
    )
    set_tests_properties(ArtifactModel3DLightCompositionOcclusionIntegrationTest PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
        LABELS "layer;model3d;light;occlusion;depth;renderer;gpu;composition;integration;contract"
        TIMEOUT 180
        RUN_SERIAL TRUE
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/fixtures/fbx-materials/separate-pbr.fbx")
        add_test(NAME ArtifactFbxLightRayTracingIntegrationTest
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerDisplaysLitFbxAndRegistersRayTracingGeometry"
        )
        set_tests_properties(ArtifactFbxLightRayTracingIntegrationTest PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=${_artifact_model3d_light_backend};ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/$<CONFIG>"
            LABELS "fbx;model3d;light;ray-tracing;renderer;gpu;composition;integration;contract"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
        if(VULKAN_SUPPORTED AND D3D12_SUPPORTED)
            add_test(NAME ArtifactFbxLightRayTracingD3D12IntegrationTest
                COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                    "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerDisplaysLitFbxAndRegistersRayTracingGeometry"
            )
            set_tests_properties(ArtifactFbxLightRayTracingD3D12IntegrationTest PROPERTIES
                ENVIRONMENT
                    "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=d3d12;ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/d3d12/$<CONFIG>"
                LABELS "fbx;model3d;light;ray-tracing;renderer;gpu;composition;integration;contract;d3d12"
                TIMEOUT 180
                RUN_SERIAL TRUE
            )
        endif()
    endif()
    if(VULKAN_SUPPORTED AND D3D12_SUPPORTED)
        foreach(_light_case IN LISTS _artifact_model3d_light_integration_cases)
            string(TOLOWER "${_light_case}" _light_case_lower)
            set(_light_test_name
                "ArtifactModel3DLightCompositionD3D12${_light_case}IntegrationTest")
            add_test(NAME "${_light_test_name}"
                COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                    "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerApplies${_light_case}LightLayer"
            )
            set_tests_properties("${_light_test_name}" PROPERTIES
                ENVIRONMENT
                    "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=d3d12;ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/d3d12/$<CONFIG>"
                LABELS "layer;model3d;light;renderer;gpu;composition;integration;contract;d3d12;${_light_case_lower}"
                TIMEOUT 180
                RUN_SERIAL TRUE
            )
        endforeach()
        add_test(NAME ArtifactModel3DLightCompositionD3D12PointLinkingIntegrationTest
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerRespectsPointLightLinking"
        )
        set_tests_properties(ArtifactModel3DLightCompositionD3D12PointLinkingIntegrationTest PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=d3d12;ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/d3d12/$<CONFIG>"
            LABELS "layer;model3d;light;light-linking;renderer;gpu;composition;integration;contract;d3d12;point"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
        add_test(NAME ArtifactModel3DLightCompositionD3D12MultiplePointLightsIntegrationTest
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerCombinesMultiplePointLights"
        )
        set_tests_properties(ArtifactModel3DLightCompositionD3D12MultiplePointLightsIntegrationTest PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=d3d12;ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/d3d12/$<CONFIG>"
            LABELS "layer;model3d;light;multiple-lights;renderer;gpu;composition;integration;contract;d3d12;point"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
        add_test(NAME ArtifactModel3DLightCompositionD3D12VisibilityIntegrationTest
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerRespectsLightVisibilityAndModelOptOut"
        )
        set_tests_properties(ArtifactModel3DLightCompositionD3D12VisibilityIntegrationTest PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=d3d12;ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/d3d12/$<CONFIG>"
            LABELS "layer;model3d;light;visibility;renderer;gpu;composition;integration;contract;d3d12;point"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
        add_test(NAME ArtifactModel3DLightCompositionD3D12OcclusionIntegrationTest
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerDepthOccludesLaterFarModel"
        )
        set_tests_properties(ArtifactModel3DLightCompositionD3D12OcclusionIntegrationTest PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=${_artifact_model3d_light_qpa_platform};QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:ArtifactModel3DLightCompositionIntegrationTest>/platforms;ARTIFACT_RENDER_BACKEND=d3d12;ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR=${CMAKE_BINARY_DIR}/model3d_light_capture/d3d12/$<CONFIG>"
            LABELS "layer;model3d;light;occlusion;depth;renderer;gpu;composition;integration;contract;d3d12"
            TIMEOUT 180
            RUN_SERIAL TRUE
        )
    endif()
    if(WIN32)
        add_test(NAME ArtifactModel3DLightWindowsSehDetectionTest
            COMMAND $<TARGET_FILE:ArtifactModel3DLightCompositionIntegrationTest>
                "--gtest_filter=Model3DLightCompositionIntegrationTest.CompositionControllerAppliesPointLightLayer"
        )
        set_tests_properties(ArtifactModel3DLightWindowsSehDetectionTest PROPERTIES
            ENVIRONMENT "ARTIFACT_TEST_TRIGGER_UNHANDLED_EXCEPTION=1"
            PASS_REGULAR_EXPRESSION "SEH exception with code 0xc0000005 thrown in the test body"
            LABELS "layer;model3d;light;exception;seh;diagnostic;standalone"
            TIMEOUT 30
            RUN_SERIAL TRUE
        )
    endif()
endif()
