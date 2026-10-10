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
    add_dependencies(ArtifactRenderQueueLayerImageIntegrationTest Artifact)
    add_test(NAME ArtifactRenderQueueLayerImageIntegrationTest
        COMMAND ArtifactRenderQueueLayerImageIntegrationTest
            --gtest_filter=RenderQueueLayerImageIntegrationTest.Production2DAnd3DParticlesRenderThroughManagerQueue
    )
    set_tests_properties(ArtifactRenderQueueLayerImageIntegrationTest PROPERTIES
        LABELS "render-queue;renderer;gpu;headless;particle;image;integration"
        TIMEOUT 300
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
endif()
