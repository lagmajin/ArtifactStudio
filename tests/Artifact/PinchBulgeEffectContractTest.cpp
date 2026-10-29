#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.PinchBulge;
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
            pixels[offset + 2] = 0.2f;
            pixels[offset + 3] = 0.7f;
        }
    }
    return pixels;
}

void configure(PinchBulgeEffect& effect, float amount)
{
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRadius(0.5f);
    effect.setAmount(amount);
}

} // namespace

TEST(PinchBulgeEffectContractTest, AmountSignSelectsInwardPinchOrOutwardBulge)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache pinchOutput;
    ImageF32x4RGBAWithCache bulgeOutput;

    PinchBulgeEffect pinch;
    configure(pinch, 50.0f);
    pinch.applyCPUOnly(source, pinchOutput);

    PinchBulgeEffect bulge;
    configure(bulge, -50.0f);
    bulge.applyCPUOnly(source, bulgeOutput);

    const float original = source.image().getPixel(6, 4).r();
    const float pinched = pinchOutput.image().getPixel(6, 4).r();
    const float bulged = bulgeOutput.image().getPixel(6, 4).r();
    EXPECT_LT(pinched, original);
    EXPECT_GT(bulged, original);
    EXPECT_NEAR(pinchOutput.image().getPixel(4, 4).r(),
                source.image().getPixel(4, 4).r(), 1.0e-6f);
    EXPECT_NEAR(bulgeOutput.image().getPixel(4, 4).r(),
                source.image().getPixel(4, 4).r(), 1.0e-6f);
    EXPECT_NEAR(pinchOutput.image().getPixel(6, 4).a(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(bulgeOutput.image().getPixel(6, 4).a(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(6, 4).r(), 6.0f / 7.0f, 1.0e-6f);
}

TEST(PinchBulgeEffectContractTest, BothAmountSignsLeavePixelsOutsideRadiusUntouched)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache pinchOutput;
    ImageF32x4RGBAWithCache bulgeOutput;
    PinchBulgeEffect pinch;
    PinchBulgeEffect bulge;
    configure(pinch, 80.0f);
    configure(bulge, -80.0f);

    pinch.applyCPUOnly(source, pinchOutput);
    bulge.applyCPUOnly(source, bulgeOutput);

    const auto original = source.image().getPixel(0, 0);
    const auto pinched = pinchOutput.image().getPixel(0, 0);
    const auto bulged = bulgeOutput.image().getPixel(0, 0);
    EXPECT_NEAR(pinched.r(), original.r(), 1.0e-6f);
    EXPECT_NEAR(pinched.g(), original.g(), 1.0e-6f);
    EXPECT_NEAR(pinched.b(), original.b(), 1.0e-6f);
    EXPECT_NEAR(pinched.a(), original.a(), 1.0e-6f);
    EXPECT_NEAR(bulged.r(), original.r(), 1.0e-6f);
    EXPECT_NEAR(bulged.g(), original.g(), 1.0e-6f);
    EXPECT_NEAR(bulged.b(), original.b(), 1.0e-6f);
    EXPECT_NEAR(bulged.a(), original.a(), 1.0e-6f);
}

TEST(PinchBulgeEffectContractTest, ZeroRadiusPreservesTheEntireInput)
{
    const auto pixels = makeGradient();
    const auto source = makeSurface(pixels.data(), kWidth, kHeight);
    ImageF32x4RGBAWithCache output;
    PinchBulgeEffect effect;
    effect.setCenterX(0.5f);
    effect.setCenterY(0.5f);
    effect.setRadius(0.0f);
    effect.setAmount(100.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], original[channel])
            << "channel=" << channel;
    }
}
\n
