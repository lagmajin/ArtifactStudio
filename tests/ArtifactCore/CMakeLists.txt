# ArtifactCore Tests

if(ARTIFACT_BUILD_TESTS)
    add_subdirectory(MeshShaderLodStandalone)
endif()

if(ARTIFACT_ENABLE_RAY_TRACING_TEST)
    if(TARGET Diligent-GraphicsEngineVk-static)
        set(_artifact_rt_test_backend Diligent-GraphicsEngineVk-static)
    elseif(TARGET Diligent-GraphicsEngineVk-shared)
        set(_artifact_rt_test_backend Diligent-GraphicsEngineVk-shared)
    else()
        message(FATAL_ERROR "ARTIFACT_ENABLE_RAY_TRACING_TEST requires the Diligent Vulkan backend")
    endif()

    artifact_add_test(ArtifactCoreRayTracingAcceptanceTest
        SOURCES
            RayTracingAcceptanceTest.cpp
        LIBS
            ArtifactCore
            ${_artifact_rt_test_backend}
    )
    if(PLATFORM_WIN32)
        copy_shader_compiler_dlls(ArtifactCoreRayTracingAcceptanceTest
            DXCOMPILER_FOR_SPIRV YES)
    endif()
    set_tests_properties(ArtifactCoreRayTracingAcceptanceTest PROPERTIES
        LABELS "renderer;gpu;ray-tracing;headless;acceptance;lod;core"
        TIMEOUT 180
        RUN_SERIAL TRUE
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ARTIFACT_RENDER_BACKEND=vulkan;ARTIFACT_RT_FRAME_OUTPUT_DIR=${CMAKE_BINARY_DIR}/artifacts/ray-tracing-frames"
    )
endif()


# Mesh-shader meshlet LOD selection and draw contract. Kept in its own process
# so it can run without ray tracing support or ray-tracing test state.
if((ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_RAY_TRACING_TEST
    OR ARTIFACT_ENABLE_MESH_SHADER_LOD_TEST) AND VULKAN_SUPPORTED)
    if(TARGET Diligent-GraphicsEngineVk-static)
        set(_artifact_mesh_lod_test_backend Diligent-GraphicsEngineVk-static)
    elseif(TARGET Diligent-GraphicsEngineVk-shared)
        set(_artifact_mesh_lod_test_backend Diligent-GraphicsEngineVk-shared)
    else()
        set(_artifact_mesh_lod_test_backend "")
    endif()
    if(ARTIFACT_ENABLE_MESH_SHADER_LOD_TEST AND
       NOT _artifact_mesh_lod_test_backend)
        message(FATAL_ERROR
            "ARTIFACT_ENABLE_MESH_SHADER_LOD_TEST requires a Diligent Vulkan backend target")
    endif()
    if(_artifact_mesh_lod_test_backend)
        artifact_add_test(ArtifactCoreMeshShaderLodTest
            SOURCES
                MeshShaderLodTest.cpp
            LIBS
                ArtifactCore
                ${_artifact_mesh_lod_test_backend}
        )
        if(PLATFORM_WIN32)
            copy_shader_compiler_dlls(ArtifactCoreMeshShaderLodTest
                DXCOMPILER_FOR_SPIRV YES)
        endif()
        set_tests_properties(ArtifactCoreMeshShaderLodTest PROPERTIES
            LABELS "renderer;gpu;gpu-driven;mesh-shader;meshlet;lod;headless;standalone;core"
            TIMEOUT 180
            RUN_SERIAL TRUE
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ARTIFACT_RENDER_BACKEND=vulkan"
        )
    endif()
endif()

if(ARTIFACT_ENABLE_MESH_SHADER_LOD_TEST AND NOT VULKAN_SUPPORTED)
    message(FATAL_ERROR
        "ARTIFACT_ENABLE_MESH_SHADER_LOD_TEST requires Vulkan support")
endif()


if(ARTIFACT_BUILD_TESTS)
    add_subdirectory(AnimationDynamicsStandalone)
endif()


if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_KEYING_VIDEO_TEST)
    artifact_add_test(ArtifactCoreKeyingVideoInputTest
        SOURCES
            KeyingVideoInputTest.cpp
        LIBS
            ArtifactCore
    )
    set_tests_properties(ArtifactCoreKeyingVideoInputTest PROPERTIES
        LABELS "keying;video;cpu;core"
        TIMEOUT 60
        RUN_SERIAL TRUE
    )
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_TRACKING_VIDEO_TEST)
    artifact_add_test(ArtifactCoreMotionTrackerVideoInputTest
        SOURCES
            MotionTrackerVideoInputTest.cpp
        LIBS
            ArtifactCoreTracking
    )
    set_tests_properties(ArtifactCoreMotionTrackerVideoInputTest PROPERTIES
        LABELS "tracking;video;cpu;core"
        TIMEOUT 60
        RUN_SERIAL TRUE
        ENVIRONMENT "ARTIFACT_TRACKING_COMPOSITE_OUTPUT=${CMAKE_BINARY_DIR}/tracking-composite-preview.avi"
    )
endif()

artifact_add_test(ArtifactCoreTrackingDataContractTest
    SOURCES
        TrackingDataContractTest.cpp
    LIBS
        ArtifactCoreTracking
)
set_tests_properties(ArtifactCoreTrackingDataContractTest PROPERTIES
    LABELS "tracking;data;homography;interpolation;cpu;contract;core"
)

artifact_add_test(ArtifactCorePlanarTrackerContractTest
    SOURCES
        PlanarTrackerContractTest.cpp
    LIBS
        ArtifactCoreTracking
)
set_tests_properties(ArtifactCorePlanarTrackerContractTest PROPERTIES
    LABELS "tracking;planar;homography;opencv;cpu;contract;core"
    TIMEOUT 60
)

artifact_add_test(ArtifactCoreTest
    SOURCES
        UtilsTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreCommandSandboxContractTest
    SOURCES
        CommandSandboxContractTest.cpp
    LIBS
        ArtifactCoreAI
        ArtifactCore
)
set_tests_properties(ArtifactCoreCommandSandboxContractTest PROPERTIES
    LABELS "ai;security;process;dry-run;contract;core"
)

