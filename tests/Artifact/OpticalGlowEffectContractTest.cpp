#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 33;
constexpr int kHeight = 33;

ImageF32x4RGBAWithCache makeHighlightImpulse()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    constexpr int center = (kHeight / 2 * kWidth + kWidth / 2) * 4;
    pixels[center] = 1.0f;
    pixels[center + 1] = 1.0f;
    pixels[center + 2] = 1.0f;
    pixels[center + 3] = 0.35f;

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(OpticalGlowEffectContractTest, SpreadsBrightHighlightsAndKeepsAlpha)
{
    const auto source = makeHighlightImpulse();
    ImageF32x4RGBAWithCache output;
    OpticalGlowEffect effect;
    effect.setPropertyValue(UniString("threshold"), QVariant(0.1));
    effect.setPropertyValue(UniString("radius"), QVariant(1.5));
    effect.setPropertyValue(UniString("afterglowRadius"), QVariant(3.0));
    effect.setPropertyValue(UniString("intensity"), QVariant(2.0));
    effect.setPropertyValue(UniString("afterglowIntensity"), QVariant(1.0));

    effect.applyCPUOnly(source, output);

    float maximumOutput = 0.0f;
    float offCenterGlow = 0.0f;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            maximumOutput = std::max(maximumOutput, pixel.r());
            if (x == 17 && y == 16) {
                offCenterGlow = pixel.r();
            }
            EXPECT_NEAR(pixel.a(), source.image().getPixel(x, y).a(), 1.0e-6f);
        }
    }
    EXPECT_GT(maximumOutput, 1.0f);
    EXPECT_GT(offCenterGlow, 0.0f);
    EXPECT_NEAR(source.image().getPixel(17, 16).r(), 0.0f, 1.0e-6f);
}


TEST(OpticalGlowEffectContractTest, ZeroContributionControlsPreserveEveryChannel)
{
    const auto source = makeHighlightImpulse();
    ImageF32x4RGBAWithCache output;
    OpticalGlowEffect effect;
    effect.setPropertyValue(UniString("intensity"), QVariant(0.0));
    effect.setPropertyValue(UniString("afterglowIntensity"), QVariant(0.0));
    effect.setPropertyValue(UniString("streakStrength"), QVariant(0.0));
    effect.setPropertyValue(UniString("atmosphere"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], original[channel])
            << "channel=" << channel;
    }
}

TEST(OpticalGlowEffectContractTest, PositiveWarmthBoostsRedRelativeToBlue)
{
    constexpr float red = 0.8f;
    constexpr float green = 0.5f;
    constexpr float blue = 0.3f;
    constexpr float alpha = 0.4f;
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = red;
        pixels[offset + 1] = green;
        pixels[offset + 2] = blue;
        pixels[offset + 3] = alpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    OpticalGlowEffect effect;
    effect.setPropertyValue(UniString("threshold"), QVariant(0.1));
    effect.setPropertyValue(UniString("softKnee"), QVariant(0.0));
    effect.setPropertyValue(UniString("radius"), QVariant(1.5));
    effect.setPropertyValue(UniString("intensity"), QVariant(1.0));
    effect.setPropertyValue(UniString("afterglowIntensity"), QVariant(0.0));
    effect.setPropertyValue(UniString("streakStrength"), QVariant(0.0));
    effect.setPropertyValue(UniString("atmosphere"), QVariant(0.0));
    effect.setPropertyValue(UniString("warmth"), QVariant(1.0));

    effect.applyCPUOnly(source, output);

    const auto before = source.image().getPixel(kWidth / 2, kHeight / 2);
    const auto after = output.image().getPixel(kWidth / 2, kHeight / 2);
    const float redGain = (after.r() - before.r()) / red;
    const float greenGain = (after.g() - before.g()) / green;
    const float blueGain = (after.b() - before.b()) / blue;
    EXPECT_GT(redGain, greenGain);
    EXPECT_GT(greenGain, blueGain);
    EXPECT_NEAR(after.a(), alpha, 1.0e-6f);
}
