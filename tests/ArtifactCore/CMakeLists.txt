# ArtifactCore Tests

artifact_add_test(ArtifactCoreTest
    SOURCES
        UtilsTest.cpp
    LIBS
        ArtifactCore
)

artifact_add_test(HueSaturationTest
    SOURCES
        Color/HueSaturationTest.cpp
    LIBS
        ArtifactCore
)
