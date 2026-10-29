#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VectorFlowGlitch;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 12;
constexpr float kAlpha = 0.65f;

ImageF32x4RGBAWithCache makeHorizontalRamp()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = kAlpha;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(VectorFlowGlitchEffectContractTest, FixedEvolutionDisplacesRampAndPreservesUniformAlpha)
{
    const auto source = makeHorizontalRamp();
    ImageF32x4RGBAWithCache output;
    VectorFlowGlitchEffect effect;
    effect.setPropertyValue(UniString("Glitch Amount"), QVariant(10.0f));
    effect.setPropertyValue(UniString("Frequency"), QVariant(0.3f));
    effect.setPropertyValue(UniString("Chromatic Aberration"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Edge Flow Influence"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Evolution"), QVariant(0.7f));

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    bool rgbChanged = false;
    for (std::size_t offset = 0; offset < kWidth * kHeight * 4; offset += 4) {
        const int x = static_cast<int>((offset / 4) % kWidth);
        const float sourceValue =
            static_cast<float>(x) / static_cast<float>(kWidth - 1);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            rgbChanged = rgbChanged ||
                         std::abs(sourceValue - outputPixels[offset + channel]) >
                             1.0e-4f;
        }
        EXPECT_NEAR(outputPixels[offset + 3], kAlpha, 1.0e-6f);
    }
    EXPECT_TRUE(rgbChanged);
}

TEST(VectorFlowGlitchEffectContractTest, ZeroDisplacementAndChromaticSplitPreserveInput)
{
    const auto source = makeHorizontalRamp();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ImageF32x4RGBAWithCache output;
    VectorFlowGlitchEffect effect;
    effect.setPropertyValue(UniString("Glitch Amount"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Chromatic Aberration"), QVariant(0.0f));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_NEAR(outputPixels[channel], sourcePixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}
