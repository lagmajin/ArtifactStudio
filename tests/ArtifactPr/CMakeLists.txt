# ArtifactPr contract tests.
#
# These link the ArtifactPrApp static library, so the suite is only registered
# when the ArtifactPr application target exists (ARTIFACT_BUILD_PR=ON) and
# GTest-based tests are enabled (ARTIFACT_BUILD_TESTS=ON or
# ARTIFACT_BUILD_PR_TESTS=ON). See tests/CMakeLists.txt for the guard.
#
# Coverage is limited to the parts of ArtifactPr that need neither a GPU device
# nor a real media file. Cases that require a live Diligent device
# (GpuProgramMonitor) or actual decoding (MediaFrameDecoder, exportSequence,
# renderSequenceAudio beyond argument validation) stay out of this suite.

artifact_add_test(ArtifactPrContractTest
    SOURCES
        ArtifactPrContractTest.cpp
    LIBS
        ArtifactPrApp
)
set_tests_properties(ArtifactPrContractTest PROPERTIES
    LABELS "artifactpr;effect;compositor;timecode;shortcut;cpu;standalone;contract"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    RUN_SERIAL TRUE
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactPrContractTest PRIVATE /WX-)
endif()

artifact_add_test(ArtifactPrEditorEngineTest
    SOURCES
        ArtifactPrEditorEngineTest.cpp
    LIBS
        ArtifactPrApp
)
set_tests_properties(ArtifactPrEditorEngineTest PROPERTIES
    LABELS "artifactpr;nle;editor-engine;render-plan;snap;project;ui;standalone;contract"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    RUN_SERIAL TRUE
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactPrEditorEngineTest PRIVATE /WX-)
endif()

artifact_add_test(ArtifactPrTimelinePanelTest
    SOURCES
        ArtifactPrTimelinePanelTest.cpp
    LIBS
        ArtifactPrApp
)
set_tests_properties(ArtifactPrTimelinePanelTest PROPERTIES
    LABELS "artifactpr;timeline;ui;standalone;contract"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    RUN_SERIAL TRUE
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactPrTimelinePanelTest PRIVATE /WX-)
endif()
