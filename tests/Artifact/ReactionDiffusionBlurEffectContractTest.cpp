#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ReactionDiffusionBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 24;
constexpr int kHeight = 20;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth * kHeight * 4);

std::array<float, kChannelCount> makePatternImage()
{
    std::array<float, kChannelCount> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight - 1);
            pixels[offset + 2] = static_cast<float>((x * 3 + y * 5) % 23) / 22.0f;
            pixels[offset + 3] = 0.25f + static_cast<float>((x + y) % 8) * 0.08f;
        }
    }
    return pixels;
}

ImageF32x4RGBAWithCache makeSurface(const float* pixels)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

bool differsFrom(const float* lhs, const float* rhs)
{
    for (std::size_t channel = 0; channel < kChannelCount; ++channel) {
        if (std::abs(lhs[channel] - rhs[channel]) > 1.0e-6f) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(ReactionDiffusionBlurEffectContractTest, FixedSettingsAreRepeatableAndPreserveAlpha)
{
    const auto pixels = makePatternImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    ReactionDiffusionBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Blur Radius")), QVariant(3.0));
    effect.setPropertyValue(UniString(QStringLiteral("Pattern Strength")), QVariant(0.7));
    effect.setPropertyValue(UniString(QStringLiteral("Iterations")), QVariant(8));
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(37.0));

    effect.applyCPUOnly(source, firstOutput);
    effect.applyCPUOnly(source, secondOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* second = secondOutput.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(original, nullptr);
    EXPECT_FALSE(differsFrom(first, second));
    EXPECT_TRUE(differsFrom(first, pixels.data()));
    EXPECT_FALSE(differsFrom(original, pixels.data()));
    for (std::size_t pixel = 0; pixel < kChannelCount; pixel += 4) {
        EXPECT_FLOAT_EQ(first[pixel + 3], pixels[pixel + 3])
            << "pixel=" << pixel / 4;
    }
}

TEST(ReactionDiffusionBlurEffectContractTest, ZeroPatternStrengthPreservesInputRgbAndAlpha)
{
    const auto pixels = makePatternImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache output;
    ReactionDiffusionBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Blur Radius")), QVariant(3.0));
    effect.setPropertyValue(UniString(QStringLiteral("Pattern Strength")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Iterations")), QVariant(1));
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(91.0));

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], original[channel])
            << "channel=" << channel;
    }
}
