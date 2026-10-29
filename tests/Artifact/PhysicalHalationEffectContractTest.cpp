#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.PhysicalHalation;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 5;
constexpr int kHeight = 5;

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

TEST(PhysicalHalationEffectContractTest, BrightHighlightAddsDiffusionWithoutChangingAlpha)
{
    const auto source = makeUniformHighlight(0.95f, 0.45f);
    ImageF32x4RGBAWithCache output;
    PhysicalHalationEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(0.8f));
    effect.setPropertyValue(UniString("Spread"), QVariant(1.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(1.0f));
    effect.setPropertyValue(UniString("Red Diffusion"), QVariant(1.0f));
    effect.setPropertyValue(UniString("Softness"), QVariant(0.5f));

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_GT(std::max({pixel.r(), pixel.g(), pixel.b()}), 0.95f);
    EXPECT_NEAR(pixel.a(), 0.45f, 1.0e-6f);
}

TEST(PhysicalHalationEffectContractTest, HighlightThresholdSuppressesDiffusionBelowThreshold)
{
    constexpr int width = 9;
    constexpr int height = 9;
    constexpr float background = 0.1f;
    constexpr float alpha = 0.4f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = background;
        pixels[offset + 1] = background;
        pixels[offset + 2] = background;
        pixels[offset + 3] = alpha;
    }
    const std::size_t highlightOffset =
        static_cast<std::size_t>(((height / 2) * width + width / 2) * 4);
    pixels[highlightOffset] = 1.0f;
    pixels[highlightOffset + 1] = 1.0f;
    pixels[highlightOffset + 2] = 1.0f;

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    PhysicalHalationEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(1.0f));
    effect.setPropertyValue(UniString("Spread"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Red Diffusion"), QVariant(1.0f));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f);
            EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f);
            EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f);
            EXPECT_NEAR(actual.a(), alpha, 1.0e-6f);
        }
    }
}

TEST(PhysicalHalationEffectContractTest, BrightPointDiffusesBeyondItsOriginalPixel)
{
    constexpr int width = 9;
    constexpr int height = 9;
    constexpr float background = 0.1f;
    constexpr float alpha = 0.4f;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = background;
        pixels[offset + 1] = background;
        pixels[offset + 2] = background;
        pixels[offset + 3] = alpha;
    }
    const int centerX = width / 2;
    const int centerY = height / 2;
    const std::size_t highlightOffset =
        static_cast<std::size_t>((centerY * width + centerX) * 4);
    pixels[highlightOffset] = 1.0f;
    pixels[highlightOffset + 1] = 1.0f;
    pixels[highlightOffset + 2] = 1.0f;

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    PhysicalHalationEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(0.8f));
    effect.setPropertyValue(UniString("Spread"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Red Diffusion"), QVariant(1.0f));

    effect.applyCPUOnly(source, output);

    const auto haloPixel = output.image().getPixel(centerX + 3, centerY);
    EXPECT_GT(haloPixel.r() + haloPixel.g() + haloPixel.b(),
              3.0f * background);
    EXPECT_NEAR(haloPixel.a(), alpha, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(centerX + 3, centerY).r(), background,
                1.0e-6f);
}

TEST(PhysicalHalationEffectContractTest, PropertySettersClampRangesAndRestoreNonFiniteDefaults)
{
    PhysicalHalationEffect effect;
    effect.setPropertyValue(UniString("Threshold"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Spread"), QVariant(-1.0f));
    effect.setPropertyValue(UniString("Intensity"), QVariant(8.0f));
    effect.setPropertyValue(UniString("Red Diffusion"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Softness"), QVariant(2.0f));

    auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 5u);
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 1.0f);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 0.0f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 5.0f);
    EXPECT_FLOAT_EQ(properties[3].getValue().toFloat(), 0.1f);
    EXPECT_FLOAT_EQ(properties[4].getValue().toFloat(), 1.0f);

    effect.setPropertyValue(UniString("Threshold"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    effect.setPropertyValue(UniString("Spread"),
                            QVariant(std::numeric_limits<float>::infinity()));
    effect.setPropertyValue(UniString("Intensity"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));
    effect.setPropertyValue(UniString("Red Diffusion"),
                            QVariant(std::numeric_limits<float>::infinity()));
    effect.setPropertyValue(UniString("Softness"),
                            QVariant(std::numeric_limits<float>::quiet_NaN()));

    properties = effect.getProperties();
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 0.8f);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 15.0f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 0.5f);
    EXPECT_FLOAT_EQ(properties[3].getValue().toFloat(), 2.0f);
    EXPECT_FLOAT_EQ(properties[4].getValue().toFloat(), 0.5f);
}
