#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.SlitScan;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 2;

ImageF32x4RGBAWithCache makeUniformFrame(float value, float alpha)
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

TEST(SlitScanEffectContractTest, AutoPositionAccumulatesSuccessiveColumns)
{
    const auto firstFrame = makeUniformFrame(0.2f, 0.3f);
    const auto secondFrame = makeUniformFrame(0.8f, 0.7f);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    SlitScanEffect effect;
    effect.setDirection(0.0f);
    effect.setSpeed(1.0f);
    effect.setPersistence(1.0f);
    effect.setPosition(-1.0f);

    effect.applyCPUOnly(firstFrame, firstOutput);
    effect.applyCPUOnly(secondFrame, secondOutput);

    EXPECT_NEAR(firstOutput.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(firstOutput.image().getPixel(1, 0).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(1, 0).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(2, 0).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(0, 0).a(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(1, 0).a(), 0.7f, 1.0e-6f);
}

TEST(SlitScanEffectContractTest, VerticalAutoPositionAccumulatesSuccessiveRows)
{
    const auto firstFrame = makeUniformFrame(0.2f, 0.3f);
    const auto secondFrame = makeUniformFrame(0.8f, 0.7f);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    SlitScanEffect effect;
    effect.setDirection(1.0f);
    effect.setSpeed(1.0f);
    effect.setPersistence(1.0f);
    effect.setPosition(-1.0f);

    effect.applyCPUOnly(firstFrame, firstOutput);
    effect.applyCPUOnly(secondFrame, secondOutput);

    EXPECT_NEAR(firstOutput.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(firstOutput.image().getPixel(0, 1).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(2, 0).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(2, 1).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(2, 0).a(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(2, 1).a(), 0.7f, 1.0e-6f);
}

TEST(SlitScanEffectContractTest, PersistenceDecaysPreviouslyCapturedColumn)
{
    const auto firstFrame = makeUniformFrame(0.2f, 0.3f);
    const auto secondFrame = makeUniformFrame(0.8f, 0.7f);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    SlitScanEffect effect;
    effect.setDirection(0.0f);
    effect.setSpeed(1.0f);
    effect.setPersistence(0.5f);
    effect.setPosition(-1.0f);

    effect.applyCPUOnly(firstFrame, firstOutput);
    effect.applyCPUOnly(secondFrame, secondOutput);

    EXPECT_NEAR(firstOutput.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(0, 0).r(), 0.1f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(0, 0).a(), 0.15f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(1, 0).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(secondOutput.image().getPixel(1, 0).a(), 0.7f, 1.0e-6f);
}
\n
