#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <QColor>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.LiftGammaGain;
import Artifact.Effect.Glow.ChromaticGlow;
import Artifact.Effect.Glow.EdgeBloom;
import Artifact.Effect.Glow.LiquidGlow;
import Artifact.Effect.Glow;
import Artifact.Effect.Kaleidoscope;
import Artifact.Effect.Rasterizer.AddNoise;
import Artifact.Effect.Rasterizer.Bricks;
import Artifact.Effect.Rasterizer.ChromaticAberration;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.Rasterizer.HexGrid;
import Artifact.Effect.Rasterizer.InnerShadow;
import Artifact.Effect.Rasterizer.Magnify;
import Artifact.Effect.Rasterizer.PinchBulge;
import Artifact.Effect.Rasterizer.PolarCoordinates;
import Artifact.Effect.Rasterizer.RadialBlur;
import Artifact.Effect.Rasterizer.Ripple;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.Rasterizer.Sharpen;
import Artifact.Effect.Rasterizer.TurbulentDisplace;
import Artifact.Effect.Rasterizer.Voronoi;
import Artifact.Effect.Rasterizer.Glow;
import Artifact.Effect.DirectionalGlow;
import ColorBalanceEffect;
import ColorWheelsEffect;
import CurvesEffect;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

void expectSingleSpatialNodeMask(Artifact::ArtifactAbstractEffect& effect,
                                 const std::uint8_t expectedMask)
{
    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, expectedMask);
}

ArtifactCore::ImageF32x4RGBAWithCache makeEffectTestSurface(
    const float* rgba, const int width, const int height)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(EffectParameterLodContractTest, BlurScalesOnlyItsSigmaParameter)
{
    Artifact::BlurEffect effect;
    effect.setRadius(4.0f);
    ASSERT_EQ(effect.gpuRasterEffectDomain(),
              Artifact::GpuRasterEffectDomain::Spatial);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 0);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.sigma());
}

TEST(EffectParameterLodContractTest,
     DropShadowScalesOffsetsAndSoftnessButKeepsColorAndOpacity)
{
    Artifact::DropShadowEffect effect;
    effect.setDistance(24.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(8.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1) | (1u << 2));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], -effect.distance());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 0.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.softness());
}

TEST(EffectParameterLodContractTest, RippleScalesAmplitudeOnly)
{
    Artifact::RippleEffect effect;
    effect.setAmplitude(16.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 2);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.amplitude());
}

TEST(EffectParameterLodContractTest, AddNoiseScalesNoiseSizeButNotAmount)
{
    Artifact::AddNoiseEffect effect;
    effect.setAmount(0.4f);
    effect.setSize(32.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 1);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.amount());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.size());
}

TEST(EffectParameterLodContractTest,
     TurbulentDisplaceScalesPixelAmountAndSizeOnly)
{
    Artifact::TurbulentDisplaceEffect effect;
    effect.setAmount(12.0f);
    effect.setSize(48.0f);
    effect.setOctaves(5);
    effect.setSeed(17);
    effect.setDomainWarp(0.6f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.amount());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.size());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 5.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 17.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.domainWarp());
}

TEST(EffectParameterLodContractTest, SharpenScalesSigmaButNotAmountOrThreshold)
{
    Artifact::SharpenEffect effect;
    effect.setAmount(2.5f);
    effect.setSigma(3.0f);
    effect.setThreshold(0.2f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 1);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.amount());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.sigma());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.threshold());
}

TEST(EffectParameterLodContractTest, VoronoiScalesCellSizeButNotPatternControls)
{
    Artifact::VoronoiEffect effect;
    effect.setScale(40.0f);
    effect.setJitter(0.35f);
    effect.setMode(2);
    effect.setSeed(19);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 0);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.scale());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.jitter());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 2.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 19.0f);
}

