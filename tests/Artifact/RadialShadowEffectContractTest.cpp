#include <gtest/gtest.h>

#include <QColor>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.RadialShadow;
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

TEST(RadialShadowEffectContractTest, ShadowOpacityFallsOffWithDistanceFromConfiguredCenter)
{
    constexpr int width = 5;
    constexpr int height = 5;
    constexpr std::array<float, width * height * 4> pixels{};
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RadialShadowEffect effect;
    effect.setColor(QColor(255, 0, 0, 255));
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setDistance(4.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(0.8f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(2, 2);
    const auto corner = output.image().getPixel(0, 0);
    EXPECT_GT(center.a(), corner.a());
    EXPECT_GT(center.r(), corner.r());
    EXPECT_NEAR(center.r(), center.a(), 1.0e-6f);
    EXPECT_NEAR(center.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(center.b(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).a(), 0.0f, 1.0e-6f);
}


TEST(RadialShadowEffectContractTest, ZeroOpacityPreservesSourceWhenShadowIsComposited)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.2f, 0.4f, 0.6f, 0.25f,
        0.3f, 0.5f, 0.7f, 0.5f,
        0.4f, 0.6f, 0.8f, 0.75f,
        0.8f, 0.9f, 1.0f, 1.0f,
        0.6f, 0.4f, 0.2f, 0.4f,
    };
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    RadialShadowEffect effect;
    effect.setColor(QColor(255, 0, 128, 255));
    effect.setDistance(20.0f);
    effect.setSoftness(5.0f);
    effect.setOpacity(0.0f);
    effect.setShadowOnly(false);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(RadialShadowEffectContractTest, FullColorInfluenceUsesSourceColorInShadowOnlyOutput)
{
    constexpr std::array<float, 4> pixels = {0.2f, 0.4f, 0.8f, 1.0f};
    const auto source = makeSurface(pixels.data(), 1, 1);
    ImageF32x4RGBAWithCache output;
    RadialShadowEffect effect;
    effect.setColor(QColor(0, 0, 0, 255));
    effect.setCenterX(0.0f);
    effect.setCenterY(0.0f);
    effect.setDistance(0.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(0.5f);
    effect.setColorInfluence(1.0f);
    effect.setShadowOnly(true);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), 0.1f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(0, 0).r(), 0.2f, 1.0e-6f);
}