artifact_add_test(ArtifactCoreCommandIRContractTest
    SOURCES
        CommandIRContractTest.cpp
    LIBS
        ArtifactCoreAI
        ArtifactCore
)
set_tests_properties(ArtifactCoreCommandIRContractTest PROPERTIES
    LABELS "ai;validation;contract;core"
)

artifact_add_test(ArtifactCoreAngelScriptTest
    SOURCES
        AngelScriptEngineTest.cpp
        LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactScriptTest
    SOURCES
        ArtifactScriptTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreLayerScriptComponentTest
    SOURCES
        LayerScriptComponentContractTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactScriptObjectTest
    SOURCES
        ArtifactScriptObjectTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactScriptHostMethodTest
    SOURCES
        ArtifactScriptHostMethodTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactScriptHostApiTest
    SOURCES
        ArtifactScriptHostApiTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreCSharpScriptEngineTest
    SOURCES
        CSharpScriptEngineTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreCreativeEffectTest
    SOURCES
        ColorVibranceEffectTest.cpp
    LIBS
        ArtifactCoreVideo
)
set_tests_properties(ArtifactCoreCreativeEffectTest PROPERTIES
    LABELS "effect;image-effect;cpu;contract;core"
)

artifact_add_test(ArtifactCoreAudioMixerRoutingTest
    SOURCES
        AudioMixerRoutingTest.cpp
    LIBS
        ArtifactCoreAudio
)

artifact_add_test(ArtifactCoreAudioModulationRouterTest
    SOURCES
        AudioModulationRouterTest.cpp
    LIBS
        ArtifactCoreAudio
)

artifact_add_test(ArtifactCoreAutomationClipTest
    SOURCES
        AutomationClipTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreAbstractPropertyModulationTest
    SOURCES
        AbstractPropertyModulationTest.cpp
    LIBS
        ArtifactCoreAudio
)

artifact_add_test(ArtifactCoreSourceResolutionContractTest
    SOURCES
        SourceResolutionContractTest.cpp
    LIBS
        ArtifactCoreAsset
)

artifact_add_test(ArtifactCoreNLETest
    SOURCES
        NLETest.cpp
    LIBS
        ArtifactCoreNLE
)

artifact_add_test(ArtifactCoreFrameTimeTest
    SOURCES
        FrameTimeTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreTimeCodeRangeContractTest
    SOURCES
        TimeCodeRangeContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreTimeCodeRangeContractTest PROPERTIES
    LABELS "time;timecode;range;cpu;contract"
)

artifact_add_test(ArtifactCoreViewportTransformerContractTest
    SOURCES
        ViewportTransformerContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreViewportTransformerContractTest PROPERTIES
    LABELS "viewport;transform;coordinates;cpu;contract"
)

artifact_add_test(ArtifactCoreSelectionManagerContractTest
    SOURCES
        SelectionManagerContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreSelectionManagerContractTest PROPERTIES
    LABELS "selection;event-bus;state;cpu;contract"
)

artifact_add_test(ArtifactCoreZoomScaleContractTest
    SOURCES
        ZoomScaleContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreZoomScaleContractTest PROPERTIES
    LABELS "viewport;zoom;scale;cpu;contract"
)

artifact_add_test(ArtifactCoreInputSurfaceManagerContractTest
    SOURCES
        InputSurfaceManagerContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreInputSurfaceManagerContractTest PROPERTIES
    LABELS "input;capture;state;event-bus;cpu;contract"
    RUN_SERIAL TRUE
)

artifact_add_test(ArtifactCoreTransformHelperContractTest
    SOURCES
        TransformHelperContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreTransformHelperContractTest PROPERTIES
    LABELS "transform;2d;3d;coordinates;cpu;contract"
)

artifact_add_test(ArtifactCoreInputBindingContractTest
    SOURCES
        InputBindingContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreInputBindingContractTest PROPERTIES
    LABELS "input;key-binding;shortcuts;cpu;contract"
)

artifact_add_test(ArtifactCoreUiLayoutStateContractTest
    SOURCES
        UiLayoutStateContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreUiLayoutStateContractTest PROPERTIES
    LABELS "ui-layout;serialization;settings;cpu;contract"
)

artifact_add_test(ArtifactCoreShortcutBindingsContractTest
    SOURCES
        ShortcutBindingsContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreShortcutBindingsContractTest PROPERTIES
    LABELS "shortcuts;input;serialization;cpu;contract"
    RUN_SERIAL TRUE
)

artifact_add_test(ArtifactCoreCameraContractTest
    SOURCES
        CameraContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreCameraContractTest PROPERTIES
    LABELS "camera;transform;projection;cpu;contract"
)

artifact_add_test(ArtifactCoreViewOrientationNavigatorContractTest
    SOURCES
        ViewOrientationNavigatorContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreViewOrientationNavigatorContractTest PROPERTIES
    LABELS "viewport;orientation;interaction;cpu;contract"
)

artifact_add_test(ArtifactCoreLightContractTest
    SOURCES
        LightContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLightContractTest PROPERTIES
    LABELS "light;3d;parameters;cpu;contract"
)

artifact_add_test(ArtifactCoreIESProfileContractTest
    SOURCES
        IESProfileContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreIESProfileContractTest PROPERTIES
    LABELS "light;photometry;ies;cpu;contract;core"
)

artifact_add_test(ArtifactCoreActionManagerContractTest
    SOURCES
        ActionManagerContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreActionManagerContractTest PROPERTIES
    LABELS "actions;input;event-bus;cpu;contract"
    RUN_SERIAL TRUE
)

artifact_add_test(ArtifactCoreKeyMapContractTest
    SOURCES
        KeyMapContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreKeyMapContractTest PROPERTIES
    LABELS "keymap;input;shortcuts;serialization;cpu;contract"
    RUN_SERIAL TRUE
)

artifact_add_test(ArtifactCoreLayerLockManagerContractTest
    SOURCES
        LayerLockManagerContractTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreBackgroundTaskWorkerPoolContractTest
    SOURCES
        BackgroundTaskWorkerPoolContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreBackgroundTaskWorkerPoolContractTest PROPERTIES
    LABELS "threading;background-task;cpu;contract;core"
    TIMEOUT 30
)
set_tests_properties(ArtifactCoreLayerLockManagerContractTest PROPERTIES
    LABELS "collaboration;layer-lock;event-bus;cpu;contract"
    RUN_SERIAL TRUE
)

