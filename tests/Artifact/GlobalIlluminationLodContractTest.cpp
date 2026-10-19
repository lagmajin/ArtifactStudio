#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <RenderDevice.h>
#include <Texture.h>

#include "DiligentHeadlessRenderer.hpp"

import Artifact.Render.IRenderer;
import Artifact.Render.Pipeline;

TEST(GlobalIlluminationLodContractTest,
     RecommendedQualityBudgetsIncreaseWithQuality)
{
    const auto performance = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(
            Artifact::GlobalIlluminationQuality::Performance);
    const auto balanced = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(
            Artifact::GlobalIlluminationQuality::Balanced);
    const auto quality = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(
            Artifact::GlobalIlluminationQuality::Quality);

    const std::array<float, 3> resolutionScales{
        performance.ssgiResolutionScale,
        balanced.ssgiResolutionScale,
        quality.ssgiResolutionScale};
    const std::array<unsigned int, 3> raySteps{
        performance.ssgiRaySteps,
        balanced.ssgiRaySteps,
        quality.ssgiRaySteps};
    const std::array<unsigned int, 3> raysPerProbe{
        performance.ddgiRaysPerProbe,
        balanced.ddgiRaysPerProbe,
        quality.ddgiRaysPerProbe};
    const std::array<unsigned int, 3> probeBudgets{
        performance.ddgiProbeUpdateBudget,
        balanced.ddgiProbeUpdateBudget,
        quality.ddgiProbeUpdateBudget};
    const std::array<float, 3> targetGpuTimes{
        performance.targetGpuTimeMs,
        balanced.targetGpuTimeMs,
        quality.targetGpuTimeMs};

    EXPECT_FLOAT_EQ(resolutionScales[0], 0.25f);
    EXPECT_FLOAT_EQ(resolutionScales[1], 0.5f);
    EXPECT_FLOAT_EQ(resolutionScales[2], 0.5f);
    EXPECT_LT(resolutionScales[0], resolutionScales[1]);
    for (std::size_t index = 1; index < resolutionScales.size(); ++index) {
        EXPECT_LE(resolutionScales[index - 1], resolutionScales[index]);
        EXPECT_LT(raySteps[index - 1], raySteps[index]);
        EXPECT_LT(raysPerProbe[index - 1], raysPerProbe[index]);
        EXPECT_LT(probeBudgets[index - 1], probeBudgets[index]);
        EXPECT_LT(targetGpuTimes[index - 1], targetGpuTimes[index]);
    }
    EXPECT_EQ(raysPerProbe, (std::array<unsigned int, 3>{24u, 48u, 96u}));
    EXPECT_EQ(raySteps, (std::array<unsigned int, 3>{6u, 10u, 18u}));
    EXPECT_EQ(probeBudgets,
              (std::array<unsigned int, 3>{64u, 128u, 256u}));
    EXPECT_EQ(targetGpuTimes, (std::array<float, 3>{1.5f, 3.0f, 6.0f}));
}

TEST(GlobalIlluminationLodContractTest,
     UnknownQualityKeepsFiniteBalancedBudgetDefaults)
{
    constexpr auto unknownQuality =
        static_cast<Artifact::GlobalIlluminationQuality>(99);
    const auto settings = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(unknownQuality);

    EXPECT_TRUE(settings.enabled);
    EXPECT_EQ(settings.mode, Artifact::GlobalIlluminationMode::Auto);
    EXPECT_EQ(settings.quality, unknownQuality);
    EXPECT_TRUE(std::isfinite(settings.targetGpuTimeMs));
    EXPECT_TRUE(std::isfinite(settings.ssgiResolutionScale));
    EXPECT_FLOAT_EQ(settings.targetGpuTimeMs, 3.0f);
    EXPECT_FLOAT_EQ(settings.ssgiResolutionScale, 0.5f);
    EXPECT_EQ(settings.ssgiRaySteps, 10u);
    EXPECT_EQ(settings.ddgiRaysPerProbe, 48u);
    EXPECT_EQ(settings.ddgiProbeUpdateBudget, 128u);
}

