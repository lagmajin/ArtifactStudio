#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.PixelSort;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeAscendingRows()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            const float value = 0.1f + 0.1f * static_cast<float>(x);
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.2f + 0.1f * static_cast<float>(x);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(PixelSortProEffectContractTest, SortsThresholdedSegmentsAndPreservesAlpha)
{
    const auto source = makeAscendingRows();
    ImageF32x4RGBAWithCache output;
    PixelSortProEffect effect;
    effect.setPropertyValue(UniString("angle"), QVariant(0.0));
    effect.setPropertyValue(UniString("lowThreshold"), QVariant(0.0));
    effect.setPropertyValue(UniString("highThreshold"), QVariant(1.0));
    effect.setPropertyValue(UniString("descending"), QVariant(1.0));
    effect.setPropertyValue(UniString("edgeProtection"), QVariant(0.0));
    effect.setPropertyValue(UniString("mix"), QVariant(1.0));

    effect.applyCPUOnly(source, output);

    EXPECT_GT(output.image().getPixel(0, 0).r(),
              output.image().getPixel(kWidth - 1, 0).r());
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            EXPECT_NEAR(output.image().getPixel(x, y).a(),
                        source.image().getPixel(x, y).a(), 1.0e-6f);
        }
    }
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.1f, 1.0e-6f);
}

TEST(PixelSortProEffectContractTest, ZeroMixPreservesImageWhenSortingIsConfigured)
{
    const auto source = makeAscendingRows();
    ImageF32x4RGBAWithCache output;
    PixelSortProEffect effect;
    effect.setPropertyValue(UniString("angle"), QVariant(0.0));
    effect.setPropertyValue(UniString("lowThreshold"), QVariant(0.0));
    effect.setPropertyValue(UniString("highThreshold"), QVariant(1.0));
    effect.setPropertyValue(UniString("descending"), QVariant(1.0));
    effect.setPropertyValue(UniString("edgeProtection"), QVariant(0.0));
    effect.setPropertyValue(UniString("mix"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

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
\n
