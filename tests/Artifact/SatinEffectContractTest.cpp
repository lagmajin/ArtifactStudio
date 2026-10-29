#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Satin;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 6;
constexpr int kHeight = 5;

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / static_cast<float>(kWidth + 1);
            pixels[offset + 1] = static_cast<float>(y + 1) / static_cast<float>(kHeight + 1);
            pixels[offset + 2] = static_cast<float>(x + y + 1) /
                                 static_cast<float>(kWidth + kHeight + 1);
            pixels[offset + 3] = 0.25f + static_cast<float>(x + y) * 0.05f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(SatinEffectContractTest, ZeroOpacityPreservesSourceColorAndAlpha)
{
    const auto source = makePatternSurface();
    SatinEffect effect;
    effect.setOpacity(0.0f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f);
            EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f);
            EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f);
            EXPECT_NEAR(actual.a(), expected.a(), 1.0e-6f);
        }
    }
}

TEST(SatinEffectContractTest, InvertedMaskAddsSatinAlphaOutsideSourceShape)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    const auto centerOffset = static_cast<std::size_t>(((kHeight / 2) * kWidth + kWidth / 2) * 4);
    pixels[centerOffset] = 0.4f;
    pixels[centerOffset + 1] = 0.5f;
    pixels[centerOffset + 2] = 0.6f;
    pixels[centerOffset + 3] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    SatinEffect effect;
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);
    effect.setInvert(true);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 180.0f / 255.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(kWidth / 2, kHeight / 2).a(),
                1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
}

TEST(SatinEffectContractTest, SettersClampNegativeValuesAndRestoreNonFiniteDefaults)
{
    SatinEffect effect;
    effect.setDistance(-3.0f);
    effect.setSoftness(-1.0f);
    effect.setOpacity(150.0f);
    EXPECT_FLOAT_EQ(effect.distance(), 0.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 0.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);

    effect.setDistance(std::numeric_limits<float>::infinity());
    effect.setAngle(std::numeric_limits<float>::quiet_NaN());
    effect.setSoftness(std::numeric_limits<float>::quiet_NaN());
    effect.setOpacity(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.distance(), 0.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 5.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 50.0f);
}
\n
