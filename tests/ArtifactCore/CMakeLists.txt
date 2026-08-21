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
        ArtifactCore
)

artifact_add_test(ArtifactCoreAudioMixerRoutingTest
    SOURCES
        AudioMixerRoutingTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(ArtifactCoreSourceResolutionContractTest
    SOURCES
        SourceResolutionContractTest.cpp
    LIBS
        ArtifactCore
)
