#include <gtest/gtest.h>

#include <QColor>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.DropShadow;
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

TEST(DropShadowEffectContractTest, ShadowOnlyOffsetsAlphaAndUsesConfiguredColor)
{
    constexpr int width = 9;
    constexpr int height = 9;
    std::array<float, width * height * 4> pixels{};
    const int sourceOffset = (4 * width + 4) * 4;
    pixels[sourceOffset] = 1.0f;
    pixels[sourceOffset + 1] = 1.0f;
    pixels[sourceOffset + 2] = 1.0f;
    pixels[sourceOffset + 3] = 1.0f;
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    DropShadowEffect effect;
    effect.setShadowColor(QColor(255, 0, 0, 255));
    effect.setDistance(2.0f);
    effect.setAngle(90.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);
    effect.setShadowOnly(true);

    effect.applyCPUOnly(source, output);

    const auto shadow = output.image().getPixel(4, 6);
    EXPECT_NEAR(shadow.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(shadow.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(shadow.b(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(shadow.a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 4).a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 4).a(), 1.0f, 1.0e-6f);
}


TEST(DropShadowEffectContractTest, ZeroOpacityPreservesSourceWhenShadowIsNotIsolated)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels = {
        0.1f, 0.2f, 0.3f, 0.0f,
        0.2f, 0.4f, 0.6f, 0.25f,
        0.3f, 0.5f, 0.7f, 0.5f,
        0.4f, 0.6f, 0.8f, 0.75f,
        0.5f, 0.7f, 0.9f, 1.0f,
        0.6f, 0.8f, 1.0f, 0.4f,
    };
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    DropShadowEffect effect;
    effect.setOpacity(0.0f);
    effect.setDistance(3.0f);
    effect.setSoftness(2.0f);
    effect.setShadowOnly(false);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(actual[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(DropShadowEffectContractTest, ShadowOffsetBeyondImageBoundsIsClipped)
{
    constexpr int width = 3;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    const int sourceOffset = ((height - 1) * width + 1) * 4;
    pixels[sourceOffset] = 1.0f;
    pixels[sourceOffset + 1] = 1.0f;
    pixels[sourceOffset + 2] = 1.0f;
    pixels[sourceOffset + 3] = 1.0f;
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    DropShadowEffect effect;
    effect.setDistance(2.0f);
    effect.setAngle(90.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);
    effect.setShadowOnly(true);

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            EXPECT_NEAR(output.image().getPixel(x, y).a(), 0.0f, 1.0e-6f)
                << "x=" << x << ", y=" << y;
        }
    }
    EXPECT_NEAR(source.image().getPixel(1, height - 1).a(), 1.0f, 1.0e-6f);
}
