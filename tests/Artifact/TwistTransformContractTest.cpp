#include <gtest/gtest.h>

#include <array>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Transform.Twist;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 3;

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(
    const std::array<float, kWidth * kHeight * 4>& rgba)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(TwistTransformContractTest, ZeroAnglePreservesEveryRgbaPixel)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<float>(i) / static_cast<float>(pixels.size());
    }
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::TwistTransform effect;
    effect.setAngle(0.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            EXPECT_FLOAT_EQ(actual.r(), pixels[offset]);
            EXPECT_FLOAT_EQ(actual.g(), pixels[offset + 1]);
            EXPECT_FLOAT_EQ(actual.b(), pixels[offset + 2]);
            EXPECT_FLOAT_EQ(actual.a(), pixels[offset + 3]);
        }
    }
}

TEST(TwistTransformContractTest, SettersClampFiniteValuesAndResetNonFiniteValues)
{
    Artifact::TwistTransform effect;
    effect.setAngle(900.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 720.0f);
    effect.setAngle(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.angle(), 45.0f);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);

    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setCenterY(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
}
