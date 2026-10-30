#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FilmDamage;
import Artifact.Effect.Rasterizer.Glitch;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 32;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth * kHeight * 4);

std::array<float, kChannelCount> makeGradientImage()
{
    std::array<float, kChannelCount> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                 static_cast<float>(kWidth + kHeight - 2);
            pixels[offset + 3] = 0.6f;
        }
    }
    return pixels;
}

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

bool differsFrom(const float* lhs, const float* rhs, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        if (std::abs(lhs[i] - rhs[i]) > 1.0e-6f) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(DigitalDamageEffectsContractTest, FilmDamageIsSeededAndPreservesAlphaAndInput)
{
    std::array<float, kChannelCount> pixels{};
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        pixels[pixel] = 0.4f;
        pixels[pixel + 1] = 0.4f;
        pixels[pixel + 2] = 0.4f;
        pixels[pixel + 3] = 0.6f;
    }
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    FilmDamageEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Grain")), QVariant(0.1));
    effect.setPropertyValue(UniString(QStringLiteral("Dust")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Scratches")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Gate Weave")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Flicker")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Film Burn")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(0.5));
    effect.setPropertyValue(UniString(QStringLiteral("Seed")), QVariant(23));

    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(original, nullptr);
    EXPECT_FALSE(differsFrom(first, second, kChannelCount));
    EXPECT_TRUE(differsFrom(first, pixels.data(), kChannelCount));
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        EXPECT_FLOAT_EQ(first[pixel + 3], 0.6f) << "pixel=" << pixel / 4;
        EXPECT_FLOAT_EQ(original[pixel], 0.4f) << "pixel=" << pixel / 4;
    }
}

TEST(DigitalDamageEffectsContractTest, GlitchRepeatsForFixedFrameAndSeed)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    GlitchEffect effect;
    effect.setIntensity(0.8f);
    effect.setColorShift(0.7f);
    effect.setScanlines(0.9f);
    effect.setSeed(37);

    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(original, nullptr);
    EXPECT_FALSE(differsFrom(first, second, kChannelCount));
    EXPECT_TRUE(differsFrom(first, pixels.data(), kChannelCount));
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        EXPECT_FLOAT_EQ(first[pixel + 3], pixels[pixel + 3])
            << "pixel=" << pixel / 4;
        EXPECT_FLOAT_EQ(original[pixel], pixels[pixel])
            << "source pixel=" << pixel / 4;
    }
}
