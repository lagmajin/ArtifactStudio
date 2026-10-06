# Tests for application-layer contracts that can run without starting Artifact.exe.
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

artifact_add_test(ArtifactTextRenderTargetContractTest
    SOURCES
        TextRenderTargetContractTest.cpp
    LIBS
        ArtifactTextRenderTargetRuntime
        ArtifactTextGlyphSubmitterRuntime
)
set_tests_properties(ArtifactTextRenderTargetContractTest PROPERTIES
    LABELS "renderer;gpu;headless"
)

artifact_add_test(ArtifactTextGlyphRenderContractTest
    SOURCES
        TextGlyphRenderContractTest.cpp
    LIBS
        ArtifactTextRenderTargetRuntime
        ArtifactTextGlyphSubmitterRuntime
)
set_tests_properties(ArtifactTextGlyphRenderContractTest PROPERTIES
    LABELS "renderer;gpu;text;headless;contract"
)
