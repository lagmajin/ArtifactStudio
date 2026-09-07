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