artifact_add_test(ArtifactCoreColorBridgeTest
    SOURCES
        ColorBridgeTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreColorSpaceTest
    SOURCES
        ColorSpaceTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColorSpaceTest PROPERTIES
    LABELS "color;color-space;cpu;unit"
)

artifact_add_test(ArtifactCoreLabColorContractTest
    SOURCES
        LabColorContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLabColorContractTest PROPERTIES
    LABELS "color;lab;conversion;cpu;contract"
)

artifact_add_test(ArtifactCoreOCIOConfigContractTest
    SOURCES
        OCIOConfigContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreOCIOConfigContractTest PROPERTIES
    LABELS "color;ocio;configuration;cpu;contract"
)

artifact_add_test(ArtifactCoreColorConversionTest
    SOURCES
        ColorConversionTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColorConversionTest PROPERTIES
    LABELS "color;cpu;contract"
)

artifact_add_test(ArtifactCoreFloatColorStandaloneTest
    SOURCES
        FloatColorStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreFloatColorStandaloneTest PROPERTIES
    LABELS "color;standalone;cpu;contract"
)

artifact_add_test(ArtifactCoreColorLUTContractTest
    SOURCES
        ColorLUTContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColorLUTContractTest PROPERTIES
    LABELS "color;lut;cpu;contract"
)

artifact_add_test(ArtifactCoreLUTLoaderContractTest
    SOURCES
        LUTLoaderContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLUTLoaderContractTest PROPERTIES
    LABELS "color;grading;lut;file-io;cpu;contract;core"
)

artifact_add_test(ArtifactCoreLUTWriterContractTest
    SOURCES
        LUTWriterContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLUTWriterContractTest PROPERTIES
    LABELS "color;grading;lut;file-io;cpu;contract;core"
)

artifact_add_test(ArtifactCoreColorScopesContractTest
    SOURCES
        ColorScopesContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColorScopesContractTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "color;scopes;image;cpu;contract;core"
)

artifact_add_test(ArtifactCoreColorCurvesContractTest
    SOURCES
        ColorCurvesContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColorCurvesContractTest PROPERTIES
    LABELS "color;grading;curves;image;cpu;contract;core"
)

artifact_add_test(ArtifactCoreAutoColorMatcherContractTest
    SOURCES
        AutoColorMatcherContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreAutoColorMatcherContractTest PROPERTIES
    LABELS "color;matching;image;cpu;contract;core"
)

artifact_add_test(ArtifactCoreRig2DContractTest
    SOURCES
        Rig2DContractTest.cpp
    LIBS
        ArtifactCoreRig
        ArtifactCore
)
set_tests_properties(ArtifactCoreRig2DContractTest PROPERTIES
    LABELS "rig;animation;serialization;cpu;contract;core"
)

artifact_add_test(ArtifactCoreColoramaProcessorContractTest
    SOURCES
        ColoramaProcessorContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColoramaProcessorContractTest PROPERTIES
    LABELS "color;effects;cpu;contract"
)

artifact_add_test(ArtifactCoreSurfacePixelConversionTest
    SOURCES
        SurfacePixelConversionTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreSurfacePixelConversionTest PROPERTIES
    LABELS "color;image;cpu;contract"
)

artifact_add_test(ArtifactCorePixelChannelOrderStandaloneTest
    SOURCES
        PixelChannelOrderStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePixelChannelOrderStandaloneTest PROPERTIES
    LABELS "color;image;pixel-order;standalone;contract"
)

artifact_add_test(ArtifactCoreImageSurfaceViewTest
    SOURCES
        ImageSurfaceViewTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreImageSurfaceViewTest PROPERTIES
    LABELS "image;surface;cpu;contract"
)

artifact_add_test(ArtifactCoreImageYUV420ContractTest
    SOURCES
        ImageYUV420ContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreImageYUV420ContractTest PROPERTIES
    LABELS "image;yuv420;pixel-format;cpu;contract"
)

artifact_add_test(ArtifactCoreDeepImageBufferContractTest
    SOURCES
        DeepImageBufferContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreDeepImageBufferContractTest PROPERTIES
    LABELS "image;deep;compositing;cpu;contract"
)

artifact_add_test(ArtifactCoreGPUThreadPoolContractTest
    SOURCES
        GPUThreadPoolContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreGPUThreadPoolContractTest PROPERTIES
    LABELS "graphics;gpu-queue;cpu;contract"
)

artifact_add_test(ArtifactCorePointwiseFusionGraphContractTest
    SOURCES
        PointwiseFusionGraphContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePointwiseFusionGraphContractTest PROPERTIES
    LABELS "graphics;pointwise-fusion;shader-generation;cpu;contract"
)

artifact_add_test(ArtifactCoreRawImageContractTest
    SOURCES
        RawImageContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRawImageContractTest PROPERTIES
    LABELS "image;import;cpu;contract"
)

artifact_add_test(ArtifactCoreAsyncFileWriterContractTest
    SOURCES
        AsyncFileWriterContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreAsyncFileWriterContractTest PROPERTIES
    LABELS "io;async-file-writer;serialization;cpu;contract"
    TIMEOUT 30
)

artifact_add_test(ArtifactCoreImageAnalyzerContractTest
    SOURCES
        ImageAnalyzerContractTest.cpp
    LIBS
        ArtifactCoreAnalyze
)
set_tests_properties(ArtifactCoreImageAnalyzerContractTest PROPERTIES
    LABELS "image;analysis;cpu;contract"
)

artifact_add_test(ArtifactCorePathContractTest
    SOURCES
        PathContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePathContractTest PROPERTIES
    LABELS "path;cpu;contract"
)

artifact_add_test(ArtifactCoreStringConversionUnicodeContractTest
    SOURCES
        StringConversionUnicodeContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreStringConversionUnicodeContractTest PROPERTIES
    LABELS "utils;string;unicode;cpu;contract;core"
)

artifact_add_test(ArtifactCoreCoordinateProfileContractTest
    SOURCES
        CoordinateProfileContractTest.cpp
    LIBS
        ArtifactCoreCoordinate
)
set_tests_properties(ArtifactCoreCoordinateProfileContractTest PROPERTIES
    LABELS "coordinate;profile;serialization;cpu;contract;core"
)

