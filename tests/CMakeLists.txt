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

# ArtifactCore tests
if(ARTIFACT_BUILD_TESTS)
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

# ArtifactWidgets tests (later)
# add_subdirectory(ArtifactWidgets)
