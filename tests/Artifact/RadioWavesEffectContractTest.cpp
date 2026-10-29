#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Generate.RadioWaves;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 64>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 4, 4);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

Pixels makePixels()
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
        pixels[pixel * 4] = 0.1f + static_cast<float>(pixel) * 0.01f;
        pixels[pixel * 4 + 1] = 0.2f;
        pixels[pixel * 4 + 2] = 0.3f;
        pixels[pixel * 4 + 3] = 0.5f;
    }
    return pixels;
}

} // namespace

TEST(RadioWavesEffectContractTest, ZeroOpacityPreservesSourcePixels)
{
    const Pixels pixels = makePixels();
    Artifact::RadioWavesEffect effect;
    effect.setCurrentTime(1.0f);
    effect.setOpacity(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel]) << "channel=" << channel;
    }
}

TEST(RadioWavesEffectContractTest, SettersClampAndRecoverInvalidValues)
{
    Artifact::RadioWavesEffect effect;
    effect.setOriginX(2.0f);
    EXPECT_FLOAT_EQ(effect.originX(), 1.0f);
    effect.setOriginY(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.originY(), 0.5f);
    effect.setFrequency(-1.0f);
    EXPECT_FLOAT_EQ(effect.frequency(), 0.01f);
    effect.setExpansion(-1.0f);
    EXPECT_FLOAT_EQ(effect.expansion(), 0.0f);
    effect.setLifespan(0.0f);
    EXPECT_FLOAT_EQ(effect.lifespan(), 0.01f);
    effect.setStrokeWidth(0.0f);
    EXPECT_FLOAT_EQ(effect.strokeWidth(), 0.1f);
    effect.setOpacity(120.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 100.0f);
    effect.setCurrentTime(-1.0f);
    EXPECT_FLOAT_EQ(effect.currentTime(), 0.0f);
}