artifact_add_test(ArtifactCorePropertyPathTest
    SOURCES
        PropertyPathTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePropertyPathTest PROPERTIES
    LABELS "property;path;cpu;contract"
)

artifact_add_test(ArtifactCoreSchemaMigrationContractTest
    SOURCES
        SchemaMigrationContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreSchemaMigrationContractTest PROPERTIES
    LABELS "serialization;schema-migration;cpu;contract"
)

artifact_add_test(ArtifactCoreAssetImportSettingsTest
    SOURCES
        AssetImportSettingsTest.cpp
    LIBS
        ArtifactCoreAsset
)
set_tests_properties(ArtifactCoreAssetImportSettingsTest PROPERTIES
    LABELS "asset;serialization;cpu;contract"
)

artifact_add_test(ArtifactCoreAssetManagerContractTest
    SOURCES
        AssetManagerContractTest.cpp
    LIBS
        ArtifactCoreAsset
)
set_tests_properties(ArtifactCoreAssetManagerContractTest PROPERTIES
    LABELS "asset;source;cpu;contract"
)

artifact_add_test(ArtifactCoreAssetDatabaseContractTest
    SOURCES
        AssetDatabaseContractTest.cpp
    LIBS
        ArtifactCoreAsset
)
set_tests_properties(ArtifactCoreAssetDatabaseContractTest PROPERTIES
    LABELS "asset;database;serialization;cpu;contract"
)

artifact_add_test(ArtifactCoreAssetMetaFileContractTest
    SOURCES
        AssetMetaFileContractTest.cpp
    LIBS
        ArtifactCoreAsset
)
set_tests_properties(ArtifactCoreAssetMetaFileContractTest PROPERTIES
    LABELS "asset;metadata;serialization;cpu;contract"
)

artifact_add_test(ArtifactCoreAssetImporterContractTest
    SOURCES
        AssetImporterContractTest.cpp
    LIBS
        ArtifactCoreAsset
)
set_tests_properties(ArtifactCoreAssetImporterContractTest PROPERTIES
    LABELS "asset;import;cpu;contract"
)

artifact_add_test(ArtifactCoreImageAssetMetadataContractTest
    SOURCES
        ImageAssetMetadataContractTest.cpp
    LIBS
        ArtifactCoreAsset
        ArtifactCoreMedia
)

artifact_add_test(ArtifactCoreAssetSequenceDetectionContractTest
    SOURCES
        AssetSequenceDetectionContractTest.cpp
    LIBS
        ArtifactCoreAsset
)
set_tests_properties(ArtifactCoreAssetSequenceDetectionContractTest PROPERTIES
    LABELS "asset;sequence;cpu;contract"
)
set_tests_properties(ArtifactCoreImageAssetMetadataContractTest PROPERTIES
    LABELS "asset;image;sequence;metadata;cpu;contract"
)

artifact_add_test(ArtifactCoreFileTypeDetectorContractTest
    SOURCES
        FileTypeDetectorContractTest.cpp
    LIBS
        ArtifactCoreFile
)
set_tests_properties(ArtifactCoreFileTypeDetectorContractTest PROPERTIES
    LABELS "file;type-detection;cpu;contract"
)

artifact_add_test(ArtifactCoreLottieExporterContractTest
    SOURCES
        LottieExporterContractTest.cpp
    LIBS
        ArtifactCoreExport
)
set_tests_properties(ArtifactCoreLottieExporterContractTest PROPERTIES
    LABELS "lottie;export;serialization;validation;cpu;contract"
    TIMEOUT 60
)

artifact_add_test(ArtifactCoreColorGradingContractTest
    SOURCES
        ColorGradingContractTest.cpp
    LIBS
        ArtifactCoreColorCollection
)
set_tests_properties(ArtifactCoreColorGradingContractTest PROPERTIES
    LABELS "color;grading;levels;curves;cpu;contract"
    TIMEOUT 60
)

artifact_add_test(ArtifactCoreCpuFrameViewContractTest
    SOURCES
        CpuFrameViewContractTest.cpp
    LIBS
        ArtifactCoreVideo
)
set_tests_properties(ArtifactCoreCpuFrameViewContractTest PROPERTIES
    LABELS "video;cpu-frame;frame-view;cpu;contract;core"
)

artifact_add_test(ArtifactCoreVideoFrameValidityContractTest
    SOURCES
        VideoFrameValidityContractTest.cpp
    LIBS
        ArtifactCoreVideo
)
set_tests_properties(ArtifactCoreVideoFrameValidityContractTest PROPERTIES
    LABELS "video;cpu-frame;gpu-frame;ownership;cpu;contract;core"
)

artifact_add_test(ArtifactCoreVideoStabilizerContractTest
    SOURCES
        VideoStabilizerContractTest.cpp
    LIBS
        ArtifactCoreVideo
)
set_tests_properties(ArtifactCoreVideoStabilizerContractTest PROPERTIES
    LABELS "video;stabilization;motion;cpu;contract;core"
    TIMEOUT 60
)

artifact_add_test(ArtifactCoreEncoderSettingsContractTest
    SOURCES
        EncoderSettingsContractTest.cpp
    LIBS
        ArtifactCoreVideo
)
set_tests_properties(ArtifactCoreEncoderSettingsContractTest PROPERTIES
    LABELS "video;encoder;settings;preset;serialization;cpu;contract"
    TIMEOUT 60
)

artifact_add_test(ArtifactCoreDiagnosticsContractTest
    SOURCES
        CoreDiagnosticsContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreDiagnosticsContractTest PROPERTIES
    LABELS "diagnostics;cpu;contract"
)

artifact_add_test(ArtifactCoreArtifactFoundationTest
    SOURCES
        ArtifactFoundationTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactExpectedTest
    SOURCES
        ArtifactExpectedTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactFunctionRefTest
    SOURCES
        ArtifactFunctionRefTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreArtifactSaturationTest
    SOURCES
        ArtifactSaturationTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreCollaborationSessionTest
    SOURCES
        CollaborationSessionTest.cpp
    LIBS
        ArtifactCoreCollaborate
        ArtifactCoreNetwork
)

artifact_add_test(ArtifactCoreCollabReviewTest
    SOURCES
        CollabReviewTest.cpp
    LIBS
        ArtifactCoreCollaborate
)