TEST(EffectParameterLodContractTest,
     DirectionalGlowScalesStreakLengthsButNotThresholdOrIntensity)
{
    Artifact::DirectionalGlowEffect effect;
    effect.setThreshold(0.65f);
    effect.setIntensity(1.7f);
    effect.setLength1(72.0f);
    effect.setLength2(144.0f);
    effect.setWeight1(0.3f);
    effect.setWeight2(0.7f);
    effect.setAngleOffset(18.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 2) | (1u << 3));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.threshold());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.intensity());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.length1());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.length2());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.weight1());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], effect.weight2());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[7], effect.angleOffset());
}

TEST(EffectParameterLodContractTest,
     InnerShadowScalesOffsetAndSoftnessButNotOpacityOrColor)
{
    Artifact::InnerShadowEffect effect;
    effect.setDistance(18.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(6.0f);
    effect.setOpacity(40.0f);
    effect.setShadowColor(QColor(20, 40, 60, 128));

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1) | (1u << 2));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], -effect.distance());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 0.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.softness());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3],
                    0.4f * effect.shadowColor().alphaF());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.shadowColor().redF());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], effect.shadowColor().greenF());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], effect.shadowColor().blueF());
}

TEST(EffectParameterLodContractTest,
     ChromaticAberrationScalesChannelShiftsButNotCenter)
{
    Artifact::ChromaticAberrationEffect effect;
    effect.setRedShift(5.0f);
    effect.setBlueShift(8.0f);
    effect.setCenterX(0.2f);
    effect.setCenterY(0.7f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.redShift());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.blueShift());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.centerX());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.centerY());
}

TEST(EffectParameterLodContractTest,
     HexGridScalesCellAndLineDimensionsButNotRotation)
{
    Artifact::HexGridEffect effect;
    effect.setCellSize(48.0f);
    effect.setLineWidth(3.0f);
    effect.setAngle(27.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.cellSize());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.lineWidth());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.angle());
}

TEST(EffectParameterLodContractTest,
     BricksScalesBrickAndMortarDimensionsButNotPatternOffset)
{
    Artifact::BricksEffect effect;
    effect.setBrickWidth(80.0f);
    effect.setBrickHeight(36.0f);
    effect.setMortarWidth(4.0f);
    effect.setOffset(0.35f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1) | (1u << 2));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.brickWidth());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.brickHeight());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.mortarWidth());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.offset());
}

TEST(EffectParameterLodContractTest,
     RasterizerGlowScalesRadiusButNotThresholdOrIntensity)
{
    Artifact::RasterizerGlowEffect effect;
    effect.setThreshold(0.7f);
    effect.setRadius(3.0f);
    effect.setIntensity(1.8f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 1);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.threshold());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.radius());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.intensity());
}

TEST(EffectParameterLodContractTest,
     ChromaticGlowScalesRadiusButNotColorOrDispersionControls)
{
    Artifact::ChromaticGlowEffect effect;
    effect.setThreshold(0.72f);
    effect.setRadius(9.0f);
    effect.setIntensity(1.6f);
    effect.setDispersion(0.8f);
    effect.setAngle(42.0f);
    effect.setTintMix(0.4f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 1);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.threshold());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.radius());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.intensity());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.dispersion());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.angle());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], effect.tintMix());
}

TEST(EffectParameterLodContractTest,
     EdgeBloomScalesRadiusButNotThresholdOrBloomStrength)
{
    Artifact::EdgeBloomEffect effect;
    effect.setThreshold(0.7f);
    effect.setThresholdSoftness(0.2f);
    effect.setRadius(7.0f);
    effect.setAmount(1.4f);
    effect.setEdgeBoost(2.1f);
    effect.setTintMix(0.45f);
    effect.setQuality(2);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 2);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.threshold());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.thresholdSoftness());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.radius());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.amount());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.edgeBoost());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], effect.tintMix());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 2.0f);
}

