#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Strobe;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makePixel(float r, float g, float b, float a)
{
    const std::array<float, 4> pixels{r, g, b, a};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPixel(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                 float r, float g, float b, float a)
{
    const auto pixel = image.image().getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), r, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), g, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), b, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), a, 1.0e-6f);
}

} // namespace

TEST(StrobeEffectContractTest, CapturesFirstFrameAndMixesLaterFramesWithReference)
{
    const auto reference = makePixel(0.2f, 0.4f, 0.6f, 0.2f);
    const auto current = makePixel(0.8f, 0.2f, 0.1f, 0.8f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::StrobeEffect effect;
    effect.setFrequency(4.0f);
    effect.setMixAmount(0.5f);

    // The default context time is in the strobe's on half-cycle.
    effect.applyCPUOnly(reference, output);
    expectPixel(output, 0.2f, 0.4f, 0.6f, 0.2f);

    effect.applyCPUOnly(current, output);
    expectPixel(output, 0.5f, 0.3f, 0.35f, 0.5f);

    effect.applyCPUOnly(current, output);
    expectPixel(output, 0.5f, 0.3f, 0.35f, 0.5f);
}

TEST(StrobeEffectContractTest, SettersClampFrequencyAndMixAmount)
{
    Artifact::StrobeEffect effect;
    effect.setFrequency(0.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.1f);
    effect.setMixAmount(-1.0f);
    EXPECT_FLOAT_EQ(effect.mixAmount(), 0.0f);
    effect.setMixAmount(2.0f);
    EXPECT_FLOAT_EQ(effect.mixAmount(), 1.0f);
}
