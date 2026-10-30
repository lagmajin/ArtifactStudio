#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cmath>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Kaleidoscope;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 6;

ImageF32x4RGBAWithCache makeUniqueAlphaSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = static_cast<float>(y * kWidth + x) /
                                static_cast<float>(kWidth * kHeight - 1);
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = value * 0.5f;
            pixels[offset + 3] = 0.1f + value * 0.8f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(RasterizerKaleidoscopeEffectContractTest, PreservesDimensionsAndSamplesSourceAlpha)
{
    const auto source = makeUniqueAlphaSurface();
    RasterizerKaleidoscopeEffect effect;
    effect.setSegments(5);
    effect.setRotation(17.0f);
    effect.setCenterX(0.45f);
    effect.setCenterY(0.6f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.width(), kWidth);
    ASSERT_EQ(output.height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const float alpha = output.image().getPixel(x, y).a();
            bool matchesSourceSample = false;
            for (int sourceY = 0; sourceY < kHeight; ++sourceY) {
                for (int sourceX = 0; sourceX < kWidth; ++sourceX) {
                    const float candidate = source.image().getPixel(sourceX, sourceY).a();
                    matchesSourceSample |= std::abs(alpha - candidate) < 1.0e-6f;
                }
            }
            EXPECT_TRUE(matchesSourceSample) << "x=" << x << " y=" << y;
        }
    }
}

TEST(RasterizerKaleidoscopeEffectContractTest, CenterPixelRemainsUnchangedAcrossSegmentsAndRotation)
{
    const auto source = makeUniqueAlphaSurface();
    RasterizerKaleidoscopeEffect effect;
    effect.setSegments(7);
    effect.setRotation(123.0f);
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    const auto actual = output.image().getPixel(kWidth / 2, kHeight / 2);
    const auto expected = source.image().getPixel(kWidth / 2, kHeight / 2);
    EXPECT_FLOAT_EQ(actual.r(), expected.r());
    EXPECT_FLOAT_EQ(actual.g(), expected.g());
    EXPECT_FLOAT_EQ(actual.b(), expected.b());
    EXPECT_FLOAT_EQ(actual.a(), expected.a());
}

TEST(RasterizerKaleidoscopeEffectContractTest, SegmentCountAndCenterCoordinatesClamp)
{
    RasterizerKaleidoscopeEffect effect;
    effect.setSegments(1);
    effect.setCenterX(-0.5f);
    effect.setCenterY(1.5f);
    EXPECT_EQ(effect.segments(), 2);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);

    effect.setSegments(40);
    effect.setCenterX(0.25f);
    effect.setCenterY(0.75f);
    EXPECT_EQ(effect.segments(), 32);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.25f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.75f);
}
