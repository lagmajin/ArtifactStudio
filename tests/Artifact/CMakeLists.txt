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
    set_tests_properties(ArtifactRenderQueueLayerImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;particle;image;integration"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )
    add_test(NAME ArtifactRenderQueueExrImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.WritesReadableExrThroughManagerQueue
    )
    set_tests_properties(ArtifactRenderQueueExrImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;image;openexr;integration"
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

# App-level project save/load round-trip through the production
# ArtifactProjectManager. Needs no GPU; kept out of the GPU-gated blocks.
# The manager is a process singleton, so this test must not run in parallel
# with other Artifact integration tests.
if(ARTIFACT_BUILD_TESTS)
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
endif()
