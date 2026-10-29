#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QString>

import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import LUTEffect;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 2;

ImageF32x4RGBAWithCache makeMidGraySurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.5f;
        pixels[i + 1] = 0.5f;
        pixels[i + 2] = 0.5f;
        pixels[i + 3] = 0.4f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(LUTEffectContractTest, BuiltinWarmLutTransformsRgbAndPreservesAlpha)
{
    const auto source = makeMidGraySurface();
    ImageF32x4RGBAWithCache output;
    LUTEffect effect;
    effect.setPreset(static_cast<int>(LUTEffect::Preset::Warm));
    effect.setIntensity(1.0f);
    effect.applyCPUOnly(source, output);

    const auto before = source.image().getPixel(1, 1);
    const auto after = output.image().getPixel(1, 1);
    EXPECT_NEAR(after.r(), before.r() * 1.2f, 2.0e-3f);
    EXPECT_NEAR(after.g(), before.g() * 1.05f, 2.0e-3f);
    EXPECT_NEAR(after.b(), before.b() * 0.8f, 2.0e-3f);
    EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);

    effect.setIntensity(0.0f);
    ImageF32x4RGBAWithCache identityOutput;
    effect.applyCPUOnly(source, identityOutput);
    const auto identity = identityOutput.image().getPixel(1, 1);
    EXPECT_FLOAT_EQ(identity.r(), before.r());
    EXPECT_FLOAT_EQ(identity.g(), before.g());
    EXPECT_FLOAT_EQ(identity.b(), before.b());
    EXPECT_FLOAT_EQ(identity.a(), before.a());
}


TEST(LUTEffectContractTest, ZeroIntensityPreservesEveryPixelOfNonuniformImage)
{
    constexpr std::array<float, kWidth * kHeight * 4> pixels = {
        0.0f, 0.1f, 0.2f, 0.0f,
        0.2f, 0.3f, 0.4f, 0.2f,
        0.4f, 0.5f, 0.6f, 0.4f,
        0.6f, 0.7f, 0.8f, 0.6f,
        0.8f, 0.9f, 1.0f, 0.8f,
        1.0f, 0.9f, 0.7f, 1.0f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    LUTEffect effect;
    effect.setPreset(static_cast<int>(LUTEffect::Preset::Warm));
    effect.setIntensity(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
    EXPECT_FLOAT_EQ(source.image().rgba32fData()[0], pixels[0]);
}

TEST(LUTEffectContractTest, EmptyCustomLutLeavesTheImageUnchanged)
{
    constexpr std::array<float, kWidth * kHeight * 4> pixels = {
        0.0f, 0.1f, 0.2f, 0.0f,
        0.2f, 0.3f, 0.4f, 0.2f,
        0.4f, 0.5f, 0.6f, 0.4f,
        0.6f, 0.7f, 0.8f, 0.6f,
        0.8f, 0.9f, 1.0f, 0.8f,
        1.0f, 0.9f, 0.7f, 1.0f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    LUTEffect effect;
    effect.setPreset(static_cast<int>(LUTEffect::Preset::Custom));
    effect.setFilePath(QString());
    effect.setIntensity(1.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(LUTEffectContractTest, PresetSetterClampsToAvailablePresetIndices)
{
    LUTEffect effect;

    effect.setPreset(-3);
    auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 3u);
    EXPECT_EQ(properties[0].getValue().toInt(), 0);

    effect.setPreset(25);
    properties = effect.getProperties();
    EXPECT_EQ(properties[0].getValue().toInt(), 9);
}
