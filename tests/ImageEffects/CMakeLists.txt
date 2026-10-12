cmake_minimum_required(VERSION 3.30)
project(ArtifactStudioImageEffectTests LANGUAGES CXX)

# Focused parent-repo test harness. It never configures or builds Artifact.exe.
set(ARTIFACT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS OFF CACHE BOOL "" FORCE)
set(ARTIFACT_ENABLE_OFFLINE_RENDER_TEST OFF CACHE BOOL "" FORCE)
set(ARTIFACT_ENABLE_LAYER_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../Artifact" ArtifactProduction)

# Artifact's top-level project creates application executables unconditionally.
# Keep them out of the default focused build while retaining their shared CMake
# target graph; explicitly requested test libraries remain buildable.
set_target_properties(Artifact ArtifactUiTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

find_package(GTest REQUIRED)

function(artifact_image_effect_test target source library)
    add_executable(${target} "${CMAKE_CURRENT_LIST_DIR}/../Artifact/${source}")
    target_link_libraries(${target} PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Widgets ${library})
    set_target_properties(${target} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "effect;image-effect;cpu;contract")
endfunction()

enable_testing()
artifact_image_effect_test(ArtifactExposureEffectContractTest ExposureEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactColorCorrectionEffectContractTest ColorCorrectionEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactVibranceEffectContractTest VibranceEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactBlackAndWhiteEffectContractTest BlackAndWhiteEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactColorTintEffectContractTest ColorTintEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactPosterizeEffectContractTest PosterizeEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactThresholdEffectContractTest ThresholdEffectContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactEffectSequenceContractTest EffectSequenceContractTest.cpp ArtifactEffectsColor)
artifact_image_effect_test(ArtifactSpatialImageEffectContractTest SpatialImageEffectContractTest.cpp ArtifactEffectsBlur)
