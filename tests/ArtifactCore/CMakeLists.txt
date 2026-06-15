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
