#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <limits>

#include <QVariant>

import Artifact.Effect.Abstract;
import FillEffect;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 24;
constexpr int kHeight = 24;

ImageF32x4RGBAWithCache makeUniformSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.8f;
        pixels[i + 1] = 0.6f;
        pixels[i + 2] = 0.4f;
        pixels[i + 3] = 0.35f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(PatternOverlayEffectContractTest, SeededPatternIsRepeatableAndPreservesAlpha)
{
    const auto source = makeUniformSurface();
    PatternOverlayEffect effect;
    effect.setPropertyValue(UniString("Amount"), QVariant(0.8));
    effect.setPropertyValue(UniString("Scale"), QVariant(5.0));
    effect.setPropertyValue(UniString("Seed"), QVariant(17));
    effect.setPropertyValue(UniString("Pattern"), QVariant(0));

    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache second;
    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, second);

    bool changedRgb = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto a = first.image().getPixel(x, y);
            const auto b = second.image().getPixel(x, y);
            const auto original = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(a.r(), b.r());
            EXPECT_FLOAT_EQ(a.g(), b.g());
            EXPECT_FLOAT_EQ(a.b(), b.b());
            EXPECT_NEAR(a.a(), original.a(), 1.0e-6f);
            changedRgb = changedRgb || a.r() != original.r() ||
                         a.g() != original.g() || a.b() != original.b();
        }
    }
    EXPECT_TRUE(changedRgb);
}


TEST(PatternOverlayEffectContractTest, ZeroAmountPreservesEveryChannel)
{
    const auto source = makeUniformSurface();
    PatternOverlayEffect effect;
    effect.setAmount(0.0f);
    effect.setScale(3.0f);
    effect.setSeed(41);
    effect.setPattern(5);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto original = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), original.r());
            EXPECT_FLOAT_EQ(actual.g(), original.g());
            EXPECT_FLOAT_EQ(actual.b(), original.b());
            EXPECT_FLOAT_EQ(actual.a(), original.a());
        }
    }
}

TEST(PatternOverlayEffectContractTest, InvalidPatternAndNegativeSeedClampToSupportedValues)
{
    const auto source = makeUniformSurface();
    ImageF32x4RGBAWithCache clampedPatternOutput;
    ImageF32x4RGBAWithCache maximumPatternOutput;
    PatternOverlayEffect clampedPattern;
    PatternOverlayEffect maximumPattern;
    for (PatternOverlayEffect* effect : {&clampedPattern, &maximumPattern}) {
        effect->setAmount(0.75f);
        effect->setScale(4.0f);
        effect->setSeed(0);
    }
    clampedPattern.setPattern(99);
    maximumPattern.setPattern(6);
    clampedPattern.applyCPUOnly(source, clampedPatternOutput);
    maximumPattern.applyCPUOnly(source, maximumPatternOutput);

    const float* clampedPatternPixels = clampedPatternOutput.image().rgba32fData();
    const float* maximumPatternPixels = maximumPatternOutput.image().rgba32fData();
    ASSERT_NE(clampedPatternPixels, nullptr);
    ASSERT_NE(maximumPatternPixels, nullptr);
    for (std::size_t component = 0;
         component < static_cast<std::size_t>(kWidth * kHeight * 4); ++component) {
        EXPECT_FLOAT_EQ(clampedPatternPixels[component], maximumPatternPixels[component])
            << "pattern component=" << component;
    }

    ImageF32x4RGBAWithCache clampedSeedOutput;
    ImageF32x4RGBAWithCache zeroSeedOutput;
    PatternOverlayEffect clampedSeed;
    PatternOverlayEffect zeroSeed;
    for (PatternOverlayEffect* effect : {&clampedSeed, &zeroSeed}) {
        effect->setAmount(0.75f);
        effect->setScale(4.0f);
        effect->setPattern(2);
    }
    clampedSeed.setSeed(-7);
    zeroSeed.setSeed(0);
    clampedSeed.applyCPUOnly(source, clampedSeedOutput);
    zeroSeed.applyCPUOnly(source, zeroSeedOutput);

    const float* clampedSeedPixels = clampedSeedOutput.image().rgba32fData();
    const float* zeroSeedPixels = zeroSeedOutput.image().rgba32fData();
    ASSERT_NE(clampedSeedPixels, nullptr);
    ASSERT_NE(zeroSeedPixels, nullptr);
    for (std::size_t component = 0;
         component < static_cast<std::size_t>(kWidth * kHeight * 4); ++component) {
        EXPECT_FLOAT_EQ(clampedSeedPixels[component], zeroSeedPixels[component])
            << "seed component=" << component;
    }
}

TEST(PatternOverlayEffectContractTest, NonFiniteAmountAndScaleUseTheirDefaults)
{
    const auto source = makeUniformSurface();
    PatternOverlayEffect nonFiniteValues;
    PatternOverlayEffect explicitDefaults;
    nonFiniteValues.setAmount(std::numeric_limits<float>::quiet_NaN());
    nonFiniteValues.setScale(std::numeric_limits<float>::infinity());
    nonFiniteValues.setPattern(3);
    explicitDefaults.setAmount(0.5f);
    explicitDefaults.setScale(8.0f);
    explicitDefaults.setPattern(3);

    ImageF32x4RGBAWithCache actual;
    ImageF32x4RGBAWithCache expected;
    nonFiniteValues.applyCPUOnly(source, actual);
    explicitDefaults.applyCPUOnly(source, expected);

    const float* actualPixels = actual.image().rgba32fData();
    const float* expectedPixels = expected.image().rgba32fData();
    ASSERT_NE(actualPixels, nullptr);
    ASSERT_NE(expectedPixels, nullptr);
    for (std::size_t component = 0;
         component < static_cast<std::size_t>(kWidth * kHeight * 4); ++component) {
        EXPECT_FLOAT_EQ(actualPixels[component], expectedPixels[component])
            << "component=" << component;
    }
}
