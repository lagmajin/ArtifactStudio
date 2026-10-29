#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Generate.SimpleRain;
import Artifact.Effect.Rasterizer.RadialBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

bool differsFrom(const float* lhs, const float* rhs, std::size_t count)
{
    for (std::size_t index = 0; index < count; ++index) {
        if (std::abs(lhs[index] - rhs[index]) > 1.0e-6f) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(ProceduralImageEffectsContractTest, SimpleRainRepeatsForFixedSeedAndPreservesAlpha)
{
    constexpr int width = 256;
    constexpr int height = 256;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width * height * 4);
    std::vector<float> pixels(channelCount, 0.0f);
    for (std::size_t pixel = 0; pixel < channelCount; pixel += 4) {
        pixels[pixel + 3] = 0.7f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    SimpleRainEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Density")), QVariant(0.9));
    effect.setPropertyValue(UniString(QStringLiteral("Streak Length")), QVariant(55.0));
    effect.setPropertyValue(UniString(QStringLiteral("Wind")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Opacity")), QVariant(0.6));
    effect.setPropertyValue(UniString(QStringLiteral("Splash Amount")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(0.25));
    effect.setPropertyValue(UniString(QStringLiteral("Seed")), QVariant(87));

    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(original, nullptr);
    EXPECT_FALSE(differsFrom(first, second, channelCount));
    EXPECT_TRUE(differsFrom(first, pixels.data(), channelCount));
    for (std::size_t pixel = 0; pixel < channelCount; pixel += 4) {
        EXPECT_FLOAT_EQ(first[pixel + 3], 0.7f) << "pixel=" << pixel / 4;
        EXPECT_FLOAT_EQ(original[pixel], pixels[pixel])
            << "source pixel=" << pixel / 4;
    }
}

TEST(ProceduralImageEffectsContractTest, RadialBlurPullsSamplesOutwardAndClampsAtEdges)
{
    constexpr int width = 9;
    constexpr int height = 9;
    constexpr std::size_t channelCount =
        static_cast<std::size_t>(width * height * 4);
    std::array<float, channelCount> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x) / 8.0f;
            pixels[offset + 1] = 0.3f;
            pixels[offset + 2] = 0.5f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RadialBlurEffect effect;
    effect.setAmount(1.0f);
    effect.setQuality(8);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);

    effect.applyCPUOnly(source, output);

    const auto original = source.image().getPixel(2, 4);
    const auto blurred = output.image().getPixel(2, 4);
    EXPECT_LT(blurred.r(), original.r());
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 4).r(), 0.0f);
    EXPECT_FLOAT_EQ(blurred.a(), 1.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(2, 4).r(), original.r());
}
