#include <gtest/gtest.h>
#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

import ImageProcessing;
import Graphics.SurfaceColorContract;
import Artifact.Effect.Abstract;
import Artifact.Effect.Keying.LumaKey;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

using namespace ArtifactCore::Keying;
using ArtifactCore::SurfaceAlphaMode;
using ArtifactCore::SurfaceChannelOrder;
using Artifact::LumaKeyEffect;
using ArtifactCore::ImageF32x4RGBAWithCache;
using ArtifactCore::ImageF32x4_RGBA;

namespace {

float alphaAt(const std::array<float, 12>& pixels, std::size_t pixelIndex)
{
    return pixels[pixelIndex * 4 + 3];
}

} // namespace

TEST(LumaKeyEffectContractTest, TreatsRgbaAndBgraPixelsEquivalently)
{
    const std::array<float, 8> rgbaSource{
        0.8f, 0.4f, 0.2f, 1.0f,
        0.1f, 0.6f, 0.9f, 0.7f};
    const std::array<float, 8> bgraSource{
        0.2f, 0.4f, 0.8f, 1.0f,
        0.9f, 0.6f, 0.1f, 0.7f};
    std::array<float, 8> lumaRgbaOutput{};
    std::array<float, 8> lumaBgraOutput{};
    LumaKeyParams lumaParams;
    lumaParams.low = 0.25f;
    lumaParams.high = 0.75f;
    lumaParams.softness = 0.2f;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{rgbaSource.data(), lumaRgbaOutput.data(), 2, 1,
                       SurfaceChannelOrder::RGBA}, lumaParams));
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{bgraSource.data(), lumaBgraOutput.data(), 2, 1,
                       SurfaceChannelOrder::BGRA}, lumaParams));
    for (std::size_t pixel = 0; pixel < 2; ++pixel) {
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4 + 3],
                    lumaBgraOutput[pixel * 4 + 3], 1.0e-6f);
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4],
                    lumaBgraOutput[pixel * 4 + 2], 1.0e-6f);
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4 + 1],
                    lumaBgraOutput[pixel * 4 + 1], 1.0e-6f);
        EXPECT_NEAR(lumaRgbaOutput[pixel * 4 + 2],
                    lumaBgraOutput[pixel * 4], 1.0e-6f);
    }
}

TEST(LumaKeyEffectContractTest, RejectsAliasedOrEmptyBuffersWithoutChangingInput)
{
    std::array<float, 4> pixels{0.25f, 0.5f, 0.75f, 1.0f};
    const auto original = pixels;
    LumaKeyParams params;

    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), pixels.data(), 1, 1}, params));
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), nullptr, 1, 1}, params));
    EXPECT_FALSE(processLumaKey(
        LumaKeyBuffers{pixels.data(), pixels.data(), 0, 1}, params));
    EXPECT_EQ(pixels, original);
}

TEST(LumaKeyEffectContractTest, LumaKeySelectsTheConfiguredBrightnessBand)
{
    const std::array<float, 12> source{
        0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 12> output{};
    LumaKeyParams params;
    params.low = 0.3f;
    params.high = 0.7f;
    params.softness = 0.05f;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{source.data(), output.data(), 3, 1}, params));

    EXPECT_LT(alphaAt(output, 0), 0.01f);
    EXPECT_GT(alphaAt(output, 1), 0.99f);
    EXPECT_LT(alphaAt(output, 2), 0.01f);
}

TEST(LumaKeyEffectContractTest, LumaKeySwapsReversedThresholdsAndClampsViewMode)
{
    const std::array<float, 4> source{0.5f, 0.5f, 0.5f, 1.0f};
    std::array<float, 4> output{};
    LumaKeyParams params;
    params.low = 0.8f;
    params.high = 0.2f;
    params.softness = 0.4f;
    params.viewMode = 99;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{source.data(), output.data(), 1, 1}, params));

    EXPECT_NEAR(output[0], 0.75f, 1.0e-6f);
    EXPECT_NEAR(output[1], 0.75f, 1.0e-6f);
    EXPECT_NEAR(output[2], 0.75f, 1.0e-6f);
    EXPECT_FLOAT_EQ(output[3], 1.0f);
}

TEST(LumaKeyEffectContractTest, LumaKeyHandlesEquivalentStraightAndPremultipliedSamples)
{
    const std::array<float, 4> straightSource{0.8f, 0.7f, 0.2f, 0.5f};
    const std::array<float, 4> premultipliedSource{0.4f, 0.35f, 0.1f, 0.5f};
    std::array<float, 4> straightOutput{};
    std::array<float, 4> premultipliedOutput{};
    LumaKeyParams params;
    params.low = 0.2f;
    params.high = 0.8f;
    params.softness = 0.2f;

    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{straightSource.data(), straightOutput.data(), 1, 1,
                       SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Straight},
        params));
    ASSERT_TRUE(processLumaKey(
        LumaKeyBuffers{premultipliedSource.data(), premultipliedOutput.data(), 1, 1,
                       SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Premultiplied},
        params));

    EXPECT_GT(straightOutput[3], 0.0f);
    EXPECT_LT(straightOutput[3], straightSource[3]);
    EXPECT_NEAR(straightOutput[3], premultipliedOutput[3], 1.0e-6f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(premultipliedOutput[channel],
                    straightOutput[channel] * straightOutput[3], 1.0e-6f);
    }
}

TEST(LumaKeyEffectContractTest, LumaKeyEffectPropagatesPremultipliedAlphaMode)
{
    const std::array<float, 4> straightPixels{0.2f, 0.2f, 0.2f, 0.5f};
    const std::array<float, 4> premultipliedPixels{0.1f, 0.1f, 0.1f, 0.5f};
    ImageF32x4_RGBA straightImage;
    ImageF32x4_RGBA premultipliedImage;
    straightImage.setFromRGBA32F(straightPixels.data(), 1, 1);
    premultipliedImage.setFromRGBA32F(premultipliedPixels.data(), 1, 1);
    auto straightDescriptor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();
    straightDescriptor.alphaMode = SurfaceAlphaMode::Straight;
    straightImage.setColorDescriptor(straightDescriptor);
    premultipliedImage.setColorDescriptor(
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied());

    LumaKeyEffect effect;
    ImageF32x4RGBAWithCache straightOutput;
    ImageF32x4RGBAWithCache premultipliedOutput;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(straightImage), straightOutput);
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(premultipliedImage), premultipliedOutput);

    const auto straightPixel = straightOutput.image().getPixel(0, 0);
    const auto premultipliedPixel = premultipliedOutput.image().getPixel(0, 0);
    EXPECT_NEAR(straightPixel.a(), premultipliedPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.r(), straightPixel.r() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.g(), straightPixel.g() * straightPixel.a(), 1.0e-6f);
    EXPECT_NEAR(premultipliedPixel.b(), straightPixel.b() * straightPixel.a(), 1.0e-6f);
    EXPECT_EQ(premultipliedOutput.image().colorDescriptor().alphaMode,
              SurfaceAlphaMode::Premultiplied);
}
