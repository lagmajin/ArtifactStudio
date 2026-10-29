#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Sharpen;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 33;
constexpr int kHeight = 33;

ImageF32x4RGBAWithCache makeFineDetailSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            const float value = ((x + y) % 2 == 0) ? 0.8f : 0.2f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.45f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(MagicSharpEffectContractTest, EnhancesFineDetailAndPreservesAlpha)
{
    const auto source = makeFineDetailSurface();
    ImageF32x4RGBAWithCache output;
    MagicSharpEffect effect;
    effect.setPropertyValue(UniString("amount"), QVariant(2.0));
    effect.setPropertyValue(UniString("threshold"), QVariant(0.0));
    effect.setPropertyValue(UniString("edgeProtection"), QVariant(0.0));
    effect.setPropertyValue(UniString("shadowProtection"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    bool changedRgb = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            changedRgb = changedRgb || after.r() != before.r();
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
    EXPECT_TRUE(changedRgb);
}

TEST(MagicSharpEffectContractTest, ZeroAmountOrMixPreservesEveryRgbaChannel)
{
    const auto source = makeFineDetailSurface();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);

    MagicSharpEffect effect;
    effect.setPropertyValue(UniString("amount"), QVariant(0.0));
    ImageF32x4RGBAWithCache zeroAmountOutput;
    effect.applyCPUOnly(source, zeroAmountOutput);

    effect.setPropertyValue(UniString("amount"), QVariant(2.0));
    effect.setPropertyValue(UniString("mix"), QVariant(0.0));
    ImageF32x4RGBAWithCache zeroMixOutput;
    effect.applyCPUOnly(source, zeroMixOutput);

    ASSERT_EQ(zeroAmountOutput.image().width(), kWidth);
    ASSERT_EQ(zeroAmountOutput.image().height(), kHeight);
    ASSERT_EQ(zeroMixOutput.image().width(), kWidth);
    ASSERT_EQ(zeroMixOutput.image().height(), kHeight);
    const float* zeroAmountPixels = zeroAmountOutput.image().rgba32fData();
    const float* zeroMixPixels = zeroMixOutput.image().rgba32fData();
    ASSERT_NE(zeroAmountPixels, nullptr);
    ASSERT_NE(zeroMixPixels, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(zeroAmountPixels[channel], sourcePixels[channel])
            << "zero-amount channel=" << channel;
        EXPECT_FLOAT_EQ(zeroMixPixels[channel], sourcePixels[channel])
            << "zero-mix channel=" << channel;
    }
}
