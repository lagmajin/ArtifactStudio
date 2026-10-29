#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VectorBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeImage(
    const std::array<float, 8>& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPixelsEqual(
    const ArtifactCore::ImageF32x4RGBAWithCache& image,
    const std::array<float, 8>& expected)
{
    const float* actual = image.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
    }
}

} // namespace

TEST(VectorBlurEffectContractTest, ZeroShutterPreservesInputWithoutHistory)
{
    const std::array<float, 8> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.9f, 0.8f, 0.7f, 0.6f,
    };
    const auto source = makeImage(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::VectorBlurEffect effect;
    effect.setShutterAngle(0.0f);

    effect.applyCPUOnly(source, output);

    expectPixelsEqual(output, pixels);
}

TEST(VectorBlurEffectContractTest, MissingHistoryPreservesInput)
{
    const std::array<float, 8> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.9f, 0.8f, 0.7f, 0.6f,
    };
    const auto source = makeImage(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::VectorBlurEffect effect;
    effect.setShutterAngle(180.0f);

    effect.applyCPUOnly(source, output);

    expectPixelsEqual(output, pixels);
}

TEST(VectorBlurEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::VectorBlurEffect effect;
    effect.setShutterAngle(-10.0f);
    EXPECT_FLOAT_EQ(effect.shutterAngle(), 0.0f);
    effect.setShutterAngle(900.0f);
    EXPECT_FLOAT_EQ(effect.shutterAngle(), 720.0f);
    effect.setShutterAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.shutterAngle(), 180.0f);

    effect.setSamples(0);
    EXPECT_EQ(effect.samples(), 2);
    effect.setSamples(40);
    EXPECT_EQ(effect.samples(), 32);

    effect.setExposureCompensation(-1.0f);
    EXPECT_FLOAT_EQ(effect.exposureCompensation(), 0.0f);
    effect.setExposureCompensation(10.0f);
    EXPECT_FLOAT_EQ(effect.exposureCompensation(), 4.0f);
    effect.setExposureCompensation(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.exposureCompensation(), 1.0f);
}