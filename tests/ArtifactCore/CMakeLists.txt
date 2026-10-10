# ArtifactCore Tests

artifact_add_test(ArtifactCoreTest
    SOURCES
        UtilsTest.cpp
    LIBS
        ArtifactCore
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

artifact_add_test(ArtifactCoreColorConversionTest
    SOURCES
        ColorConversionTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreColorConversionTest PROPERTIES
    LABELS "color;cpu;contract"
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

artifact_add_test(ArtifactCoreImageSurfaceViewTest
    SOURCES
        ImageSurfaceViewTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCoreImageSurfaceViewTest PROPERTIES
    LABELS "image;surface;cpu;contract"
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

artifact_add_test(ArtifactCoreImageAnalyzerContractTest
    SOURCES
        ImageAnalyzerContractTest.cpp
    LIBS
        ArtifactCore
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

artifact_add_test(ArtifactCorePropertyPathTest
    SOURCES
        PropertyPathTest.cpp
    LIBS
        ArtifactCore
)
set_tests_properties(ArtifactCorePropertyPathTest PROPERTIES
    LABELS "property;path;cpu;contract"
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
        ArtifactCore
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

artifact_add_test(ArtifactCoreMathVecTest
    SOURCES
        MathVecTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreShapeExtrudeTest
    SOURCES
        ShapeExtrudeTest.cpp
    LIBS
        ArtifactCore
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
artifact_add_test(ArtifactCoreMeshGeometryTest
    SOURCES
        MeshGeometryTest.cpp
    LIBS
        ArtifactCore
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

artifact_add_test(ArtifactCoreAcousticGenerationTest
    SOURCES
        AcousticGenerationTest.cpp
    LIBS
        ArtifactCoreAcoustic
)
set_tests_properties(ArtifactCoreAcousticGenerationTest PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "audio;acoustic;cpu;generation"
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
