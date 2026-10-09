#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QVariant>

import FloatRGBA;
import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import VibranceEffect;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

void expectPixelNear(const ImageF32x4RGBAWithCache& surface, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1.0e-6f)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

FloatRGBA expectedVibrance(const FloatRGBA& input, float vibrance,
                           float saturation)
{
    constexpr float kLumaR = 0.299f;
    constexpr float kLumaG = 0.587f;
    constexpr float kLumaB = 0.114f;
    const float luma = input.r() * kLumaR + input.g() * kLumaG +
                       input.b() * kLumaB;
    const float currentSaturation = std::max(
        {input.r(), input.g(), input.b()}) -
        std::min({input.r(), input.g(), input.b()});
    const float scale = std::max(
        0.0f, (1.0f + vibrance * (1.0f - currentSaturation)) *
                  (1.0f + saturation));
    const auto channel = [luma, scale](float value) {
        return std::clamp(luma + (value - luma) * scale, 0.0f, 1.0f);
    };
    return FloatRGBA(channel(input.r()), channel(input.g()),
                     channel(input.b()), input.a());
}

} // namespace

TEST(VibranceEffectContractTest, CpuFormulaWeightsLowSaturationAndPreservesAlpha)
{
    const float pixels[] = {
        0.50f, 0.45f, 0.40f, 0.25f,
        0.90f, 0.20f, 0.10f, 0.75f,
    };
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    VibranceEffect effect;
    effect.setVibrance(0.8f);
    effect.setSaturation(-0.1f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.width(), 2);
    ASSERT_EQ(output.height(), 1);
    expectPixelNear(output, 0, 0,
                    expectedVibrance(FloatRGBA(0.50f, 0.45f, 0.40f, 0.25f),
                                     0.8f, -0.1f));
    expectPixelNear(output, 1, 0,
                    expectedVibrance(FloatRGBA(0.90f, 0.20f, 0.10f, 0.75f),
                                     0.8f, -0.1f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.50f, 0.45f, 0.40f, 0.25f));
    expectPixelNear(source, 1, 0, FloatRGBA(0.90f, 0.20f, 0.10f, 0.75f));
}

TEST(VibranceEffectContractTest, NeutralSettingsPreserveRgba)
{
    const float pixels[] = {0.25f, 0.50f, 0.75f, 0.4f,
                            0.80f, 0.20f, 0.10f, 0.9f};
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    VibranceEffect effect;

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.25f, 0.50f, 0.75f, 0.4f));
    expectPixelNear(output, 1, 0, FloatRGBA(0.80f, 0.20f, 0.10f, 0.9f));
}

TEST(VibranceEffectContractTest, SettersClampFiniteValuesAndResetNonFiniteValues)
{
    VibranceEffect effect;
    effect.setVibrance(2.0f);
    effect.setSaturation(-2.0f);
    EXPECT_FLOAT_EQ(effect.vibrance(), 1.0f);
    EXPECT_FLOAT_EQ(effect.saturation(), -1.0f);

    effect.setVibrance(std::numeric_limits<float>::quiet_NaN());
    effect.setSaturation(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.vibrance(), 0.0f);
    EXPECT_FLOAT_EQ(effect.saturation(), 0.0f);
}

TEST(VibranceEffectContractTest, GpuSpatialDescriptorCarriesKeyAndBothParameters)
{
    VibranceEffect effect;
    effect.setVibrance(0.35f);
    effect.setSaturation(-0.2f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(effect.gpuRasterEffectDomain(), GpuRasterEffectDomain::Spatial);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, effect.gpuGenericKey());
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("vibrance"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 0.35f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], -0.2f);
}

TEST(VibranceEffectContractTest, PropertyDescriptorsAndNamedUpdatesStayAligned)
{
    VibranceEffect effect;
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 2u);
    EXPECT_EQ(properties[0].getName(), QStringLiteral("Vibrance"));
    EXPECT_EQ(properties[1].getName(), QStringLiteral("Saturation"));
    EXPECT_EQ(properties[0].getType(), PropertyType::Float);
    EXPECT_EQ(properties[1].getType(), PropertyType::Float);
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 0.0f);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 0.0f);

    effect.setPropertyValue(UniString("Vibrance"), QVariant(0.3));
    effect.setPropertyValue(UniString("Saturation"), QVariant(-0.4));
    EXPECT_FLOAT_EQ(effect.vibrance(), 0.3f);
    EXPECT_FLOAT_EQ(effect.saturation(), -0.4f);
}
