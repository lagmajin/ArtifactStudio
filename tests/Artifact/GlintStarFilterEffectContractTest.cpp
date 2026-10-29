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
    pixels[center + 1] = 0.8f;
    pixels[center + 2] = 0.6f;
    pixels[center + 3] = 0.3f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(GlintStarFilterEffectContractTest, SpreadsBrightPointAlongRaysAndKeepsAlpha)
{
    const auto source = makeHighlightImpulse();
    ImageF32x4RGBAWithCache output;
    GlintStarFilterEffect effect;
    effect.setPropertyValue(UniString("threshold"), QVariant(0.1));
    effect.setPropertyValue(UniString("length"), QVariant(10.0));
    effect.setPropertyValue(UniString("width"), QVariant(1.0));
    effect.setPropertyValue(UniString("intensity"), QVariant(4.0));
    effect.setPropertyValue(UniString("rays"), QVariant(8));

    effect.applyCPUOnly(source, output);

    float maximumOutput = 0.0f;
    float offCenterOutput = 0.0f;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            maximumOutput = std::max(maximumOutput, pixel.r());
            if (x == 18 && y == 16) {
                offCenterOutput = pixel.r();
            }
            EXPECT_NEAR(pixel.a(), source.image().getPixel(x, y).a(), 1.0e-6f);
        }
    }
    EXPECT_GT(maximumOutput, 1.0f);
    EXPECT_GT(offCenterOutput, 0.0f);
    EXPECT_NEAR(source.image().getPixel(18, 16).r(), 0.0f, 1.0e-6f);
}

TEST(GlintStarFilterEffectContractTest, ThresholdAboveHighlightLuminancePreservesImage)
{
    const auto source = makeHighlightImpulse();
    ImageF32x4RGBAWithCache output;
    GlintStarFilterEffect effect;
    effect.setPropertyValue(UniString("threshold"), QVariant(1.0));

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(kWidth * kHeight); ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto index = pixel * 4 + channel;
            EXPECT_NEAR(outputPixels[index], sourcePixels[index], 1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
}

TEST(GlintStarFilterEffectContractTest, ZeroIntensityPreservesSourceRgba)
{
    const auto source = makeHighlightImpulse();
    ImageF32x4RGBAWithCache output;
    GlintStarFilterEffect effect;
    effect.setPropertyValue(UniString("threshold"), QVariant(0.1));
    effect.setPropertyValue(UniString("intensity"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(kWidth * kHeight); ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto index = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(outputPixels[index], sourcePixels[index])
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
}
\n
\n
