#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QRect>
#include <QVector>

import Artifact.Effect.AutoMosaic;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 12;
constexpr int kHeight = 12;

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            pixels[offset] = static_cast<float>(x) / static_cast<float>(kWidth - 1);
            pixels[offset + 1] = static_cast<float>(y) / static_cast<float>(kHeight - 1);
            pixels[offset + 2] = static_cast<float>(x + y) /
                                 static_cast<float>(kWidth + kHeight - 2);
            pixels[offset + 3] = 0.7f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(AutoMosaicEffectContractTest, PixelatesOnlyConfiguredCustomRegion)
{
    const auto source = makePatternSurface();
    AutoMosaicEffect effect;
    effect.setUseFaceDetection(false);
    effect.setUseCustomRegions(true);
    effect.setMosaicType(MosaicType::Pixelate);
    effect.setMosaicStrength(4);
    effect.setCustomRegions(QVector<QRect>{QRect(0, 0, 8, 8)});
    ImageF32x4RGBAWithCache output;
    effect.apply(source, output);

    for (int blockY = 0; blockY < 2; ++blockY) {
        for (int blockX = 0; blockX < 2; ++blockX) {
            const auto first = output.image().getPixel(blockX * 4, blockY * 4);
            for (int y = blockY * 4; y < blockY * 4 + 4; ++y) {
                for (int x = blockX * 4; x < blockX * 4 + 4; ++x) {
                    const auto pixel = output.image().getPixel(x, y);
                    EXPECT_NEAR(pixel.r(), first.r(), 1.0e-6f);
                    EXPECT_NEAR(pixel.g(), first.g(), 1.0e-6f);
                    EXPECT_NEAR(pixel.b(), first.b(), 1.0e-6f);
                    EXPECT_NEAR(pixel.a(), first.a(), 1.0e-6f);
                }
            }
        }
    }

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            if (x < 8 && y < 8) continue;
            const auto actual = output.image().getPixel(x, y);
            const auto original = source.image().getPixel(x, y);
            EXPECT_NEAR(actual.r(), original.r(), 1.0e-6f);
            EXPECT_NEAR(actual.g(), original.g(), 1.0e-6f);
            EXPECT_NEAR(actual.b(), original.b(), 1.0e-6f);
            EXPECT_NEAR(actual.a(), original.a(), 1.0e-6f);
        }
    }
}


TEST(AutoMosaicEffectContractTest, DisabledDetectionAndNoCustomRegionsPreserveInput)
{
    const auto source = makePatternSurface();
    AutoMosaicEffect effect;
    effect.setUseFaceDetection(false);
    effect.setUseCustomRegions(false);
    ImageF32x4RGBAWithCache output;

    effect.apply(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel <
         static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], original[channel])
            << "channel=" << channel;
    }
}

TEST(AutoMosaicEffectContractTest, StrengthAndFeatherClampToSupportedMinimums)
{
    AutoMosaicEffect effect;
    effect.setMosaicStrength(0);
    effect.setFeather(-2.0f);

    EXPECT_EQ(effect.mosaicStrength(), 1);
    EXPECT_FLOAT_EQ(effect.feather(), 0.0f);
}

TEST(AutoMosaicEffectContractTest, ClipsCustomRegionAtImageBounds)
{
    const auto source = makePatternSurface();
    AutoMosaicEffect effect;
    effect.setUseFaceDetection(false);
    effect.setUseCustomRegions(true);
    effect.setMosaicType(MosaicType::Pixelate);
    effect.setMosaicStrength(4);
    effect.setCustomRegions(QVector<QRect>{QRect(10, 10, 5, 5)});
    ImageF32x4RGBAWithCache output;

    effect.apply(source, output);

    const auto affected = output.image().getPixel(11, 11);
    const auto originalAffected = source.image().getPixel(11, 11);
    EXPECT_NE(affected.r(), originalAffected.r());
    EXPECT_NEAR(output.image().getPixel(9, 11).r(),
                source.image().getPixel(9, 11).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(11, 9).g(),
                source.image().getPixel(11, 9).g(), 1.0e-6f);
}
