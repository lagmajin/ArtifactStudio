#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.SurfaceFX;
import Graphics.Effect.SurfaceFX;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using namespace ArtifactCore;

constexpr int kWidth = 24;
constexpr int kHeight = 24;
constexpr float kBackground = 0.5f;
constexpr float kAlpha = 0.35f;

ImageF32x4RGBAWithCache makeSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = kBackground;
        pixels[i + 1] = kBackground;
        pixels[i + 2] = kBackground;
        pixels[i + 3] = kAlpha;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

SurfaceFXData makeTimedDirt(float inTime)
{
    SurfaceFXData data;
    SurfaceFXElement element;
    element.type = SurfaceFXElementType::Dirt;
    element.x = 0.5f;
    element.y = 0.5f;
    element.width = 0.25f;
    element.height = 0.25f;
    element.opacity = 0.8f;
    element.intensity = 1.0f;
    element.inTime = inTime;
    data.elements.push_back(element);
    return data;
}

} // namespace

TEST(SurfaceFXEffectContractTest, ElementBeforeItsInTimeLeavesSurfaceUnchanged)
{
    const auto source = makeSurface();
    ImageF32x4RGBAWithCache output;
    Artifact::SurfaceFXEffect effect;
    effect.setData(makeTimedDirt(1.0f));
    Artifact::EffectContext context;
    context.timeSeconds = 0.5;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* pixels = output.image().rgba32fData();
    ASSERT_NE(pixels, nullptr);
    for (std::size_t i = 0; i < kWidth * kHeight; ++i) {
        EXPECT_FLOAT_EQ(pixels[i * 4], kBackground);
        EXPECT_FLOAT_EQ(pixels[i * 4 + 1], kBackground);
        EXPECT_FLOAT_EQ(pixels[i * 4 + 2], kBackground);
        EXPECT_FLOAT_EQ(pixels[i * 4 + 3], kAlpha);
    }
}

TEST(SurfaceFXEffectContractTest, ActiveDirtElementCompositesColorAndPreservesAlpha)
{
    const auto source = makeSurface();
    ImageF32x4RGBAWithCache output;
    Artifact::SurfaceFXEffect effect;
    effect.setData(makeTimedDirt(0.0f));
    Artifact::EffectContext context;
    context.timeSeconds = 0.5;
    effect.setContext(context);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* pixels = output.image().rgba32fData();
    ASSERT_NE(pixels, nullptr);
    bool colorChanged = false;
    for (std::size_t i = 0; i < kWidth * kHeight; ++i) {
        colorChanged = colorChanged ||
            pixels[i * 4] != kBackground ||
            pixels[i * 4 + 1] != kBackground ||
            pixels[i * 4 + 2] != kBackground;
        EXPECT_FLOAT_EQ(pixels[i * 4 + 3], kAlpha) << "pixel=" << i;
    }
    EXPECT_TRUE(colorChanged);
}
