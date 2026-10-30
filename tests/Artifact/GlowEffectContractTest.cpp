#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow;
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

TEST(GlowEffectContractTest, HighlightsReceiveLayeredGlowAndKeepAlpha)
{
    const auto source = makeUniformSurface(0.9f, 0.25f);
    ImageF32x4RGBAWithCache output;
    GlowEffect effect;
    effect.setGlowGain(1.0f);
    effect.setLayerCount(1);
    effect.setBaseSigma(0.5f);
    effect.setSigmaGrowth(1.0f);
    effect.setBaseAlpha(0.5f);
    effect.setAlphaFalloff(0.5f);
    effect.setContributionPreview(false);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_GT(pixel.r(), 0.9f);
    EXPECT_NEAR(pixel.r(), pixel.g(), 1.0e-6f);
    EXPECT_NEAR(pixel.g(), pixel.b(), 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.25f, 1.0e-6f);
}


TEST(GlowEffectContractTest, ZeroGainPreservesInputAndAlpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.1f + static_cast<float>(i / 4) * 0.01f;
        pixels[i + 1] = 0.35f;
        pixels[i + 2] = 0.2f;
        pixels[i + 3] = 0.4f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    GlowEffect effect;
    effect.setGlowGain(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(actual[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(GlowEffectContractTest, NumericSettingsClampAndResetNonFiniteValues)
{
    GlowEffect effect;
    effect.setGlowGain(12.0f);
    effect.setLayerCount(20);
    effect.setBaseSigma(0.0f);
    effect.setSigmaGrowth(5.0f);
    effect.setBaseAlpha(-1.0f);
    effect.setAlphaFalloff(2.0f);
    EXPECT_FLOAT_EQ(effect.glowGain(), 10.0f);
    EXPECT_EQ(effect.layerCount(), 16);
    EXPECT_FLOAT_EQ(effect.baseSigma(), 0.1f);
    EXPECT_FLOAT_EQ(effect.sigmaGrowth(), 4.0f);
    EXPECT_FLOAT_EQ(effect.baseAlpha(), 0.0f);
    EXPECT_FLOAT_EQ(effect.alphaFalloff(), 1.0f);

    effect.setGlowGain(std::numeric_limits<float>::quiet_NaN());
    effect.setBaseSigma(std::numeric_limits<float>::infinity());
    effect.setSigmaGrowth(std::numeric_limits<float>::quiet_NaN());
    effect.setBaseAlpha(std::numeric_limits<float>::infinity());
    effect.setAlphaFalloff(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.glowGain(), 1.0f);
    EXPECT_FLOAT_EQ(effect.baseSigma(), 5.0f);
    EXPECT_FLOAT_EQ(effect.sigmaGrowth(), 1.8f);
    EXPECT_FLOAT_EQ(effect.baseAlpha(), 0.3f);
    EXPECT_FLOAT_EQ(effect.alphaFalloff(), 0.6f);

    effect.setLayerCount(-4);
    EXPECT_EQ(effect.layerCount(), 1);
}
