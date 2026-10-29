#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Deflicker;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeSolidSurface(float value,
                                                        float alpha)
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = alpha;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(DeflickerEffectContractTest, MedianHistoryCorrectsAOneFrameBrightnessSpike)
{
    const auto normal = makeSolidSurface(0.25f, 0.3f);
    const auto spike = makeSolidSurface(0.75f, 0.6f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DeflickerEffect effect;
    effect.setWindowSize(4);
    effect.setStrength(1.0f);
    effect.setLumaWeight(1.0f);

    effect.applyCPUOnly(normal, output);
    effect.applyCPUOnly(normal, output);
    effect.applyCPUOnly(spike, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), 0.25f, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), 0.25f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), 0.25f, 1.0e-6f);
            EXPECT_FLOAT_EQ(pixel.a(), 0.6f);
        }
    }
}

TEST(DeflickerEffectContractTest, ZeroStrengthLeavesCurrentPixelsUnchanged)
{
    const auto normal = makeSolidSurface(0.25f, 0.3f);
    const auto current = makeSolidSurface(0.72f, 0.45f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DeflickerEffect effect;
    effect.setStrength(0.0f);

    effect.applyCPUOnly(normal, output);
    effect.applyCPUOnly(normal, output);
    effect.applyCPUOnly(current, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), 0.72f);
            EXPECT_FLOAT_EQ(pixel.g(), 0.72f);
            EXPECT_FLOAT_EQ(pixel.b(), 0.72f);
            EXPECT_FLOAT_EQ(pixel.a(), 0.45f);
        }
    }
}

TEST(DeflickerEffectContractTest, SettersClampWindowStrengthAndLumaWeight)
{
    Artifact::DeflickerEffect effect;
    effect.setWindowSize(1);
    EXPECT_EQ(effect.windowSize(), 2);
    effect.setWindowSize(200);
    EXPECT_EQ(effect.windowSize(), 120);

    effect.setStrength(-1.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 0.0f);
    effect.setStrength(2.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 1.0f);

    effect.setLumaWeight(-1.0f);
    EXPECT_FLOAT_EQ(effect.lumaWeight(), 0.0f);
    effect.setLumaWeight(2.0f);
    EXPECT_FLOAT_EQ(effect.lumaWeight(), 1.0f);
}

TEST(DeflickerEffectContractTest, NamedPropertiesUpdateThePublicValues)
{
    Artifact::DeflickerEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("windowSize"), QVariant(9));
    effect.setPropertyValue(ArtifactCore::UniString("strength"), QVariant(0.35));
    effect.setPropertyValue(ArtifactCore::UniString("lumaWeight"), QVariant(0.6));

    EXPECT_EQ(effect.windowSize(), 9);
    EXPECT_FLOAT_EQ(effect.strength(), 0.35f);
    EXPECT_FLOAT_EQ(effect.lumaWeight(), 0.6f);

    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 3u);
    EXPECT_EQ(properties[0].getName(), QStringLiteral("windowSize"));
    EXPECT_EQ(properties[1].getName(), QStringLiteral("strength"));
    EXPECT_EQ(properties[2].getName(), QStringLiteral("lumaWeight"));
    EXPECT_EQ(properties[0].getValue().toInt(), 9);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 0.35f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 0.6f);
}
