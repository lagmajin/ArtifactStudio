#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 8;
constexpr std::size_t kPixelCount =
    static_cast<std::size_t>(kWidth) * kHeight;

ArtifactCore::ImageF32x4RGBAWithCache makeBlockStep()
{
    std::array<float, kPixelCount * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            const float value = x < 8 ? 0.2f : 0.24f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] =
                0.1f + static_cast<float>(x + y) / 32.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(CreativeImageCleanupEffectContractTest,
     DeblockAveragesEligibleBlockBoundaryAndPreservesOtherPixelsAndAlpha)
{
    const auto source = makeBlockStep();
    Artifact::ArtifactDeblockEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("strength")),
                            QVariant(1.0));
    effect.setPropertyValue(
        ArtifactCore::UniString(QStringLiteral("threshold")), QVariant(0.05));
    effect.setPropertyValue(
        ArtifactCore::UniString(QStringLiteral("detailRecovery")), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("mix")),
                            QVariant(1.0));
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            const bool onBlockSeam = x == 7 || x == 8;
            const float expected = onBlockSeam ? 0.22f : original[offset];
            for (std::size_t channel = 0; channel < 3; ++channel) {
                EXPECT_NEAR(actual[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
            EXPECT_FLOAT_EQ(actual[offset + 3], original[offset + 3])
                << "x=" << x << " y=" << y;
        }
    }

    ArtifactCore::ImageF32x4RGBAWithCache thresholdedOutput;
    effect.setPropertyValue(
        ArtifactCore::UniString(QStringLiteral("threshold")), QVariant(0.03));
    effect.applyCPUOnly(source, thresholdedOutput);
    const float* thresholded = thresholdedOutput.image().rgba32fData();
    ASSERT_NE(thresholded, nullptr);
    for (std::size_t pixel = 0; pixel < kPixelCount; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(thresholded[pixel * 4 + channel],
                            original[pixel * 4 + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(thresholded[pixel * 4 + 3], original[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}
