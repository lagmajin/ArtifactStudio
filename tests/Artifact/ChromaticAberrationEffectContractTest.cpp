#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ChromaticAberration;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 5;
constexpr float kAlpha = 0.65f;

ImageF32x4RGBAWithCache makeHorizontalGrayRamp()
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

TEST(ChromaticAberrationEffectContractTest, RadialChannelOffsetsSeparateColorAndPreserveAlpha)
{
    const auto source = makeHorizontalGrayRamp();
    ImageF32x4RGBAWithCache output;
    ChromaticAberrationEffect effect;
    effect.setRedShift(8.0f);
    effect.setBlueShift(8.0f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);

    effect.applyCPUOnly(source, output);

    const auto original = source.image().getPixel(1, 2);
    const auto shifted = output.image().getPixel(1, 2);
    EXPECT_NEAR(original.r(), original.b(), 1.0e-6f);
    EXPECT_GT(std::abs(shifted.r() - shifted.b()), 0.1f);
    EXPECT_NEAR(shifted.a(), kAlpha, 1.0e-6f);
}

TEST(ChromaticAberrationEffectContractTest, ZeroChannelShiftsPreserveEveryInputChannel)
{
    const auto source = makeHorizontalGrayRamp();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ImageF32x4RGBAWithCache output;
    ChromaticAberrationEffect effect;
    effect.setRedShift(0.0f);
    effect.setBlueShift(0.0f);

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

TEST(ChromaticAberrationEffectContractTest, LeavesGreenAndAlphaUnshiftedAcrossImageBounds)
{
    const auto source = makeHorizontalGrayRamp();
    ImageF32x4RGBAWithCache output;
    ChromaticAberrationEffect effect;
    effect.setRedShift(50.0f);
    effect.setBlueShift(50.0f);
    effect.setCenterX(0.0f);
    effect.setCenterY(0.0f);

    effect.applyCPUOnly(source, output);

    const float* inputPixels = source.image().rgba32fData();
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(inputPixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
        const int offset = pixel * 4;
        EXPECT_NEAR(outputPixels[offset + 1], inputPixels[offset + 1], 1.0e-6f)
            << "green pixel=" << pixel;
        EXPECT_NEAR(outputPixels[offset + 3], inputPixels[offset + 3], 1.0e-6f)
            << "alpha pixel=" << pixel;
    }
    EXPECT_NEAR(output.image().getPixel(0, 0).r(), source.image().getPixel(0, 0).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(kWidth - 1, 0).b(),
                source.image().getPixel(0, 0).b(), 1.0e-6f);
}
