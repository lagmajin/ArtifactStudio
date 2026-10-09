if(NOT TARGET ArtifactRenderer)
    message(FATAL_ERROR "ArtifactRenderer CLI target is required for its contract tests")
endif()

add_test(NAME ArtifactRendererCliContractTest
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/test_cli.py"
        "$<TARGET_FILE:ArtifactRenderer>"
        "${CMAKE_SOURCE_DIR}/ArtifactRenderer/examples/minimal-job.json"
)
set_tests_properties(ArtifactRendererCliContractTest PROPERTIES
    LABELS "renderer;cli;cpu;contract"
    TIMEOUT 30
)
