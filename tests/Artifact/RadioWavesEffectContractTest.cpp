#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Generate.RadioWaves;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 9;

ImageF32x4RGBAWithCache makeTransparentSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(RadioWavesEffectContractTest, RendersBirthRingAtOriginAndLeavesDistantPixelsAlone)
{
    const auto source = makeTransparentSurface();
    RadioWavesEffect effect;
    effect.setOriginX(0.5f);
    effect.setOriginY(0.5f);
    effect.setFrequency(2.0f);
    effect.setExpansion(80.0f);
    effect.setLifespan(2.0f);
    effect.setStrokeWidth(3.0f);
    effect.setOpacity(100.0f);
    effect.setWaveColor(QColor(255, 0, 0, 255));
    effect.setCurrentTime(0.0f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    const auto ring = output.image().getPixel(4, 4);
    EXPECT_NEAR(ring.r(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(ring.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(ring.b(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(ring.a(), 1.0f, 1.0e-6f);

    const auto distant = output.image().getPixel(0, 0);
    EXPECT_NEAR(distant.a(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(4, 4).a(), 0.0f, 1.0e-6f);
}

TEST(RadioWavesEffectContractTest, LeavesImageUnchangedWhenActiveRingsAreOffCanvas)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        pixels[pixel] = 0.2f;
        pixels[pixel + 1] = 0.4f;
        pixels[pixel + 2] = 0.6f;
        pixels[pixel + 3] = 0.5f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    RadioWavesEffect effect;
    effect.setExpansion(80.0f);
    effect.setCurrentTime(10.25f);

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}

TEST(RadioWavesEffectContractTest, ExpiredWaveLeavesFrameUnchangedBeforeNextBirth)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        pixels[pixel] = 0.2f;
        pixels[pixel + 1] = 0.4f;
        pixels[pixel + 2] = 0.6f;
        pixels[pixel + 3] = 0.5f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    RadioWavesEffect effect;
    effect.setFrequency(2.0f);
    effect.setLifespan(0.1f);
    effect.setExpansion(10.0f);
    effect.setCurrentTime(0.25f);

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_NEAR(outputPixels[channel], pixels[channel], 1.0e-6f)
            << "channel=" << channel;
    }
}
\n
\n