TEST(GlobalIlluminationLodContractTest,
     RendererClampsSSGIResolutionLODAndRestoresFullResolution)
{
    Artifact::ArtifactIRenderer renderer;
    auto settings = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(
            Artifact::GlobalIlluminationQuality::Balanced);

    settings.ssgiResolutionScale = 0.0f;
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    0.25f);

    settings.ssgiResolutionScale = 0.75f;
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    0.75f);

    settings.ssgiResolutionScale = 2.0f;
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    1.0f);

    settings.ssgiResolutionScale = 0.25f;
    renderer.setGlobalIlluminationSettings(settings);
    settings.ssgiResolutionScale = 1.0f;
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    1.0f);
}

TEST(GlobalIlluminationLodContractTest,
     RendererKeepsNonFiniteSSGIResolutionLODWithinSupportedRange)
{
    Artifact::ArtifactIRenderer renderer;
    auto settings = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(
            Artifact::GlobalIlluminationQuality::Balanced);

    settings.ssgiResolutionScale = std::numeric_limits<float>::quiet_NaN();
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    0.25f);

    settings.ssgiResolutionScale = std::numeric_limits<float>::infinity();
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    1.0f);

    settings.ssgiResolutionScale = -std::numeric_limits<float>::infinity();
    renderer.setGlobalIlluminationSettings(settings);
    EXPECT_FLOAT_EQ(renderer.globalIlluminationSettings().ssgiResolutionScale,
                    0.25f);
}

TEST(GlobalIlluminationLodContractTest,
     SSGIOutputDimensionsTrackResolutionLODAndRestoreAfterUpscaling)
{
    constexpr int width = 63;
    constexpr int height = 47;
    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(width, height)) {
        GTEST_SKIP() << "No supported Diligent headless device is available";
    }

    const auto device = renderer.device();
    const auto context = renderer.immediateContext();
    ASSERT_TRUE(device);
    ASSERT_TRUE(context);

    Artifact::RenderPipeline pipeline;
    Artifact::RenderPipeline::AuxiliaryTargetRequest auxiliaryTargets;
    auxiliaryTargets.normal = true;
    auxiliaryTargets.albedo = true;
    ASSERT_TRUE(pipeline.initialize(
        device.RawPtr(), width, height, Diligent::TEX_FORMAT_RGBA16_FLOAT,
        auxiliaryTargets));
    void* depthTexture = renderer.createOffscreenDepthTexture(width, height);
    ASSERT_NE(depthTexture, nullptr);
    struct Cleanup final {
        Artifact::RenderPipeline& pipeline;
        ArtifactTest::HeadlessRenderer& renderer;
        void* depthTexture;
        ~Cleanup()
        {
            pipeline.destroy();
            renderer.destroyOffscreenTexture(depthTexture);
        }
    } cleanup{pipeline, renderer, depthTexture};

    Artifact::GlobalIlluminationInputs inputs;
    inputs.depth = renderer.offscreenTextureShaderResourceView(depthTexture);
    inputs.normal = pipeline.normalSRV();
    inputs.albedo = pipeline.albedoSRV();
    ASSERT_NE(inputs.depth, nullptr);
    ASSERT_NE(inputs.normal, nullptr);
    ASSERT_NE(inputs.albedo, nullptr);

    constexpr std::array<float, 4> lodScales{0.25f, 0.5f, 1.0f, 0.25f};
    constexpr std::array<Diligent::Uint32, 4> expectedWidths{
        16u, 32u, 63u, 16u};
    constexpr std::array<Diligent::Uint32, 4> expectedHeights{
        12u, 24u, 47u, 12u};
    for (std::size_t index = 0; index < lodScales.size(); ++index) {
        ASSERT_TRUE(pipeline.dispatchScreenSpaceGlobalIllumination(
            context.RawPtr(), inputs, lodScales[index], 8, 1.0f, 0.01f,
            false, false))
            << "SSGI dispatch failed at resolution scale " << lodScales[index];
        auto* output = pipeline.screenSpaceGlobalIlluminationSRV();
        ASSERT_NE(output, nullptr);
        ASSERT_NE(output->GetTexture(), nullptr);
        const auto& outputDesc = output->GetTexture()->GetDesc();
        EXPECT_EQ(outputDesc.Width, expectedWidths[index]);
        EXPECT_EQ(outputDesc.Height, expectedHeights[index]);
    }
}

