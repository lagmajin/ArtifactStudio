#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Glow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 17;
constexpr int kHeight = 17;

ImageF32x4RGBAWithCache makeBrightImpulse()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    constexpr int center = (kHeight / 2 * kWidth + kWidth / 2) * 4;
    pixels[center] = 1.0f;
    pixels[center + 1] = 1.0f;
    pixels[center + 2] = 1.0f;
    pixels[center + 3] = 0.4f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(RasterizerGlowEffectContractTest, ThresholdedHighlightSpreadsAndKeepsAlpha)
{
    const auto source = makeBrightImpulse();
    ImageF32x4RGBAWithCache output;
    RasterizerGlowEffect effect;
    effect.setThreshold(0.1f);
    effect.setRadius(3.0f);
    effect.setIntensity(2.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_GT(output.image().getPixel(9, 8).r(), 0.0f);
    EXPECT_NEAR(output.image().getPixel(9, 8).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(8, 8).a(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(9, 8).r(), 0.0f, 1.0e-6f);
}

TEST(RasterizerGlowEffectContractTest, ZeroIntensityPreservesSourceRgba)
{
    const auto source = makeBrightImpulse();
    ImageF32x4RGBAWithCache output;
    RasterizerGlowEffect effect;
    effect.setIntensity(0.0f);

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

TEST(RasterizerGlowEffectContractTest, SettersClampFiniteValuesToSupportedRanges)
{
    RasterizerGlowEffect effect;
    effect.setThreshold(-0.5f);
    effect.setRadius(0.25f);
    effect.setIntensity(8.0f);

    EXPECT_FLOAT_EQ(effect.threshold(), 0.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 1.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 5.0f);

    effect.setThreshold(1.5f);
    effect.setRadius(3.0f);
    effect.setIntensity(-2.0f);

    EXPECT_FLOAT_EQ(effect.threshold(), 1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 3.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 0.0f);
}
\n
