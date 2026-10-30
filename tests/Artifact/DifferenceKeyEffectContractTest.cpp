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
import Artifact.Effect.Keying.DifferenceKey;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

using namespace ArtifactCore::Keying;
using ArtifactCore::SurfaceAlphaMode;
using ArtifactCore::SurfaceChannelOrder;
using Artifact::DifferenceKeyEffect;
using ArtifactCore::ImageF32x4RGBAWithCache;
using ArtifactCore::ImageF32x4_RGBA;

namespace {

float alphaAt(const std::array<float, 12>& pixels, std::size_t pixelIndex)
{
    return pixels[pixelIndex * 4 + 3];
}

} // namespace

TEST(DifferenceKeyEffectContractTest, TreatsRgbaAndBgraPixelsEquivalently)
{
    const std::array<float, 8> rgbaSource{
        0.8f, 0.4f, 0.2f, 1.0f,
        0.1f, 0.6f, 0.9f, 0.7f};
    const std::array<float, 8> bgraSource{
        0.2f, 0.4f, 0.8f, 1.0f,
        0.9f, 0.6f, 0.1f, 0.7f};
    std::array<float, 8> differenceRgbaOutput{};
    std::array<float, 8> differenceBgraOutput{};
    DifferenceKeyParams differenceParams;
    differenceParams.refR = 0.2f;
    differenceParams.refG = 0.4f;
    differenceParams.refB = 0.8f;
    differenceParams.threshold = 0.15f;
    differenceParams.softness = 0.3f;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{rgbaSource.data(), differenceRgbaOutput.data(), 2, 1,
                             SurfaceChannelOrder::RGBA}, differenceParams));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{bgraSource.data(), differenceBgraOutput.data(), 2, 1,
                             SurfaceChannelOrder::BGRA}, differenceParams));

    for (std::size_t pixel = 0; pixel < 2; ++pixel) {
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4 + 3],
                    differenceBgraOutput[pixel * 4 + 3], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4],
                    differenceBgraOutput[pixel * 4 + 2], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4 + 1],
                    differenceBgraOutput[pixel * 4 + 1], 1.0e-6f);
        EXPECT_NEAR(differenceRgbaOutput[pixel * 4 + 2],
                    differenceBgraOutput[pixel * 4], 1.0e-6f);
    }
}

TEST(DifferenceKeyEffectContractTest, DifferenceKeyRemovesTheReferenceColor)
{
    const std::array<float, 12> source{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.9f, 0.08f, 0.04f, 1.0f,
        0.0f, 0.5f, 0.0f, 1.0f};
    std::array<float, 12> output{};
    DifferenceKeyParams params;
    params.refR = 0.0f;
    params.refG = 1.0f;
    params.refB = 0.0f;
    params.threshold = 0.08f;
    params.softness = 0.04f;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), output.data(), 3, 1}, params));

    EXPECT_LT(alphaAt(output, 0), 0.01f);
    EXPECT_GT(alphaAt(output, 1), 0.99f);
    EXPECT_GT(alphaAt(output, 2), 0.99f);
}

TEST(DifferenceKeyEffectContractTest, DifferenceKeySanitizesInvalidParametersLikeItsFiniteFallbacks)
{
    const std::array<float, 4> source{0.2f, 0.4f, 0.6f, 1.0f};
    std::array<float, 4> sanitizedOutput{};
    std::array<float, 4> explicitFallbackOutput{};
    DifferenceKeyParams invalid;
    invalid.refR = std::numeric_limits<float>::infinity();
    invalid.refG = -1.0f;
    invalid.refB = 2.0f;
    invalid.threshold = std::numeric_limits<float>::quiet_NaN();
    invalid.softness = std::numeric_limits<float>::infinity();
    invalid.choke = std::numeric_limits<float>::quiet_NaN();
    invalid.matteBlur = std::numeric_limits<float>::infinity();
    invalid.viewMode = 99;
    DifferenceKeyParams fallback;
    fallback.refR = 0.0f;
    fallback.refG = 0.0f;
    fallback.refB = 1.0f;
    fallback.threshold = 0.1f;
    fallback.softness = 0.08f;
    fallback.viewMode = 1;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), sanitizedOutput.data(), 1, 1}, invalid));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{source.data(), explicitFallbackOutput.data(), 1, 1}, fallback));

    for (std::size_t channel = 0; channel < sanitizedOutput.size(); ++channel) {
        EXPECT_NEAR(sanitizedOutput[channel], explicitFallbackOutput[channel], 1.0e-6f);
        EXPECT_TRUE(std::isfinite(sanitizedOutput[channel]));
    }
}

TEST(DifferenceKeyEffectContractTest, DifferenceKeyHandlesEquivalentStraightAndPremultipliedSamples)
{
    const std::array<float, 4> straightSource{0.8f, 0.2f, 0.1f, 0.5f};
    const std::array<float, 4> premultipliedSource{0.4f, 0.1f, 0.05f, 0.5f};
    std::array<float, 4> straightOutput{};
    std::array<float, 4> premultipliedOutput{};
    DifferenceKeyParams params;
    params.refR = 0.2f;
    params.refG = 0.2f;
    params.refB = 0.1f;
    params.threshold = 0.1f;
    params.softness = 0.8f;

    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{straightSource.data(), straightOutput.data(), 1, 1,
                             SurfaceChannelOrder::RGBA, SurfaceAlphaMode::Straight},
        params));
    ASSERT_TRUE(processDifferenceKey(
        DifferenceKeyBuffers{premultipliedSource.data(), premultipliedOutput.data(), 1, 1,
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

TEST(DifferenceKeyEffectContractTest, DifferenceKeyEffectPropagatesPremultipliedAlphaMode)
{
    const std::array<float, 4> straightPixels{0.15f, 0.0f, 0.0f, 0.5f};
    const std::array<float, 4> premultipliedPixels{0.075f, 0.0f, 0.0f, 0.5f};
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

    DifferenceKeyEffect effect;
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
