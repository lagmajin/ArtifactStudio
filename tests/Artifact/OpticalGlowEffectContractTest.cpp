#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Glow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

using Pixels = std::array<float, 64>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 4, 4);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(OpticalGlowEffectContractTest, OutputPreservesDimensionsAndSourceAlpha)
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
        const float value = static_cast<float>((pixel * 7) % 16) / 15.0f;
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = 1.0f - value;
        pixels[pixel * 4 + 2] = value * 0.6f;
        pixels[pixel * 4 + 3] = 0.1f + value * 0.8f;
    }
    Artifact::OpticalGlowEffect effect;
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    EXPECT_EQ(output.image().width(), 4);
    EXPECT_EQ(output.image().height(), 4);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 3], pixels[pixel * 4 + 3]);
    }
}

TEST(OpticalGlowEffectContractTest, PropertyUpdatesClampToPublishedRanges)
{
    Artifact::OpticalGlowEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("threshold"), QVariant(10.0));
    effect.setPropertyValue(ArtifactCore::UniString("radius"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("intensity"), QVariant(20.0));
    effect.setPropertyValue(ArtifactCore::UniString("warmth"), QVariant(-4.0));
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 9u);
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 4.0f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 0.1f);
    EXPECT_FLOAT_EQ(properties[3].getValue().toFloat(), 8.0f);
    EXPECT_FLOAT_EQ(properties[8].getValue().toFloat(), -1.0f);
}