artifact_add_test(ArtifactCoreTokenExpansionTest
    SOURCES
        TokenExpansionTest.cpp
    LIBS
        ArtifactCoreEnvironment
)

artifact_add_test(ArtifactCoreEnvScriptTest
    SOURCES
        EnvScriptTest.cpp
    LIBS
        ArtifactCore
        ArtifactCoreEnvironment
)

artifact_add_test(ArtifactCorePhysicsDeterminismTest
    SOURCES
        PhysicsDeterminismTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePhysicsDeterminismTest PROPERTIES
    LABELS "physics;cpu;contract;lod"
)

artifact_add_test(ArtifactCorePhysicsLodSettingsContractTest
    SOURCES
        PhysicsLodSettingsContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePhysicsLodSettingsContractTest PROPERTIES
    LABELS "physics;lod;cpu;contract;core"
)

artifact_add_test(ArtifactCoreFracturePlaneImageTest
    SOURCES
        FracturePlaneImageTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreFracturePlaneImageTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "fracture;physics;cpu;contract;image"
    TIMEOUT 60
)

artifact_add_test(ArtifactCoreMathVecTest
    SOURCES
        MathVecTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreRandomStreamContractTest
    SOURCES
        RandomStreamContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRandomStreamContractTest PROPERTIES
    LABELS "math;random;determinism;cpu;contract;core"
)

artifact_add_test(ArtifactCoreSpatialGridContractTest
    SOURCES
        SpatialGridStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreSpatialGridContractTest PROPERTIES
    LABELS "math;spatial-grid;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCoreResolutionRemapContractTest
    SOURCES
        ResolutionRemapStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreResolutionRemapContractTest PROPERTIES
    LABELS "geometry;resolution-remap;cpu;contract;core"
)

artifact_add_test(ArtifactCoreShapeExtrudeTest
    SOURCES
        ShapeExtrudeTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreVectorExportContractTest
    SOURCES
        VectorExportContractTest.cpp
    LIBS
        ArtifactCoreShape
)
set_tests_properties(ArtifactCoreVectorExportContractTest PROPERTIES
    LABELS "vector-export;svg;html;css-animation;cpu;contract"
)

artifact_add_test(ArtifactCoreBezierPathSamplerContractTest
    SOURCES
        BezierPathSamplerStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreBezierPathSamplerContractTest PROPERTIES
    LABELS "geometry;bezier;path-sampling;cpu;contract"
)

artifact_add_test(ArtifactCoreCameraGuideContractTest
    SOURCES
        CameraGuideStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreCameraGuideContractTest PROPERTIES
    LABELS "geometry;camera-guide;cpu;contract"
)

artifact_add_test(ArtifactCoreLayerBoundsCalculatorContractTest
    SOURCES
        LayerBoundsCalculatorStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLayerBoundsCalculatorContractTest PROPERTIES
    LABELS "geometry;layer-bounds;cpu;contract"
)

artifact_add_test(ArtifactCoreLayerAlignmentContractTest
    SOURCES
        LayerAlignmentStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLayerAlignmentContractTest PROPERTIES
    LABELS "geometry;layer-alignment;cpu;contract"
)

artifact_add_test(ArtifactCoreQuaternionContractTest
    SOURCES
        QuaternionStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreQuaternionContractTest PROPERTIES
    LABELS "math;quaternion;cpu;contract"
)

artifact_add_test(ArtifactCoreMatrix4x4ContractTest
    SOURCES
        Matrix4x4StandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreMatrix4x4ContractTest PROPERTIES
    LABELS "math;matrix;cpu;contract"
)

artifact_add_test(ArtifactCoreArtifactMinMaxContractTest
    SOURCES
        ArtifactMinMaxStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreArtifactMinMaxContractTest PROPERTIES
    LABELS "math;numeric;cpu;contract"
)

artifact_add_test(ArtifactCoreRotationTurnsContractTest
    SOURCES
        RotationTurnsStandaloneTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRotationTurnsContractTest PROPERTIES
    LABELS "math;rotation;cpu;contract"
)

artifact_add_test(ArtifactCoreRotoMaskContractTest
    SOURCES
        RotoMaskContractTest.cpp
    LIBS
        ArtifactCoreMask
)

artifact_add_test(ArtifactCoreDepthMaskContractTest
    SOURCES
        DepthMaskContractTest.cpp
    LIBS
        ArtifactCoreMask
)
set_tests_properties(ArtifactCoreDepthMaskContractTest PROPERTIES
    LABELS "mask;depth;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCoreVolumeMaskContractTest
    SOURCES
        VolumeMaskContractTest.cpp
    LIBS
        ArtifactCoreMask
)
set_tests_properties(ArtifactCoreVolumeMaskContractTest PROPERTIES
    LABELS "mask;volume;geometry;opencv;cpu;contract;core"
)

set_tests_properties(ArtifactCoreRotoMaskContractTest PROPERTIES
    LABELS "mask;roto;animation;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCorePathMorphContractTest
    SOURCES
        PathMorphContractTest.cpp
    LIBS
        ArtifactCoreMask
)
set_tests_properties(ArtifactCorePathMorphContractTest PROPERTIES
    LABELS "mask;path-morph;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCoreSequenceGroupContractTest
    SOURCES
        SequenceGroupContractTest.cpp
    LIBS
        ArtifactCoreSequence
)
set_tests_properties(ArtifactCoreSequenceGroupContractTest PROPERTIES
    LABELS "sequence;transport;timing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreStarfieldSpectralContractTest
    SOURCES
        StarfieldSpectralContractTest.cpp
    LIBS
        ArtifactCoreGenerate
)
set_tests_properties(ArtifactCoreStarfieldSpectralContractTest PROPERTIES
    LABELS "starfield;spectral;generation;cpu;contract;core"
)

artifact_add_test(ArtifactCoreMetadataCollectorContractTest
    SOURCES
        MetadataCollectorContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreMetadataCollectorContractTest PROPERTIES
    LABELS "project;metadata;serialization;csv;cpu;contract;core"
)

artifact_add_test(ArtifactCoreSharedMemoryRingBufferContractTest
    SOURCES
        SharedMemoryRingBufferContractTest.cpp
    LIBS
        ArtifactCoreIPC
)
set_tests_properties(ArtifactCoreSharedMemoryRingBufferContractTest PROPERTIES
    LABELS "ipc;shared-memory;ring-buffer;cpu;contract;core"
)

