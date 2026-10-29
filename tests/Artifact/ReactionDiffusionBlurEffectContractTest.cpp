#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ReactionDiffusionBlur;
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

TEST(ReactionDiffusionBlurEffectContractTest, OutputKeepsDimensionsAndSourceAlpha)
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
        const float value = static_cast<float>((pixel * 5) % 16) / 15.0f;
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = 1.0f - value;
        pixels[pixel * 4 + 2] = static_cast<float>(pixel) / 15.0f;
        pixels[pixel * 4 + 3] = 0.1f + static_cast<float>(pixel) * 0.05f;
    }
    Artifact::ReactionDiffusionBlurEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
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

TEST(ReactionDiffusionBlurEffectContractTest, PropertiesClampSimulationInputs)
{
    Artifact::ReactionDiffusionBlurEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Blur Radius"), QVariant(100.0));
    effect.setPropertyValue(ArtifactCore::UniString("Feed"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("Kill"), QVariant(1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Pattern Strength"), QVariant(-1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Iterations"), QVariant(0));
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 6u);
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 80.0f);
    EXPECT_FLOAT_EQ(properties[1].getValue().toFloat(), 0.01f);
    EXPECT_FLOAT_EQ(properties[2].getValue().toFloat(), 0.1f);
    EXPECT_FLOAT_EQ(properties[3].getValue().toFloat(), 0.0f);
    EXPECT_EQ(properties[4].getValue().toInt(), 1);
}
