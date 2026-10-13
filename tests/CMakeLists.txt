# Artifact Suite Test Configuration

enable_testing()

# Common test settings
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

function(artifact_add_test target_name)
    cmake_parse_arguments(AAT "" "" "SOURCES;LIBS" ${ARGN})
    
    add_executable(${target_name} ${AAT_SOURCES})
    
    target_link_libraries(${target_name}
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            Qt6::Core
            Qt6::Widgets
            ${AAT_LIBS}
    )
    
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(${target_name} PRIVATE /Zf /Zc:__cplusplus /utf-8 /MP)
    endif()
    
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# ArtifactCore tests, including the isolated MotionTracker video test.
if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_TRACKING_VIDEO_TEST
   OR ARTIFACT_ENABLE_KEYING_VIDEO_TEST OR ARTIFACT_ENABLE_RAY_TRACING_TEST)
    add_subdirectory(ArtifactCore)
endif()

# Keep the broad CPU creative-effect contract suite independently selectable
# without registering the rest of ArtifactCore's tests.
if(ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS AND NOT ARTIFACT_BUILD_TESTS)
    artifact_add_test(ArtifactCoreCreativeEffectTest
        SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/ArtifactCore/ColorVibranceEffectTest.cpp"
        LIBS
            ArtifactCoreVideo
    )
    set_tests_properties(ArtifactCoreCreativeEffectTest PROPERTIES
        LABELS "effect;image-effect;cpu;contract;core"
    )
endif()

if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS
   OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST)
    add_subdirectory(Artifact)
endif()

if(ARTIFACT_BUILD_TESTS)
    # Standalone visual contract (own main(), no GTest): link the runtime
    # OBJECT library, never the Artifact executable.
    add_executable(ArtifactMaskVisualTest
        "${CMAKE_CURRENT_SOURCE_DIR}/mask_visual/MaskVisualTest.cpp"
    )
    target_link_libraries(ArtifactMaskVisualTest
        PRIVATE
            ArtifactAppRuntime
            ${OpenCV_LIBS}
    )
    target_include_directories(ArtifactMaskVisualTest PRIVATE ${OpenCV_INCLUDE_DIRS})
    set_target_properties(ArtifactMaskVisualTest PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(ArtifactMaskVisualTest PRIVATE /Zf /Zc:__cplusplus /utf-8 /MP)
    endif()
    add_test(NAME ArtifactMaskVisualTest
        COMMAND ArtifactMaskVisualTest "${CMAKE_BINARY_DIR}/mask-visual-output/$<CONFIG>"
    )
    set_tests_properties(ArtifactMaskVisualTest PROPERTIES
        LABELS "mask;image;cpu;visual;contract"
    )
endif()

# ArtifactWidgets tests (later)
# add_subdirectory(ArtifactWidgets)
