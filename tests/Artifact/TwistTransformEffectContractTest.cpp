#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Transform.Twist;
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

TEST(TwistTransformEffectContractTest, TwistsAroundConfiguredCenterAndPreservesCenter)
{
    const auto source = makeCoordinateSurface();
    ImageF32x4RGBAWithCache output;
    TwistTransform effect;
    effect.setAngle(120.0f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
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
    EXPECT_NEAR(output.image().getPixel(12, 12).r(),
                source.image().getPixel(12, 12).r(), 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(6, 6).g(), 0.25f, 1.0e-6f);
}

TEST(TwistTransformEffectContractTest, ZeroAnglePreservesEveryInputChannel)
{
    const auto source = makeCoordinateSurface();
    ImageF32x4RGBAWithCache output;
    TwistTransform effect;
    effect.setAngle(0.0f);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_NEAR(after.r(), before.r(), 1.0e-6f);
            EXPECT_NEAR(after.g(), before.g(), 1.0e-6f);
            EXPECT_NEAR(after.b(), before.b(), 1.0e-6f);
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
}

TEST(TwistTransformEffectContractTest, CornerCenterWithZeroMaximumRadiusPreservesInput)
{
    const auto source = makeCoordinateSurface();
    ImageF32x4RGBAWithCache output;
    TwistTransform effect;
    effect.setAngle(120.0f);
    effect.setCenterX(0.0f);
    effect.setCenterY(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}
\n
