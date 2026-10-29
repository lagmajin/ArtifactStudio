#include <gtest/gtest.h>

#include <array>

#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.RadialShadow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeSurface()
{
    constexpr std::array<float, 3 * 3 * 4> pixels{
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
        0.2f, 0.2f, 0.2f, 0.1f,
    };
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 3);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(RadialShadowEffectContractTest,
     ShadowOnlyUsesRadialFalloffAndReplacesSourcePixels)
{
    const auto source = makeSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::RadialShadowEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setColor(QColor(255, 0, 0));
    effect.setOpacity(0.6f);
    effect.setDistance(1.0f);
    effect.setSoftness(1.0f);
    effect.setCenterX(1.0f / 3.0f);
    effect.setCenterY(1.0f / 3.0f);
    effect.setShadowOnly(true);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(1, 1);
    EXPECT_FLOAT_EQ(center.r(), 0.6f);
    EXPECT_FLOAT_EQ(center.g(), 0.0f);
    EXPECT_FLOAT_EQ(center.b(), 0.0f);
    EXPECT_FLOAT_EQ(center.a(), 0.6f);

    const auto onePixelAway = output.image().getPixel(2, 1);
    EXPECT_FLOAT_EQ(onePixelAway.r(), 0.3f);
    EXPECT_FLOAT_EQ(onePixelAway.g(), 0.0f);
    EXPECT_FLOAT_EQ(onePixelAway.b(), 0.0f);
    EXPECT_FLOAT_EQ(onePixelAway.a(), 0.3f);
}
