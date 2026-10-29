#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Glitch;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 12;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeColorPattern()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[offset] = 0.05f * static_cast<float>(x + 1) +
                             0.001f * static_cast<float>(y);
            pixels[offset + 1] = 0.2f + 0.01f * static_cast<float>(y);
            pixels[offset + 2] = 0.9f - 0.03f * static_cast<float>(x) -
                                 0.001f * static_cast<float>(y);
            pixels[offset + 3] = 0.1f + 0.02f * static_cast<float>(x);
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(GlitchEffectContractTest, SeededChannelShiftIsRepeatableAndPreservesAlpha)
{
    const auto source = makeColorPattern();
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache second;
    Artifact::GlitchEffect effect;
    effect.setIntensity(0.0f);
    effect.setScanlines(0.0f);
    effect.setColorShift(1.0f);
    effect.setSeed(73);

    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, second);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto input = source.image().getPixel(x, y);
            const auto a = first.image().getPixel(x, y);
            const auto b = second.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(a.r(), b.r());
            EXPECT_FLOAT_EQ(a.g(), b.g());
            EXPECT_FLOAT_EQ(a.b(), b.b());
            EXPECT_FLOAT_EQ(a.a(), input.a());

            if (x == 0 || x == kWidth - 1) {
                EXPECT_FLOAT_EQ(a.r(), input.r());
                EXPECT_FLOAT_EQ(a.b(), input.b());
            } else {
                const bool unchanged = a.r() == input.r() && a.b() == input.b();
                const bool swapped = a.r() == input.b() && a.b() == input.r();
                EXPECT_TRUE(unchanged || swapped);
            }
            EXPECT_FLOAT_EQ(a.g(), input.g());
        }
    }
}

TEST(GlitchEffectContractTest, SettersClampAmountsAndSeed)
{
    Artifact::GlitchEffect effect;
    effect.setIntensity(-1.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 0.0f);
    effect.setIntensity(2.0f);
    EXPECT_FLOAT_EQ(effect.intensity(), 1.0f);
    effect.setIntensity(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.intensity(), 0.3f);

    effect.setColorShift(-1.0f);
    EXPECT_FLOAT_EQ(effect.colorShift(), 0.0f);
    effect.setColorShift(2.0f);
    EXPECT_FLOAT_EQ(effect.colorShift(), 1.0f);
    effect.setColorShift(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.colorShift(), 0.5f);

    effect.setScanlines(-1.0f);
    EXPECT_FLOAT_EQ(effect.scanlines(), 0.0f);
    effect.setScanlines(2.0f);
    EXPECT_FLOAT_EQ(effect.scanlines(), 1.0f);
    effect.setScanlines(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.scanlines(), 0.5f);

    effect.setSeed(-1);
    EXPECT_EQ(effect.seed(), 0);
    effect.setSeed(10000);
    EXPECT_EQ(effect.seed(), 9999);
}