artifact_add_test(ArtifactCoreLocaleFormattingContractTest
    SOURCES
        LocaleFormattingContractTest.cpp
    LIBS
        ArtifactCoreLocalization
)
set_tests_properties(ArtifactCoreLocaleFormattingContractTest PROPERTIES
    LABELS "localization;formatting;locale;cpu;contract;core"
)

artifact_add_test(ArtifactCoreImageTransformCVContractTest
    SOURCES
        ImageTransformCVContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreImageTransformCVContractTest PROPERTIES
    LABELS "image;opencv;transform;conversion;cpu;contract;core"
)

artifact_add_test(ArtifactCoreOpenVDBSnapshotContractTest
    SOURCES
        OpenVDBSnapshotContractTest.cpp
    LIBS
        ArtifactCoreSimulation
)
set_tests_properties(ArtifactCoreOpenVDBSnapshotContractTest PROPERTIES
    LABELS "simulation;openvdb;snapshot;cpu;contract;core"
)

artifact_add_test(ArtifactCoreTestImageGeneratorContractTest
    SOURCES
        TestImageGeneratorContractTest.cpp
    LIBS
        ArtifactCoreGenerate
)
set_tests_properties(ArtifactCoreTestImageGeneratorContractTest PROPERTIES
    LABELS "image;test-patterns;rgba;cpu;contract;core"
)

artifact_add_test(ArtifactCoreMediaMetaDataContractTest
    SOURCES
        MediaMetaDataContractTest.cpp
    LIBS
        ArtifactCoreMedia
)
set_tests_properties(ArtifactCoreMediaMetaDataContractTest PROPERTIES
    LABELS "media;metadata;streams;formatting;cpu;contract;core"
)

artifact_add_test(ArtifactCoreSourceInterpretContractTest
    SOURCES
        SourceInterpretContractTest.cpp
    LIBS
        ArtifactCoreMedia
)
set_tests_properties(ArtifactCoreSourceInterpretContractTest PROPERTIES
    LABELS "media;source-interpret;impact;cpu;contract;core"
)

artifact_add_test(ArtifactCoreBroadcastColorsContractTest
    SOURCES
        BroadcastColorsContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreBroadcastColorsContractTest PROPERTIES
    LABELS "image;broadcast-colors;color;cpu;contract;core"
)

artifact_add_test(ArtifactCoreThresholdEffectContractTest
    SOURCES
        ThresholdEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreThresholdEffectContractTest PROPERTIES
    LABELS "image;threshold;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreSimpleChokerContractTest
    SOURCES
        SimpleChokerContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreSimpleChokerContractTest PROPERTIES
    LABELS "image;alpha;choker;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreScatterEffectContractTest
    SOURCES
        ScatterEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreScatterEffectContractTest PROPERTIES
    LABELS "image;scatter;determinism;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreLeaveColorEffectContractTest
    SOURCES
        LeaveColorEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreLeaveColorEffectContractTest PROPERTIES
    LABELS "image;color;leave-color;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreMotionTrailContractTest
    SOURCES
        MotionTrailContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreMotionTrailContractTest PROPERTIES
    LABELS "image;temporal;motion-trail;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreDuotoneEffectContractTest
    SOURCES
        DuotoneEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreDuotoneEffectContractTest PROPERTIES
    LABELS "image;color;duotone;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCorePosterizeTimeContractTest
    SOURCES
        PosterizeTimeContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePosterizeTimeContractTest PROPERTIES
    LABELS "image;temporal;posterize-time;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreStrobeLightContractTest
    SOURCES
        StrobeLightContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreStrobeLightContractTest PROPERTIES
    LABELS "image;temporal;strobe;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreEmbossEffectContractTest
    SOURCES
        EmbossEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreEmbossEffectContractTest PROPERTIES
    LABELS "image;emboss;convolution;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreMedianEffectContractTest
    SOURCES
        MedianEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreMedianEffectContractTest PROPERTIES
    LABELS "image;median;neighborhood;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreChromaSpreadEffectContractTest
    SOURCES
        ChromaSpreadEffectContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreChromaSpreadEffectContractTest PROPERTIES
    LABELS "image;chroma-spread;remap;opencv;cpu;contract;core"
)

artifact_add_test(ArtifactCoreAnamorphicFlareContractTest
    SOURCES
        AnamorphicFlareContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreAnamorphicFlareContractTest PROPERTIES
    LABELS "image;flare;anamorphic;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreChromaSpreadGlowContractTest
    SOURCES
        ChromaSpreadGlowContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreChromaSpreadGlowContractTest PROPERTIES
    LABELS "image;chroma-spread;glow;pixel-processing;cpu;contract;core"
)

artifact_add_test(ArtifactCoreAeShapeOperatorsContractTest
    SOURCES
        AeShapeOperatorsContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreAeShapeOperatorsContractTest PROPERTIES
    LABELS "shape;operators;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCoreTrimPathsContractTest
    SOURCES
        TrimPathsContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreTrimPathsContractTest PROPERTIES
    LABELS "shape;trim-paths;animation;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCoreRepeaterContractTest
    SOURCES
        RepeaterContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRepeaterContractTest PROPERTIES
    LABELS "shape;repeater;geometry;cpu;contract;core"
)

artifact_add_test(ArtifactCoreMeshImporterPlyTest
    SOURCES
        MeshImporterPlyTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreMeshImporterLasTest
    SOURCES
        MeshImporterLasTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCorePathTubeTrimTest
    SOURCES
        PathTubeTrimTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreProcedural3DQualityLodTest
    SOURCES
        Procedural3DQualityLodTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreProcedural3DQualityLodTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "procedural-3d;lod;mesh;cpu;contract;core"
)
artifact_add_test(ArtifactCorePreviewSettingsLodContractTest
    SOURCES
        PreviewSettingsLodContractTest.cpp
    LIBS
        ArtifactCorePreview
)
set_tests_properties(ArtifactCorePreviewSettingsLodContractTest PROPERTIES
    LABELS "preview;lod;cpu;contract;core"
)
artifact_add_test(ArtifactCoreMeshGeometryTest
    SOURCES
        MeshGeometryTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreMeshLodContractTest
    SOURCES
        MeshLodContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreMeshLodContractTest PROPERTIES
    LABELS "mesh;lod;cpu;contract;core"
)
artifact_add_test(ArtifactCoreBezierArcLengthTest
    SOURCES
        BezierArcLengthTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreKeyframeSplineTest
    SOURCES
        KeyframeSplineTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreKeyframeSplineTest PROPERTIES
    LABELS "animation;keyframe;cpu;contract"
)
artifact_add_test(ArtifactCoreKeyframePatternGeneratorTest
    SOURCES
        KeyframePatternGeneratorTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreKeyframePatternGeneratorTest PROPERTIES
    LABELS "animation;keyframe;cpu;contract"
)

