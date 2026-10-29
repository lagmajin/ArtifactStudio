#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.LiquidGlow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 5;
constexpr int kHeight = 5;

ImageF32x4RGBAWithCache makeUniformSurface(float value, float alpha)
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

TEST(LiquidGlowEffectContractTest, ThresholdedGlowAddsColorAndPreservesAlpha)
{
    const auto source = makeUniformSurface(0.5f, 0.35f);
    ImageF32x4RGBAWithCache output;
    LiquidGlowEffect effect;
    effect.setThreshold(0.1f);
    effect.setRadius(0.5f);
    effect.setIntensity(1.0f);
    effect.setFlowScale(42.0f);
    effect.setDistortion(0.0f);
    effect.setPhase(0.0f);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_GT(pixel.r(), 0.5f);
    EXPECT_NEAR(pixel.r(), pixel.g(), 1.0e-6f);
    EXPECT_NEAR(pixel.g(), pixel.b(), 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.35f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).r(), 0.5f, 1.0e-6f);
}

TEST(LiquidGlowEffectContractTest, ZeroIntensityPreservesEveryRgbaChannel)
{
    constexpr std::array<float, 16> pixels = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.4f, 0.5f, 0.6f, 0.25f,
        0.7f, 0.8f, 0.9f, 0.75f,
        0.3f, 0.2f, 0.1f, 1.0f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    LiquidGlowEffect effect;
    effect.setIntensity(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 2);
    ASSERT_EQ(output.image().height(), 2);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(LiquidGlowEffectContractTest, HighlightAtExactThresholdProducesNoGlow)
{
    const auto source = makeUniformSurface(0.5f, 0.35f);
    ImageF32x4RGBAWithCache output;
    LiquidGlowEffect effect;
    effect.setThreshold(0.5f);
    effect.setRadius(2.0f);
    effect.setIntensity(1.0f);
    effect.setDistortion(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    const float* expected = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}
\n
