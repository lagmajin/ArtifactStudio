#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Generate.SimpleRain;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 96;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth) * kHeight * 4;

ArtifactCore::ImageF32x4RGBAWithCache makeSource(
    std::array<float, kChannelCount>& pixels)
{
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        pixels[pixel * 4] = 0.12f;
        pixels[pixel * 4 + 1] = 0.24f;
        pixels[pixel * 4 + 2] = 0.36f;
        pixels[pixel * 4 + 3] =
            static_cast<float>((pixel % 17) + 1) / 20.0f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SimpleRainEffectContractTest,
     FixedSeedProducesRepeatableRainWithoutMutatingInputOrAlpha)
{
    std::array<float, kChannelCount> sourcePixels{};
    const auto source = makeSource(sourcePixels);
    ArtifactCore::ImageF32x4RGBAWithCache firstOutput;
    ArtifactCore::ImageF32x4RGBAWithCache repeatedOutput;
    Artifact::SimpleRainEffect firstEffect;
    firstEffect.setPropertyValue(ArtifactCore::UniString("Density"),
                                 QVariant(4.0));
    firstEffect.setPropertyValue(ArtifactCore::UniString("Seed"),
                                 QVariant(1729));
    firstEffect.setPropertyValue(ArtifactCore::UniString("Evolution"),
                                 QVariant(0.37));
    firstEffect.applyCPUOnly(source, firstOutput);

    Artifact::SimpleRainEffect repeatedEffect;
    repeatedEffect.setPropertyValue(ArtifactCore::UniString("Density"),
                                   QVariant(4.0));
    repeatedEffect.setPropertyValue(ArtifactCore::UniString("Seed"),
                                   QVariant(1729));
    repeatedEffect.setPropertyValue(ArtifactCore::UniString("Evolution"),
                                   QVariant(0.37));
    repeatedEffect.applyCPUOnly(source, repeatedOutput);

    const float* input = source.image().rgba32fData();
    const float* first = firstOutput.image().rgba32fData();
    const float* repeated = repeatedOutput.image().rgba32fData();
    ASSERT_NE(input, nullptr);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(repeated, nullptr);
    bool colorChanged = false;
    for (std::size_t pixel = 0; pixel < kChannelCount / 4; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t index = pixel * 4 + channel;
            EXPECT_FLOAT_EQ(first[index], repeated[index])
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(input[index], sourcePixels[index])
                << "source changed at pixel=" << pixel
                << " channel=" << channel;
            colorChanged = colorChanged || first[index] != input[index];
        }
        const std::size_t alphaIndex = pixel * 4 + 3;
        EXPECT_FLOAT_EQ(first[alphaIndex], sourcePixels[alphaIndex])
            << "alpha changed at pixel=" << pixel;
        EXPECT_FLOAT_EQ(repeated[alphaIndex], sourcePixels[alphaIndex])
            << "repeated alpha changed at pixel=" << pixel;
    }
    EXPECT_TRUE(colorChanged);
}

TEST(SimpleRainEffectContractTest, ZeroDensityIsAnExactPassThrough)
{
    std::array<float, kChannelCount> sourcePixels{};
    const auto source = makeSource(sourcePixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::SimpleRainEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Density"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t index = 0; index < kChannelCount; ++index) {
        EXPECT_FLOAT_EQ(actual[index], sourcePixels[index])
            << "channel index=" << index;
    }
}
