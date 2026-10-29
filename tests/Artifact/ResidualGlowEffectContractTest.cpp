#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow.ResidualGlow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 5;
constexpr int kHeight = 5;

ImageF32x4RGBAWithCache makeUniformSurface(float value, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(ResidualGlowEffectContractTest, InitialBrightFrameAddsGlowAndPreservesAlpha)
{
    const auto source = makeUniformSurface(0.8f, 0.3f);
    ImageF32x4RGBAWithCache output;
    ResidualGlowEffect effect;
    effect.setThreshold(0.2f);
    effect.setRadius(0.5f);
    effect.setIntensity(1.0f);
    effect.setDecay(0.5f);
    effect.setHistoryMix(1.0f);

    effect.applyCPUOnly(source, output);

    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_GT(pixel.r(), 0.8f);
    EXPECT_NEAR(pixel.r(), pixel.g(), 1.0e-6f);
    EXPECT_NEAR(pixel.g(), pixel.b(), 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).r(), 0.8f, 1.0e-6f);
}

TEST(ResidualGlowEffectContractTest, DecayedHistoryAddsResidualGlowToFollowingDarkFrame)
{
    const auto brightSource = makeUniformSurface(0.8f, 0.3f);
    const auto darkSource = makeUniformSurface(0.0f, 0.4f);
    ImageF32x4RGBAWithCache firstOutput;
    ImageF32x4RGBAWithCache secondOutput;
    ResidualGlowEffect effect;
    effect.setThreshold(0.2f);
    effect.setRadius(0.5f);
    effect.setIntensity(1.0f);
    effect.setDecay(0.5f);
    effect.setHistoryMix(1.0f);

    effect.applyCPUOnly(brightSource, firstOutput);
    effect.applyCPUOnly(darkSource, secondOutput);

    const auto residual = secondOutput.image().getPixel(2, 2);
    EXPECT_GT(residual.r(), 0.0f);
    EXPECT_NEAR(residual.r(), residual.g(), 1.0e-6f);
    EXPECT_NEAR(residual.g(), residual.b(), 1.0e-6f);
    EXPECT_NEAR(residual.a(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(darkSource.image().getPixel(2, 2).r(), 0.0f, 1.0e-6f);
}

TEST(ResidualGlowEffectContractTest, ResetsHistoryWhenImageDimensionsChange)
{
    const auto brightSource = makeUniformSurface(0.8f, 0.3f);
    constexpr std::array<float, 3 * 3 * 4> darkPixels = {
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.4f,
    };
    ImageF32x4_RGBA darkImage;
    darkImage.setFromRGBA32F(darkPixels.data(), 3, 3);
    const ImageF32x4RGBAWithCache darkSource(darkImage);
    ImageF32x4RGBAWithCache output;
    ResidualGlowEffect effect;
    effect.setThreshold(0.2f);
    effect.setRadius(0.5f);
    effect.setIntensity(1.0f);
    effect.setDecay(0.5f);
    effect.setHistoryMix(1.0f);

    ImageF32x4RGBAWithCache firstOutput;
    effect.applyCPUOnly(brightSource, firstOutput);
    effect.applyCPUOnly(darkSource, output);

    ASSERT_EQ(output.image().width(), 3);
    ASSERT_EQ(output.image().height(), 3);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), 0.0f);
            EXPECT_FLOAT_EQ(actual.g(), 0.0f);
            EXPECT_FLOAT_EQ(actual.b(), 0.0f);
            EXPECT_FLOAT_EQ(actual.a(), 0.4f);
        }
    }
}
\n
