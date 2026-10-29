#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Deflicker;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
constexpr float kAlpha = 0.55f;

ImageF32x4RGBAWithCache makeUniformFrame(float value)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = kAlpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(DeflickerEffectContractTest, RollingMedianCorrectsBrightnessFlickerAndKeepsAlpha)
{
    const auto darkFrame = makeUniformFrame(0.2f);
    const auto middleFrame = makeUniformFrame(0.3f);
    const auto brightFrame = makeUniformFrame(0.8f);
    ImageF32x4RGBAWithCache darkOutput;
    ImageF32x4RGBAWithCache middleOutput;
    ImageF32x4RGBAWithCache correctedOutput;
    DeflickerEffect effect;
    effect.setWindowSize(3);
    effect.setStrength(1.0f);
    effect.setLumaWeight(1.0f);

    effect.applyCPUOnly(darkFrame, darkOutput);
    effect.applyCPUOnly(middleFrame, middleOutput);
    effect.applyCPUOnly(brightFrame, correctedOutput);

    EXPECT_NEAR(darkOutput.image().getPixel(1, 1).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(middleOutput.image().getPixel(1, 1).r(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(correctedOutput.image().getPixel(1, 1).r(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(correctedOutput.image().getPixel(1, 1).a(), kAlpha, 1.0e-6f);
}

TEST(DeflickerEffectContractTest, ZeroStrengthPreservesEachFrameWhileHistoryAdvances)
{
    const auto darkFrame = makeUniformFrame(0.2f);
    const auto brightFrame = makeUniformFrame(0.8f);
    ImageF32x4RGBAWithCache darkOutput;
    ImageF32x4RGBAWithCache brightOutput;
    DeflickerEffect effect;
    effect.setWindowSize(3);
    effect.setStrength(0.0f);
    effect.setLumaWeight(1.0f);

    effect.applyCPUOnly(darkFrame, darkOutput);
    effect.applyCPUOnly(brightFrame, brightOutput);

    EXPECT_NEAR(darkOutput.image().getPixel(1, 1).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(darkOutput.image().getPixel(1, 1).a(), kAlpha, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).g(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).b(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).a(), kAlpha, 1.0e-6f);
}

TEST(DeflickerEffectContractTest, EvenWindowUsesUpperMedianForLatestBrightFrame)
{
    const auto darkFrame = makeUniformFrame(0.2f);
    const auto brightFrame = makeUniformFrame(0.8f);
    ImageF32x4RGBAWithCache darkOutput;
    ImageF32x4RGBAWithCache brightOutput;
    DeflickerEffect effect;
    effect.setWindowSize(2);
    effect.setStrength(1.0f);
    effect.setLumaWeight(1.0f);

    effect.applyCPUOnly(darkFrame, darkOutput);
    effect.applyCPUOnly(brightFrame, brightOutput);

    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).g(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).b(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(brightOutput.image().getPixel(1, 1).a(), kAlpha, 1.0e-6f);
}
\n