artifact_add_test(ArtifactCoreTrajectoryKeyframeContractTest
    SOURCES
        TrajectoryKeyframeContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreTrajectoryKeyframeContractTest PROPERTIES
    LABELS "animation;trajectory;keyframes;cpu;contract;core"
)

artifact_add_test(ArtifactCoreEasingFunctionsTest
    SOURCES
        EasingFunctionsTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreEasingLabCurveTest
    SOURCES
        EasingLabCurveTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreMatteStackTest
    SOURCES
        MatteStackTest.cpp
    LIBS
        ArtifactCore
)
artifact_add_test(ArtifactCoreTaskSystemMattePoC
    SOURCES
        TaskSystemMattePoCTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreThreadPoolContractTest
    SOURCES
        ThreadPoolContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreThreadPoolContractTest PROPERTIES
    LABELS "core;thread-pool;concurrency;cpu;contract"
)

artifact_add_test(ArtifactCoreTextShapingTest
    SOURCES
        TextShapingTest.cpp
    LIBS
        ArtifactCoreText
)
set_tests_properties(ArtifactCoreTextShapingTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "animation;text;cpu;contract"
)
artifact_add_test(ArtifactCoreTextAnimatorContractTest
    SOURCES
        TextAnimatorContractTest.cpp
    LIBS
        ArtifactCoreText
)
set_tests_properties(ArtifactCoreTextAnimatorContractTest PROPERTIES
    LABELS "animation;text;cpu;contract"
)
artifact_add_test(ArtifactCoreTextGlyphRasterTest
    SOURCES
        TextGlyphRasterTest.cpp
    LIBS
        ArtifactCoreText
)
set_tests_properties(ArtifactCoreTextGlyphRasterTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "animation;text;raster;cpu;contract"
)
artifact_add_test(ArtifactCoreImageDistortionMapperTest
    SOURCES
        ImageDistortionMapperTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreImageDistortionMapperTest PROPERTIES
    LABELS "image;distortion;cpu;contract"
)

artifact_add_test(ArtifactCoreRenderImageContractTest
    SOURCES
        RenderImageContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRenderImageContractTest PROPERTIES
    LABELS "renderer;image;cpu;contract"
)

artifact_add_test(ArtifactCoreRenderPipelineContractTest
    SOURCES
        RenderPipelineContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRenderPipelineContractTest PROPERTIES
    LABELS "renderer;pipeline;cpu;contract"
)

artifact_add_test(ArtifactCorePointwiseEffectFusionContractTest
    SOURCES
        PointwiseEffectFusionContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePointwiseEffectFusionContractTest PROPERTIES
    LABELS "renderer;effects;cpu;contract"
)

artifact_add_test(ArtifactCoreRenderJobModelContractTest
    SOURCES
        RenderJobModelContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRenderJobModelContractTest PROPERTIES
    LABELS "render-manager;model;cpu;contract"
)

artifact_add_test(ArtifactCoreProgressAggregatorContractTest
    SOURCES
        ProgressAggregatorContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreProgressAggregatorContractTest PROPERTIES
    LABELS "render-farm;progress;threading;cpu;contract;core"
    TIMEOUT 30
)

artifact_add_test(ArtifactCoreAcousticGenerationTest
    SOURCES
        AcousticGenerationTest.cpp
    LIBS
        ArtifactCoreAcoustic
)
set_tests_properties(ArtifactCoreAcousticGenerationTest PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "audio;acoustic;cpu;generation;lod"
)

artifact_add_test(ArtifactCoreProjectRoundTripTest
    SOURCES
        ProjectRoundTripTest.cpp
    LIBS
        ArtifactCore
        ArtifactCoreComposition
        ArtifactCoreProject
)
set_tests_properties(ArtifactCoreProjectRoundTripTest PROPERTIES
    LABELS "project;serialization;cpu;contract"
)

artifact_add_test(ArtifactCoreCompositionTransformTimeContractTest
    SOURCES
        CompositionTransformTimeContractTest.cpp
    LIBS
        ArtifactCore
        ArtifactCoreAnalyze
        ArtifactCoreComposition
)
set_tests_properties(ArtifactCoreCompositionTransformTimeContractTest PROPERTIES
    LABELS "composition;transform;time;cpu;contract"
)

artifact_add_test(ArtifactCoreRenderGraphTest
    SOURCES
        RenderGraphTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreRenderGraphTest PROPERTIES
    LABELS "graphics;render-graph;cpu;contract"
)

artifact_add_test(ArtifactCoreSoftBodyLodContractTest
    SOURCES
        SoftBodyLodContractTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreSoftBodyLodContractTest PROPERTIES
    LABELS "physics;soft-body;lod;cpu;contract;core"
)

artifact_add_test(ArtifactCoreBoidsDistanceLodContractTest
    SOURCES
        BoidsDistanceLodContractTest.cpp
    LIBS
        ArtifactCoreCrowd
)
set_tests_properties(ArtifactCoreBoidsDistanceLodContractTest PROPERTIES
    LABELS "crowd;boids;lod;cpu;contract;core"
)