TEST(EffectParameterLodContractTest,
     EdgeBloomPreservesAllQualityLevelsAndClampsOutOfRangeValues)
{
    Artifact::EdgeBloomEffect effect;
    for (const int requestedQuality : {-5, 0, 1, 2, 9}) {
        effect.setQuality(requestedQuality);
        const int expectedQuality = std::clamp(requestedQuality, 0, 2);
        EXPECT_EQ(effect.quality(), expectedQuality);

        Artifact::GpuSpatialEffectStack stack;
        ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
        ASSERT_EQ(stack.count, 1u);
        EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 2);
        EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6],
                        static_cast<float>(expectedQuality));
    }
}

TEST(EffectParameterLodContractTest,
     RadialBlurSamplingQualityChangesOutputAndRestoresDeterministically)
{
    constexpr int width = 32;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = ((x / 2 + y / 2) % 2) == 0 ? 1.0f : 0.0f;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = (x % 3) == 0 ? 1.0f : 0.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeEffectTestSurface(pixels.data(), width, height);
    Artifact::RadialBlurEffect effect;
    effect.setAmount(1.2f);
    effect.setCenterX(0.37f);
    effect.setCenterY(0.61f);

    const auto renderAtQuality = [&](const int quality) {
        effect.setQuality(quality);
        ArtifactCore::ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };
    const auto differenceCount = [](const auto& first, const auto& second) {
        const float* a = first.image().rgba32fData();
        const float* b = second.image().rgba32fData();
        if (!a || !b || first.image().width() != second.image().width() ||
            first.image().height() != second.image().height()) {
            return -1;
        }
        int changed = 0;
        const std::size_t componentCount = static_cast<std::size_t>(
            first.image().width() * first.image().height() * 4);
        for (std::size_t index = 0; index < componentCount; ++index) {
            if (std::abs(a[index] - b[index]) > 1.0e-6f) ++changed;
        }
        return changed;
    };

    const auto lowQuality = renderAtQuality(2);
    const auto standardQuality = renderAtQuality(8);
    const auto highQuality = renderAtQuality(32);
    const auto restoredLowQuality = renderAtQuality(2);

    EXPECT_EQ(effect.quality(), 2);
    EXPECT_GT(differenceCount(lowQuality, standardQuality), 0)
        << "increasing the sample count should change the radial blur result";
    EXPECT_GT(differenceCount(standardQuality, highQuality), 0)
        << "the highest sample count should refine the radial blur result";
    EXPECT_EQ(differenceCount(lowQuality, restoredLowQuality), 0)
        << "returning to the same sampling quality should reproduce the output";

    effect.setQuality(-10);
    EXPECT_EQ(effect.quality(), 2);
    effect.setQuality(100);
    EXPECT_EQ(effect.quality(), 32);

    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("quality")),
                            QVariant(16));
    EXPECT_EQ(effect.quality(), 16)
        << "the effect property-edit path should update sampling quality";
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("quality")),
                            QVariant(-1));
    EXPECT_EQ(effect.quality(), 2);
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("quality")),
                            QVariant(100));
    EXPECT_EQ(effect.quality(), 32);

    const auto properties = effect.getProperties();
    const auto qualityProperty = std::find_if(
        properties.begin(), properties.end(), [](const auto& property) {
            return property.getName() == QStringLiteral("quality");
        });
    ASSERT_NE(qualityProperty, properties.end());
    EXPECT_EQ(qualityProperty->getType(), ArtifactCore::PropertyType::Integer);
    EXPECT_EQ(qualityProperty->getValue().toInt(), 32);
    EXPECT_EQ(qualityProperty->getMinValue().toInt(), 2);
    EXPECT_EQ(qualityProperty->getMaxValue().toInt(), 32);
}

