#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cmath>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.FilmDamage;
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
            const std::size_t offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                 static_cast<float>(kWidth + kHeight - 2);
            pixels[offset + 3] = 0.6f;
        }
    }
    return pixels;
}

ImageF32x4RGBAWithCache makeSurface(const float* rgba)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

bool differsFrom(const float* lhs, const float* rhs)
{
    for (std::size_t i = 0; i < kChannelCount; ++i) {
        if (std::abs(lhs[i] - rhs[i]) > 1.0e-6f) {
            return true;
        }
    }
    return false;
}

void configure(FilmDamageEffect& effect)
{
    effect.setPropertyValue(UniString(QStringLiteral("Grain")), QVariant(0.1));
    effect.setPropertyValue(UniString(QStringLiteral("Dust")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Scratches")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Gate Weave")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Flicker")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Film Burn")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(0.5));
    effect.setPropertyValue(UniString(QStringLiteral("Seed")), QVariant(23));
}

} // namespace

TEST(FilmDamageEffectContractTest, FixedSeedAndEvolutionAreRepeatableAndKeepAlpha)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    FilmDamageEffect effect;
    configure(effect);

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

TEST(FilmDamageEffectContractTest, EvolutionWithinOneFrameStepKeepsDamagePattern)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache sameFrameOutput;
    FilmDamageEffect firstEffect;
    FilmDamageEffect sameFrameEffect;
    configure(firstEffect);
    configure(sameFrameEffect);
    firstEffect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(0.10));
    sameFrameEffect.setPropertyValue(UniString(QStringLiteral("Evolution")), QVariant(0.12));

    firstEffect.applyCPUOnly(source, firstOutput);
    sameFrameEffect.applyCPUOnly(source, sameFrameOutput);

    const float* first = firstOutput.image().rgba32fData();
    const float* sameFrame = sameFrameOutput.image().rgba32fData();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(sameFrame, nullptr);
    EXPECT_FALSE(differsFrom(first, sameFrame));
}

TEST(FilmDamageEffectContractTest, ZeroDamageControlsPreserveEveryChannel)
{
    const auto pixels = makeGradientImage();
    const auto source = makeSurface(pixels.data());
    ImageF32x4RGBAWithCache output;
    FilmDamageEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Grain")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Dust")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Scratches")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Gate Weave")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Flicker")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Film Burn")), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    EXPECT_FALSE(differsFrom(actual, pixels.data()));
}
