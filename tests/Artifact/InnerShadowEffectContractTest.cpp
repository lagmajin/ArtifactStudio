#include <gtest/gtest.h>

#include <QColor>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.InnerShadow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(InnerShadowEffectContractTest, ShadowIsMaskedInsideSilhouetteAndPreservesSourceAlpha)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 1.0f;
            pixels[offset + 2] = 1.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    InnerShadowEffect effect;
    effect.setShadowColor(QColor(0, 0, 0, 255));
    effect.setDistance(2.0f);
    effect.setAngle(90.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);

    effect.applyCPUOnly(source, output);

    const auto shadedInterior = output.image().getPixel(4, 4);
    const auto unshadedInterior = output.image().getPixel(4, 2);
    EXPECT_LT(shadedInterior.r(), unshadedInterior.r());
    EXPECT_NEAR(shadedInterior.r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(unshadedInterior.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(shadedInterior.a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 0).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 4).r(), 1.0f, 1.0e-6f);
}


TEST(InnerShadowEffectContractTest, ZeroOpacityLeavesSourceUnshaded)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels = {
        0.0f, 0.0f, 0.0f, 0.0f,
        0.2f, 0.4f, 0.6f, 0.25f,
        0.3f, 0.5f, 0.7f, 0.5f,
        0.4f, 0.6f, 0.8f, 0.75f,
        0.8f, 0.9f, 1.0f, 1.0f,
        0.6f, 0.4f, 0.2f, 0.4f,
    };
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    InnerShadowEffect effect;
    effect.setShadowColor(QColor(255, 0, 128, 255));
    effect.setDistance(4.0f);
    effect.setSoftness(2.0f);
    effect.setOpacity(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(InnerShadowEffectContractTest, ZeroDegreeLightCastsShadowTowardLeft)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    for (int y = 2; y <= 6; ++y) {
        for (int x = 2; x <= 6; ++x) {
            const int offset = (y * width + x) * 4;
            pixels[offset] = 1.0f;
            pixels[offset + 1] = 1.0f;
            pixels[offset + 2] = 1.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    InnerShadowEffect effect;
    effect.setShadowColor(QColor(0, 0, 0, 255));
    effect.setDistance(2.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(4, 4).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 4).r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(6, 4).r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 4).a(), 1.0f, 1.0e-6f);
}
