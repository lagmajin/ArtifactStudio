#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.DataMosh;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 64;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth * kHeight * 4);

ImageF32x4RGBAWithCache makeSolidSurface(float red, float green, float blue)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        pixels[pixel] = red;
        pixels[pixel + 1] = green;
        pixels[pixel + 2] = blue;
        pixels[pixel + 3] = 0.65f;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

void configure(DataMoshEffect& effect)
{
    effect.setIntensity(0.45f);
    effect.setHoldFrames(5);
    effect.setBlockSize(8);
    effect.setBlend(0.75f);
}

} // namespace

TEST(DataMoshEffectContractTest, FixedInputSequenceIsRepeatableAndPreservesSource)
{
    const auto firstFrame = makeSolidSurface(0.15f, 0.25f, 0.35f);
    const auto secondFrame = makeSolidSurface(0.75f, 0.45f, 0.05f);
    const float* firstFramePixels = firstFrame.image().rgba32fData();
    const float* secondFramePixels = secondFrame.image().rgba32fData();
    ASSERT_NE(firstFramePixels, nullptr);
    ASSERT_NE(secondFramePixels, nullptr);

    DataMoshEffect firstEffect;
    DataMoshEffect secondEffect;
    configure(firstEffect);
    configure(secondEffect);

    ImageF32x4RGBAWithCache firstEffectFrameOne;
    ImageF32x4RGBAWithCache firstEffectFrameTwo;
    ImageF32x4RGBAWithCache secondEffectFrameOne;
    ImageF32x4RGBAWithCache secondEffectFrameTwo;
    firstEffect.applyCPUOnly(firstFrame, firstEffectFrameOne);
    firstEffect.applyCPUOnly(secondFrame, firstEffectFrameTwo);
    secondEffect.applyCPUOnly(firstFrame, secondEffectFrameOne);
    secondEffect.applyCPUOnly(secondFrame, secondEffectFrameTwo);

    const float* outputs[] = {
        firstEffectFrameOne.image().rgba32fData(),
        firstEffectFrameTwo.image().rgba32fData(),
        secondEffectFrameOne.image().rgba32fData(),
        secondEffectFrameTwo.image().rgba32fData(),
    };
    for (const float* output : outputs) {
        ASSERT_NE(output, nullptr);
        for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
            EXPECT_FLOAT_EQ(output[pixel + 3], 0.65f)
                << "pixel=" << pixel / 4;
        }
    }

    for (std::size_t channel = 0; channel < kChannelCount; ++channel) {
        EXPECT_FLOAT_EQ(outputs[0][channel], outputs[2][channel]);
        EXPECT_FLOAT_EQ(outputs[1][channel], outputs[3][channel]);
        EXPECT_FLOAT_EQ(firstFramePixels[channel], channel % 4 == 0 ? 0.15f
                                        : channel % 4 == 1 ? 0.25f
                                        : channel % 4 == 2 ? 0.35f : 0.65f);
        EXPECT_FLOAT_EQ(secondFramePixels[channel], channel % 4 == 0 ? 0.75f
                                        : channel % 4 == 1 ? 0.45f
                                        : channel % 4 == 2 ? 0.05f : 0.65f);
    }
}

TEST(DataMoshEffectContractTest, ZeroIntensityPreservesEachInputFrame)
{
    const auto firstFrame = makeSolidSurface(0.15f, 0.25f, 0.35f);
    const auto secondFrame = makeSolidSurface(0.75f, 0.45f, 0.05f);
    DataMoshEffect effect;
    effect.setIntensity(0.0f);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;

    effect.applyCPUOnly(firstFrame, firstOutput);
    effect.applyCPUOnly(secondFrame, secondOutput);

    for (std::size_t channel = 0; channel < kChannelCount; ++channel) {
        EXPECT_FLOAT_EQ(firstOutput.image().rgba32fData()[channel],
                        firstFrame.image().rgba32fData()[channel])
            << "first channel=" << channel;
        EXPECT_FLOAT_EQ(secondOutput.image().rgba32fData()[channel],
                        secondFrame.image().rgba32fData()[channel])
            << "second channel=" << channel;
    }
}