TEST(EffectParameterLodContractTest,
     GlowQualityLayerCountChangesOutputAndRestoresDeterministically)
{
    constexpr int width = 32;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = (x >= 12 && x <= 19 && y >= 12 && y <= 19)
                ? 1.0f : 0.0f;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeEffectTestSurface(pixels.data(), width, height);
    Artifact::GlowEffect effect;
    effect.setGlowGain(2.0f);
    effect.setBaseSigma(2.0f);
    effect.setSigmaGrowth(2.0f);
    effect.setBaseAlpha(0.7f);
    effect.setAlphaFalloff(0.8f);
    effect.setContributionPreview(true);

    const auto renderAtLayerCount = [&](const int layerCount) {
        effect.setLayerCount(layerCount);
        ArtifactCore::ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        return output;
    };
    const auto differenceCount = [](const auto& first, const auto& second) {
        const float* a = first.image().rgba32fData();
        const float* b = second.image().rgba32fData();
        if (!a || !b || first.image().width() != second.image().width() ||
            first.image().height() != second.image().height()) {
            return -1;
        }
        int changed = 0;
        const std::size_t componentCount = static_cast<std::size_t>(
            first.image().width() * first.image().height() * 4);
        for (std::size_t index = 0; index < componentCount; ++index) {
            if (std::abs(a[index] - b[index]) > 1.0e-6f) ++changed;
        }
        return changed;
    };

    const auto oneLayer = renderAtLayerCount(1);
    const auto fourLayers = renderAtLayerCount(4);
    const auto eightLayers = renderAtLayerCount(8);
    const auto restoredOneLayer = renderAtLayerCount(1);

    EXPECT_EQ(effect.layerCount(), 1);
    EXPECT_GT(differenceCount(oneLayer, fourLayers), 0)
        << "adding glow layers should change the bloom contribution";
    EXPECT_GT(differenceCount(fourLayers, eightLayers), 0)
        << "additional glow layers should refine the bloom contribution";
    EXPECT_EQ(differenceCount(oneLayer, restoredOneLayer), 0)
        << "returning to one layer should reproduce the original output";

    effect.setLayerCount(0);
    EXPECT_EQ(effect.layerCount(), 1);
    effect.setLayerCount(100);
    EXPECT_EQ(effect.layerCount(), 16);
}

TEST(EffectParameterLodContractTest,
     LiquidGlowScalesRadiusButNotFlowOrDistortionControls)
{
    Artifact::LiquidGlowEffect effect;
    effect.setThreshold(0.6f);
    effect.setRadius(6.0f);
    effect.setIntensity(1.5f);
    effect.setFlowScale(50.0f);
    effect.setDistortion(5.0f);
    effect.setPhase(0.75f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 1);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.6f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 6.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 1.5f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 50.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], 5.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], 0.75f);
}

TEST(EffectParameterLodContractTest,
     GaussianBlurScalesSigmaForSpatialResolution)
{
    Artifact::GaussianBlur effect;
    effect.setSigma(7.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind,
              Artifact::GpuSpatialEffectKind::SeparableGaussianBlur);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 0);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.sigma());
}

TEST(EffectParameterLodContractTest,
     MultiLayerGlowScalesBaseSigmaButKeepsLayerAndAlphaControls)
{
    Artifact::GlowEffect effect;
    effect.setGlowGain(1.4f);
    effect.setLayerCount(5);
    effect.setBaseSigma(8.0f);
    effect.setSigmaGrowth(1.6f);
    effect.setBaseAlpha(0.7f);
    effect.setAlphaFalloff(0.8f);
    effect.setContributionPreview(true);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 2);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.glowGain());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 5.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.baseSigma());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.sigmaGrowth());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.baseAlpha());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], effect.alphaFalloff());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 1.0f);
}

TEST(EffectParameterLodContractTest,
     PolarCoordinatesKeepsNormalizedCenterAndRadiusUnscaled)
{
    Artifact::PolarCoordinatesEffect effect;
    effect.setCenterX(0.3f);
    effect.setCenterY(0.65f);
    effect.setRadius(0.4f);
    effect.setAmount(0.75f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 0u);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.centerX());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.centerY());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.radius());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.amount());
}

TEST(EffectParameterLodContractTest,
     PinchBulgeKeepsNormalizedCenterAndRadiusUnscaled)
{
    Artifact::PinchBulgeEffect effect;
    effect.setCenterX(0.25f);
    effect.setCenterY(0.8f);
    effect.setRadius(0.35f);
    effect.setAmount(-42.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 0u);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.centerX());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.centerY());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.radius());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.amount());
}

