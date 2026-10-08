if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS)
    artifact_add_test(ArtifactExposureEffectContractTest
        SOURCES
            ExposureEffectContractTest.cpp
        LIBS
            ArtifactEffectsColor
    )
    set_tests_properties(ArtifactExposureEffectContractTest PROPERTIES
        LABELS "effect;image-effect;cpu;contract"
    )

    artifact_add_test(ArtifactColorCorrectionEffectContractTest
        SOURCES
            ColorCorrectionEffectContractTest.cpp
        LIBS
            ArtifactEffectsColor
    )
    set_tests_properties(ArtifactColorCorrectionEffectContractTest PROPERTIES
        LABELS "effect;image-effect;cpu;contract"
    )

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
