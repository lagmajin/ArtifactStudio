#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.LinearWipe;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 16>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 4, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(LinearWipeEffectContractTest, HorizontalWipePreservesRgbAndReducesAlphaAcrossFrame)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 1.0f,
        0.4f, 0.5f, 0.6f, 1.0f,
        0.7f, 0.8f, 0.9f, 1.0f,
        0.2f, 0.3f, 0.4f, 1.0f,
    };
    Artifact::LinearWipeEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setAngle(0.0f);
    effect.setSoftness(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t pixel = 0; pixel < 4; ++pixel) {
        EXPECT_FLOAT_EQ(actual[pixel * 4], pixels[pixel * 4]);
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 1], pixels[pixel * 4 + 1]);
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 2], pixels[pixel * 4 + 2]);
    }
    EXPECT_GE(actual[3], actual[7]);
    EXPECT_GE(actual[7], actual[11]);
    EXPECT_GE(actual[11], actual[15]);
    EXPECT_FLOAT_EQ(actual[3], 1.0f);
    EXPECT_FLOAT_EQ(actual[15], 0.0f);
}

TEST(LinearWipeEffectContractTest, SettersNormalizeOrClampValues)
{
    Artifact::LinearWipeEffect effect;
    effect.setAngle(-90.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 270.0f);
    effect.setAngle(810.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 90.0f);
    effect.setSoftness(2.0f);
    EXPECT_FLOAT_EQ(effect.softness(), 1.0f);
    effect.setFeather(-1.0f);
    EXPECT_FLOAT_EQ(effect.feather(), 0.0f);
}
