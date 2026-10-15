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

if(ARTIFACT_ENABLE_FBX_IMPORT_TEST)
    set(_artifact_fbx_animation_fixture "${ARTIFACT_FBX_ANIMATION_TEST_FILE}")
    if(NOT _artifact_fbx_animation_fixture)
        set(_artifact_fbx_animation_fixture "${ARTIFACT_FBX_TEST_FILE}")
    endif()
    set(_artifact_fbx_texture_fixture "${ARTIFACT_FBX_TEXTURE_TEST_FILE}")
    if(NOT _artifact_fbx_texture_fixture)
        set(_artifact_fbx_texture_fixture "${ARTIFACT_FBX_TEST_FILE}")
    endif()
    set(_artifact_fbx_base_color_fixture "${ARTIFACT_FBX_BASE_COLOR_TEST_FILE}")
    if(NOT _artifact_fbx_base_color_fixture)
        set(_artifact_fbx_base_color_fixture "${_artifact_fbx_texture_fixture}")
    endif()
    if(NOT _artifact_fbx_animation_fixture OR
       NOT _artifact_fbx_base_color_fixture OR
       NOT _artifact_fbx_texture_fixture)
        message(FATAL_ERROR
            "ARTIFACT_ENABLE_FBX_IMPORT_TEST requires animation, embedded "
            "base-color, and embedded base-color/normal FBX fixtures. Set "
            "ARTIFACT_FBX_TEST_FILE, or set ARTIFACT_FBX_ANIMATION_TEST_FILE, "
            "ARTIFACT_FBX_BASE_COLOR_TEST_FILE, and "
            "ARTIFACT_FBX_TEXTURE_TEST_FILE as needed.")
    endif()
    get_filename_component(_artifact_fbx_animation_fixture
        "${_artifact_fbx_animation_fixture}" ABSOLUTE
        BASE_DIR "${PROJECT_SOURCE_DIR}")
    get_filename_component(_artifact_fbx_texture_fixture
        "${_artifact_fbx_texture_fixture}" ABSOLUTE
        BASE_DIR "${PROJECT_SOURCE_DIR}")
    get_filename_component(_artifact_fbx_base_color_fixture
        "${_artifact_fbx_base_color_fixture}" ABSOLUTE
        BASE_DIR "${PROJECT_SOURCE_DIR}")
    set(_artifact_fbx_external_texture_fixture
        "${CMAKE_CURRENT_LIST_DIR}/fixtures/fbx-materials/external/FBX/stale-source-path.fbx")

    foreach(_artifact_fbx_fixture IN ITEMS
            _artifact_fbx_animation_fixture _artifact_fbx_base_color_fixture
            _artifact_fbx_texture_fixture)
        if(NOT EXISTS "${${_artifact_fbx_fixture}}" OR
           IS_DIRECTORY "${${_artifact_fbx_fixture}}")
            message(FATAL_ERROR
                "FBX test fixture does not exist or is not a file: "
                "${${_artifact_fbx_fixture}}")
        endif()
    endforeach()
    if(NOT EXISTS "${_artifact_fbx_external_texture_fixture}" OR
       IS_DIRECTORY "${_artifact_fbx_external_texture_fixture}")
        message(FATAL_ERROR
            "External FBX texture-path regression fixture does not exist: "
            "${_artifact_fbx_external_texture_fixture}")
    endif()

    artifact_add_test(ArtifactCoreMeshImporterFbxTest
        SOURCES
            ArtifactCore/MeshImporterFbxTest.cpp
        LIBS
            ArtifactCore
    )
    if(WIN32)
        add_custom_command(TARGET ArtifactCoreMeshImporterFbxTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ArtifactCoreMeshImporterFbxTest>
                "$<TARGET_FILE_DIR:ArtifactCoreMeshImporterFbxTest>/"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
    set_tests_properties(ArtifactCoreMeshImporterFbxTest PROPERTIES
        LABELS "fbx;animation;textures;import;core"
        TIMEOUT 120
        ENVIRONMENT
            "ARTIFACT_FBX_ANIMATION_TEST_FILE=${_artifact_fbx_animation_fixture};ARTIFACT_FBX_BASE_COLOR_TEST_FILE=${_artifact_fbx_base_color_fixture};ARTIFACT_FBX_TEXTURE_TEST_FILE=${_artifact_fbx_texture_fixture};ARTIFACT_FBX_EXTERNAL_TEXTURE_TEST_FILE=${_artifact_fbx_external_texture_fixture}"
    )
endif()

# ArtifactCore tests, including the isolated MotionTracker video test.
if(ARTIFACT_BUILD_TESTS OR ARTIFACT_ENABLE_FBX_IMPORT_TEST
   OR ARTIFACT_ENABLE_TRACKING_VIDEO_TEST
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
   OR ARTIFACT_ENABLE_OFFLINE_RENDER_TEST OR ARTIFACT_ENABLE_LAYER_TEST)
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
