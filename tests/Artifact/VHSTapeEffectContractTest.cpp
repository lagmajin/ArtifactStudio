#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VHSTape;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeUniformSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 0.5f;
        pixels[offset + 1] = 0.5f;
        pixels[offset + 2] = 0.5f;
        pixels[offset + 3] = 1.0f;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

void setNeutralTapeProperties(VHSTapeEffect& effect)
{
    effect.setPropertyValue(UniString(QStringLiteral("Tracking Error")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Noise Amount")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Color Bleed")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Wobble")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Saturation")), 1.0);
    effect.setPropertyValue(UniString(QStringLiteral("Sharpness")), 1.0);
    effect.setPropertyValue(UniString(QStringLiteral("Bottom Jitter")), 0.0);
    effect.setPropertyValue(UniString(QStringLiteral("Show OSD")), false);
}

} // namespace

TEST(VHSTapeEffectContractTest, ScanlineGapDarkensEveryConfiguredRow)
{
    const auto source = makeUniformSurface();
    ImageF32x4RGBAWithCache output;
    VHSTapeEffect effect;
    setNeutralTapeProperties(effect);
    effect.setPropertyValue(UniString(QStringLiteral("Scanline Gap")), 2.0);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        const float expected = (y % 2 == 0) ? 0.4f : 0.5f;
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.g(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
            EXPECT_NEAR(pixel.b(), expected, 1.0e-6f) << "x=" << x << " y=" << y;
        }
    }
}