TEST(GlobalIlluminationLodContractTest,
     RendererClampsEveryQualityBudgetAtBothSupportedEndpoints)
{
    Artifact::ArtifactIRenderer renderer;
    auto settings = Artifact::ArtifactIRenderer::
        recommendedGlobalIlluminationSettings(
            Artifact::GlobalIlluminationQuality::Balanced);

    settings.targetGpuTimeMs = 0.0f;
    settings.ssgiResolutionScale = 0.0f;
    settings.ssgiRaySteps = 0;
    settings.ddgiRaysPerProbe = 0;
    settings.ddgiProbeUpdateBudget = 0;
    renderer.setGlobalIlluminationSettings(settings);
    auto actual = renderer.globalIlluminationSettings();
    EXPECT_FLOAT_EQ(actual.targetGpuTimeMs, 0.5f);
    EXPECT_FLOAT_EQ(actual.ssgiResolutionScale, 0.25f);
    EXPECT_EQ(actual.ssgiRaySteps, 4u);
    EXPECT_EQ(actual.ddgiRaysPerProbe, 16u);
    EXPECT_EQ(actual.ddgiProbeUpdateBudget, 1u);

    settings.targetGpuTimeMs = 100.0f;
    settings.ssgiResolutionScale = 2.0f;
    settings.ssgiRaySteps = 1000;
    settings.ddgiRaysPerProbe = 1000;
    settings.ddgiProbeUpdateBudget = 100000;
    renderer.setGlobalIlluminationSettings(settings);
    actual = renderer.globalIlluminationSettings();
    EXPECT_FLOAT_EQ(actual.targetGpuTimeMs, 16.0f);
    EXPECT_FLOAT_EQ(actual.ssgiResolutionScale, 1.0f);
    EXPECT_EQ(actual.ssgiRaySteps, 128u);
    EXPECT_EQ(actual.ddgiRaysPerProbe, 512u);
    EXPECT_EQ(actual.ddgiProbeUpdateBudget, 4096u);
}

TEST(GlobalIlluminationLodContractTest,
     AutomaticModeSelectsQualityPathAndReportsCapabilityFallback)
{
    Artifact::ArtifactIRenderer renderer;
    constexpr std::array<Artifact::GlobalIlluminationQuality, 3> qualities{
        Artifact::GlobalIlluminationQuality::Performance,
        Artifact::GlobalIlluminationQuality::Balanced,
        Artifact::GlobalIlluminationQuality::Quality};

    bool rayTracingSupportKnown = false;
    bool rayTracingSupported = false;
    for (const auto quality : qualities) {
        auto settings = Artifact::ArtifactIRenderer::
            recommendedGlobalIlluminationSettings(quality);
        settings.enabled = true;
        settings.mode = Artifact::GlobalIlluminationMode::Auto;
        renderer.setGlobalIlluminationSettings(settings);

        const auto state = renderer.globalIlluminationState();
        if (!rayTracingSupportKnown) {
            rayTracingSupported = state.rayTracingSupported;
            rayTracingSupportKnown = true;
        }
        EXPECT_EQ(state.rayTracingSupported, rayTracingSupported);
        EXPECT_EQ(state.quality, quality);

        if (quality == Artifact::GlobalIlluminationQuality::Performance ||
            !rayTracingSupported) {
            EXPECT_EQ(state.selectedMode, Artifact::GlobalIlluminationMode::SSGI);
        } else {
            EXPECT_EQ(state.selectedMode, Artifact::GlobalIlluminationMode::Hybrid);
        }
        EXPECT_EQ(state.usingFallback,
                  !rayTracingSupported &&
                      quality != Artifact::GlobalIlluminationQuality::Performance);
    }
}