TEST(EffectParameterLodContractTest,
     MagnifyKeepsNormalizedCenterAndRadiusUnscaled)
{
    Artifact::MagnifyEffect effect;
    effect.setCenterX(0.7f);
    effect.setCenterY(0.2f);
    effect.setRadius(0.6f);
    effect.setAmount(63.0f);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 0u);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], effect.centerX());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.centerY());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.radius());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.amount());
}

TEST(EffectParameterLodContractTest,
     KaleidoscopeKeepsNormalizedGeometryAndPatternControlsUnscaled)
{
    Artifact::KaleidoscopeEffect effect;
    effect.setSegments(9);
    effect.setCenterX(0.35f);
    effect.setCenterY(0.6f);
    effect.setRotation(27.0f);
    effect.setZoom(1.4f);
    effect.setFeather(0.08f);
    effect.setMirror(false);

    Artifact::GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 0u);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 9.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], effect.centerX());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], effect.centerY());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], effect.rotation());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[4], effect.zoom());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[5], effect.feather());
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[6], 0.0f);
}

TEST(EffectParameterLodContractTest,
     ColorCorrectionNodesDoNotScaleColorControlsWithResolutionLOD)
{
    Artifact::LiftGammaGainEffect liftGammaGain;
    Artifact::ColorBalanceEffect colorBalance;
    Artifact::ColorWheelsEffect colorWheels;
    Artifact::CurvesEffect curves;

    expectSingleSpatialNodeMask(liftGammaGain, 0u);
    expectSingleSpatialNodeMask(colorBalance, 0u);
    expectSingleSpatialNodeMask(colorWheels, 0u);
    expectSingleSpatialNodeMask(curves, 0u);
}

TEST(EffectParameterLodContractTest,
     SpatialStackCapacityRejectsOverflowWithoutChangingStoredLODNodes)
{
    Artifact::GpuSpatialEffectStack stack;
    for (std::size_t index = 0;
         index < Artifact::GpuSpatialEffectStack::kCapacity; ++index) {
        Artifact::GpuSpatialEffectNode node;
        node.kind = Artifact::GpuSpatialEffectKind::Generic;
        node.genericKey = static_cast<std::uint32_t>(index + 1);
        node.parameters[0] = static_cast<float>(index) + 0.25f;
        node.resolutionScaledParameterMask =
            static_cast<std::uint8_t>(1u << (index % 8));
        ASSERT_TRUE(stack.append(node));
    }

    ASSERT_EQ(stack.count, Artifact::GpuSpatialEffectStack::kCapacity);
    const auto firstNode = stack.nodes[0];
    const auto lastNode = stack.nodes[Artifact::GpuSpatialEffectStack::kCapacity - 1];
    Artifact::GpuSpatialEffectNode overflow;
    overflow.kind = Artifact::GpuSpatialEffectKind::SeparableGaussianBlur;
    overflow.parameters[0] = 999.0f;
    overflow.resolutionScaledParameterMask = 1u;

    EXPECT_FALSE(stack.append(overflow));
    EXPECT_EQ(stack.count, Artifact::GpuSpatialEffectStack::kCapacity);
    EXPECT_EQ(stack.nodes[0].genericKey, firstNode.genericKey);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], firstNode.parameters[0]);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              firstNode.resolutionScaledParameterMask);
    EXPECT_EQ(stack.nodes[Artifact::GpuSpatialEffectStack::kCapacity - 1].genericKey,
              lastNode.genericKey);
    EXPECT_FLOAT_EQ(
        stack.nodes[Artifact::GpuSpatialEffectStack::kCapacity - 1].parameters[0],
        lastNode.parameters[0]);
    EXPECT_EQ(
        stack.nodes[Artifact::GpuSpatialEffectStack::kCapacity - 1]
            .resolutionScaledParameterMask,
        lastNode.resolutionScaledParameterMask);
}
