#include <gtest/gtest.h>

#include <array>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.PolarCoordinates;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

std::array<float, kWidth * kHeight * 4> makeGradient()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int offset = (y * kWidth + x) * 4;
            pixels[offset] = static_cast<float>(x) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y) / 7.0f;
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = 0.8f;
        }
    }
    return pixels;
}

} // namespace

TEST(PolarCoordinatesEffectContractTest, ZeroAmountPreservesImageExactly)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    PolarCoordinatesEffect effect;
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t component = 0; component < pixels.size(); ++component) {
        EXPECT_FLOAT_EQ(actual[component], pixels[component])
            << "component=" << component;
    }
}

TEST(PolarCoordinatesEffectContractTest, FullAmountKeepsCenterAndRemapsRadialSample)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    PolarCoordinatesEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRadius(0.5f);
    effect.setAmount(1.0f);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(4, 4).r(),
                source.image().getPixel(4, 4).r(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 6).g(),
                source.image().getPixel(4, 7).g(), 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(4, 6).a(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 6).g(), 6.0f / 7.0f, 1.0e-6f);
}

TEST(PolarCoordinatesEffectContractTest, ZeroRadiusPreservesImageEvenAtFullAmount)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    PolarCoordinatesEffect effect;
    effect.setCenterX(0.25f);
    effect.setCenterY(0.75f);
    effect.setRadius(0.0f);
    effect.setAmount(1.0f);

    effect.applyCPUOnly(source, output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t component = 0; component < pixels.size(); ++component) {
        EXPECT_FLOAT_EQ(actual[component], pixels[component])
            << "component=" << component;
    }
}

TEST(PolarCoordinatesEffectContractTest, CenterRadiusAndAmountClampAndResetNonFiniteValues)
{
    PolarCoordinatesEffect effect;
    effect.setCenterX(-1.0f);
    effect.setCenterY(2.0f);
    effect.setRadius(-0.5f);
    effect.setAmount(2.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);

    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    effect.setCenterY(std::numeric_limits<float>::infinity());
    effect.setRadius(std::numeric_limits<float>::quiet_NaN());
    effect.setAmount(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);
    EXPECT_FLOAT_EQ(effect.radius(), 0.5f);
    EXPECT_FLOAT_EQ(effect.amount(), 1.0f);
}