TEST(GlobalIlluminationLodContractTest,
     ExplicitModeRemainsAuthoritativeAcrossQualityLevels)
{
    Artifact::ArtifactIRenderer renderer;
    constexpr std::array<Artifact::GlobalIlluminationQuality, 3> qualities{
        Artifact::GlobalIlluminationQuality::Performance,
        Artifact::GlobalIlluminationQuality::Balanced,
        Artifact::GlobalIlluminationQuality::Quality};
    constexpr std::array<Artifact::GlobalIlluminationMode, 4> modes{
        Artifact::GlobalIlluminationMode::Off,
        Artifact::GlobalIlluminationMode::SSGI,
        Artifact::GlobalIlluminationMode::DDGI,
        Artifact::GlobalIlluminationMode::Hybrid};

    for (const auto mode : modes) {
        Artifact::GlobalIlluminationMode expectedMode = mode;
        bool expectedFallback = false;
        for (const auto quality : qualities) {
            auto settings = Artifact::ArtifactIRenderer::
                recommendedGlobalIlluminationSettings(quality);
            settings.enabled = true;
            settings.mode = mode;
            renderer.setGlobalIlluminationSettings(settings);

            const auto state = renderer.globalIlluminationState();
            if ((mode == Artifact::GlobalIlluminationMode::DDGI ||
                 mode == Artifact::GlobalIlluminationMode::Hybrid) &&
                !state.rayTracingSupported) {
                expectedMode = Artifact::GlobalIlluminationMode::SSGI;
                expectedFallback = true;
            } else {
                expectedMode = mode;
                expectedFallback = false;
            }
            EXPECT_EQ(state.quality, quality);
            EXPECT_EQ(state.requestedMode, mode);
            EXPECT_EQ(state.selectedMode, expectedMode);
            EXPECT_EQ(state.usingFallback, expectedFallback);
        }
    }
}

TEST(GlobalIlluminationLodContractTest,
     DisablingGIStaysOffAcrossQualityChangesAndRestoresAutoSelection)
{
    Artifact::ArtifactIRenderer renderer;
    constexpr std::array<Artifact::GlobalIlluminationQuality, 3> qualities{
        Artifact::GlobalIlluminationQuality::Performance,
        Artifact::GlobalIlluminationQuality::Balanced,
        Artifact::GlobalIlluminationQuality::Quality};

    for (const auto quality : qualities) {
        auto settings = Artifact::ArtifactIRenderer::
            recommendedGlobalIlluminationSettings(quality);
        settings.enabled = false;
        settings.mode = Artifact::GlobalIlluminationMode::Auto;
        renderer.setGlobalIlluminationSettings(settings);

        const auto disabledState = renderer.globalIlluminationState();
        EXPECT_EQ(disabledState.requestedMode,
                  Artifact::GlobalIlluminationMode::Auto);
        EXPECT_EQ(disabledState.quality, quality);
        EXPECT_EQ(disabledState.selectedMode,
                  Artifact::GlobalIlluminationMode::Off)
            << "changing GI quality must not override the user's disabled state";
        EXPECT_FALSE(disabledState.usingFallback);

        settings.enabled = true;
        renderer.setGlobalIlluminationSettings(settings);
        const auto enabledState = renderer.globalIlluminationState();
        const auto expectedMode =
            quality == Artifact::GlobalIlluminationQuality::Performance ||
                    !enabledState.rayTracingSupported
                ? Artifact::GlobalIlluminationMode::SSGI
                : Artifact::GlobalIlluminationMode::Hybrid;
        EXPECT_EQ(enabledState.selectedMode, expectedMode)
            << "re-enabling GI should restore the quality-driven Auto path";
    }
}
