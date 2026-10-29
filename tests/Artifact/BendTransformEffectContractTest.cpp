#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Transform.Bend;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 25;
constexpr int kHeight = 25;

ImageF32x4RGBAWithCache makeCoordinateSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            pixels[offset] = static_cast<float>(x) / (kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / (kHeight - 1);
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(BendTransformEffectContractTest, NonzeroBendRemapsCoordinatesAndKeepsAlpha)
{
    const auto source = makeCoordinateSurface();
    ImageF32x4RGBAWithCache output;
    BendTransform effect;
    effect.setAngle(2.0f);
    effect.setDirection(0.0f);
    effect.setSize(20.0f);
    effect.applyCPUOnly(source, output);

    bool changedRgb = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            changedRgb = changedRgb || std::abs(after.r() - before.r()) > 1.0e-4f ||
                         std::abs(after.g() - before.g()) > 1.0e-4f;
            EXPECT_NEAR(after.a(), 1.0f, 1.0e-6f);
        }
    }
    EXPECT_TRUE(changedRgb);
    EXPECT_NEAR(source.image().getPixel(12, 12).r(), 0.5f, 1.0e-6f);
}

TEST(BendTransformEffectContractTest, ZeroAnglePreservesTheWholeImage)
{
    const auto source = makeCoordinateSurface();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ImageF32x4RGBAWithCache output;
    BendTransform effect;
    effect.setAngle(0.0f);
    effect.setDirection(37.0f);
    effect.setSize(20.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(outputPixels[channel], sourcePixels[channel])
            << "channel=" << channel;
    }
}

TEST(BendTransformEffectContractTest, LargeBendKeepsOutputFiniteAndWithinSourceBounds)
{
    const auto source = makeCoordinateSurface();
    ImageF32x4RGBAWithCache output;
    BendTransform effect;
    effect.setAngle(720.0f);
    effect.setDirection(135.0f);
    effect.setSize(0.01f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_TRUE(std::isfinite(actual[channel])) << "channel=" << channel;
        EXPECT_GE(actual[channel], 0.0f) << "channel=" << channel;
        EXPECT_LE(actual[channel], 1.0f) << "channel=" << channel;
    }
}