# ---------------------------------------------------------------------------
# Video transition render contract + standalone MCP lab.
#
# Same shape as the FxStudio FX animation test: the contract executable renders
# every registered Video.Transitions.* over a deterministic A/B pair and writes
# PNG strips, per-progress grids and a behaviour report; the MCP host exposes the
# same render path as transition.* tools so a transition can be driven and
# inspected without the NLE editor.
#
# Linking ArtifactCoreVideo is not sufficient: its transition implementation
# units only contain static registrars, so a static-library link drops them. The
# test therefore compiles the needed Video module set and the implementation
# units into the executable itself, and each transition also exposes an explicit
# register<T>Transition() hook that the lab calls, because MSVC does not run
# implementation-unit initializers reliably for a library consumer.
# ---------------------------------------------------------------------------
if(ARTIFACT_BUILD_TESTS OR ARTIFACT_BUILD_TRANSITION_TEST)
    # Compile the module set the transitions need directly into each test
    # executable, exactly like the FxStudio test compiles its module units
    # instead of linking ArtifactAppRuntime.  (A static library is not enough:
    # lld-link also rejects CMake's CXXMODWEAKSYM archive member for a module
    # interface library.)
    set(ARTIFACT_TRANSITION_TEST_MODULES
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/VideoFrame.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/AbstractTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/CpuFrameView.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/TransitionFactory.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/BlockDissolveTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/CrossfadeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/CubeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/DissolveTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/DoorsTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/FlipTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/GlitchDisplaceTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/GradientWipeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/IrisWipeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/LightLeakTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/LinearWipeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/RadialWipeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/SlideTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/SpinTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/WipeTransition.ixx"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include/Video/Transitions/ZoomTransition.ixx"
    )
    set(ARTIFACT_TRANSITION_TEST_IMPL
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/TransitionFactory.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/BlockDissolveTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/CrossfadeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/CubeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/DissolveTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/DoorsTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/FlipTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/GlitchDisplaceTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/GradientWipeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/IrisWipeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/LightLeakTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/LinearWipeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/RadialWipeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/SlideTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/SpinTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/WipeTransition.cpp"
        "${PROJECT_SOURCE_DIR}/ArtifactCore/src/Video/Transitions/ZoomTransition.cpp"
    )
    add_library(ArtifactTransitionTestModules STATIC)
    target_sources(ArtifactTransitionTestModules
        PRIVATE ${ARTIFACT_TRANSITION_TEST_IMPL}
        PUBLIC FILE_SET CXX_MODULES
            BASE_DIRS "${PROJECT_SOURCE_DIR}/ArtifactCore/include"
            FILES ${ARTIFACT_TRANSITION_TEST_MODULES}
    )
    target_link_libraries(ArtifactTransitionTestModules
        PUBLIC ArtifactCoreNLE Qt6::Core Qt6::Gui Qt6::Widgets)
    target_include_directories(ArtifactTransitionTestModules PRIVATE
        "${PROJECT_SOURCE_DIR}/ArtifactCore/include"
        "${PROJECT_SOURCE_DIR}/ArtifactCore"
        "${PROJECT_SOURCE_DIR}/third_party"
    )
    set_target_properties(ArtifactTransitionTestModules PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactTransitionTestModules PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /MP /WX-
        )
    endif()

    function(artifact_transition_test_sources target)
        target_sources(${target}
            PRIVATE ${ARTIFACT_TRANSITION_TEST_IMPL}
            PUBLIC FILE_SET CXX_MODULES
                BASE_DIRS "${PROJECT_SOURCE_DIR}/ArtifactCore/include"
                FILES ${ARTIFACT_TRANSITION_TEST_MODULES}
        )
        target_link_libraries(${target} PRIVATE
            ArtifactCoreNLE Qt6::Core Qt6::Gui Qt6::Widgets)
        target_include_directories(${target} PRIVATE
            "${PROJECT_SOURCE_DIR}/ArtifactCore/include"
            "${PROJECT_SOURCE_DIR}/ArtifactCore"
            "${PROJECT_SOURCE_DIR}/third_party"
        )
        set_target_properties(${target} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
        )
        if(ARTIFACT_FRONTEND_MSVC)
            target_compile_options(${target} PRIVATE
                /Zf /Zc:__cplusplus /utf-8 /MP /WX-
            )
        endif()
    endfunction()

    artifact_add_test(ArtifactCoreTransitionRenderContractTest
        SOURCES
            TransitionRenderContractTest.cpp
    )
    artifact_transition_test_sources(ArtifactCoreTransitionRenderContractTest)
    if(WIN32)
        add_custom_command(TARGET ArtifactCoreTransitionRenderContractTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactCoreTransitionRenderContractTest>
                "$<TARGET_FILE_DIR:ArtifactCoreTransitionRenderContractTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    set_tests_properties(ArtifactCoreTransitionRenderContractTest PROPERTIES
        LABELS "video;transition;image;cpu;standalone;contract;core"
        TIMEOUT 180
        RUN_SERIAL TRUE
        ENVIRONMENT "ARTIFACT_TRANSITION_CAPTURE_DIR=${CMAKE_BINARY_DIR}/artifacts/transitions"
        # The copied Qt DLLs pull further vcpkg runtime DLLs (harfbuzz, pcre2,
        # ...) that the POST_BUILD copy does not enumerate; CTest starts the
        # process without the developer PATH, so add the vcpkg runtime dirs.
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin;PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin;PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
endif()

if(ARTIFACT_BUILD_TRANSITION_TEST)
    add_executable(ArtifactTransitionMcpTest
        "${CMAKE_CURRENT_SOURCE_DIR}/TransitionMcpMain.cpp"
    )
    artifact_transition_test_sources(ArtifactTransitionMcpTest)
    target_include_directories(ArtifactTransitionMcpTest PRIVATE
        "${PROJECT_SOURCE_DIR}/tests"
    )
    set_target_properties(ArtifactTransitionMcpTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactTransitionMcpTest PRIVATE
            /Zf /Zc:__cplusplus /utf-8 /MP /WX-
        )
    endif()
    if(WIN32)
        add_custom_command(TARGET ArtifactTransitionMcpTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactTransitionMcpTest>
                "$<TARGET_FILE_DIR:ArtifactTransitionMcpTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    add_test(NAME ArtifactTransitionMcpSelfCheck
        COMMAND ArtifactTransitionMcpTest --self-check
    )
    set_tests_properties(ArtifactTransitionMcpSelfCheck PROPERTIES
        LABELS "video;transition;image;mcp;cpu;standalone;contract"
        TIMEOUT 300
        RUN_SERIAL TRUE
        ENVIRONMENT "ARTIFACT_TRANSITION_CAPTURE_DIR=${CMAKE_BINARY_DIR}/artifacts/transitions/mcp"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin;PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    )
endif()
