#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.LuminescenceCaustics;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 32;

ImageF32x4RGBAWithCache makeUniformHighlight(float value, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(LuminescenceCausticsEffectContractTest, BrightSurfaceGetsCausticPatternAndKeepsAlpha)
{
    constexpr float baseValue = 0.9f;
    constexpr float alpha = 0.4f;
    const auto source = makeUniformHighlight(baseValue, alpha);
    ImageF32x4RGBAWithCache output;
    LuminescenceCausticsEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(0.1f));
    effect.setPropertyValue(UniString("Edge Weight"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Scale"), QVariant(5.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Evolution"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Color Shift"), QVariant(0.5f));

    effect.applyCPUOnly(source, output);

    float maximumOutput = baseValue;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            maximumOutput = std::max(
                maximumOutput, std::max({pixel.r(), pixel.g(), pixel.b()}));
            EXPECT_NEAR(pixel.a(), alpha, 1.0e-6f);
        }
    }
    EXPECT_GT(maximumOutput, baseValue + 1.0e-4f);
    EXPECT_NEAR(source.image().getPixel(10, 10).r(), baseValue, 1.0e-6f);
}

TEST(LuminescenceCausticsEffectContractTest, BelowThresholdFlatSurfaceReceivesNoCaustics)
{
    constexpr float baseValue = 0.4f;
    constexpr float alpha = 0.7f;
    const auto source = makeUniformHighlight(baseValue, alpha);
    ImageF32x4RGBAWithCache output;
    LuminescenceCausticsEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(0.8f));
    effect.setPropertyValue(UniString("Edge Weight"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(5.0f));

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), baseValue, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), baseValue, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), baseValue, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), alpha, 1.0e-6f);
        }
    }
}

TEST(LuminescenceCausticsEffectContractTest, PropertiesClampAndResetNonFiniteValues)
{
    LuminescenceCausticsEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Edge Weight"), QVariant(-1.0f));
    effect.setPropertyValue(UniString("Scale"), QVariant(300.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(8.0f));
    effect.setPropertyValue(UniString("Color Shift"), QVariant(-2.0f));

    const auto clamped = effect.getProperties();
    EXPECT_FLOAT_EQ(clamped[0].getValue().toFloat(), 1.0f);
    EXPECT_FLOAT_EQ(clamped[1].getValue().toFloat(), 0.0f);
    EXPECT_FLOAT_EQ(clamped[2].getValue().toFloat(), 200.0f);
    EXPECT_FLOAT_EQ(clamped[3].getValue().toFloat(), 5.0f);
    EXPECT_FLOAT_EQ(clamped[5].getValue().toFloat(), -1.0f);

    effect.setPropertyValue(UniString("Threshold"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    effect.setPropertyValue(UniString("Edge Weight"),
                            QVariant(std::numeric_limits<float>::infinity()));
    effect.setPropertyValue(UniString("Scale"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    effect.setPropertyValue(UniString("Intensity"),
                            QVariant(std::numeric_limits<float>::infinity()));
    effect.setPropertyValue(UniString("Evolution"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    effect.setPropertyValue(UniString("Color Shift"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));

    const auto defaults = effect.getProperties();
    EXPECT_FLOAT_EQ(defaults[0].getValue().toFloat(), 0.55f);
    EXPECT_FLOAT_EQ(defaults[1].getValue().toFloat(), 0.8f);
    EXPECT_FLOAT_EQ(defaults[2].getValue().toFloat(), 22.0f);
    EXPECT_FLOAT_EQ(defaults[3].getValue().toFloat(), 0.75f);
    EXPECT_FLOAT_EQ(defaults[4].getValue().toFloat(), 0.0f);
    EXPECT_FLOAT_EQ(defaults[5].getValue().toFloat(), 0.35f);
}
