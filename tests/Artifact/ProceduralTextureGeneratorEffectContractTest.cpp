#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Generator.ProceduralTexture;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

TEST(ProceduralTextureGeneratorEffectContractTest, SeedAndDimensionsAreStable)
{
    Artifact::ProceduralTextureGeneratorEffect effect;
    effect.setWidth(8);
    effect.setHeight(6);
    effect.setSeed(42);
    ArtifactCore::ImageF32x4RGBAWithCache input;
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache second;

    effect.applyCPUOnly(input, first);
    effect.applyCPUOnly(input, second);

    ASSERT_EQ(first.image().width(), 8);
    ASSERT_EQ(first.image().height(), 6);
    ASSERT_EQ(second.image().width(), 8);
    ASSERT_EQ(second.image().height(), 6);
    const float* firstPixels = first.image().rgba32fData();
    const float* secondPixels = second.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(secondPixels, nullptr);
    for (std::size_t channel = 0; channel < 8u * 6u * 4u; ++channel) {
        EXPECT_FLOAT_EQ(firstPixels[channel], secondPixels[channel]);
    }
}

TEST(ProceduralTextureGeneratorEffectContractTest, DimensionsAndPresetClampToSupportedRanges)
{
    Artifact::ProceduralTextureGeneratorEffect effect;
    effect.setWidth(0);
    effect.setHeight(9000);
    EXPECT_EQ(effect.width(), 1);
    EXPECT_EQ(effect.height(), 8192);
    effect.setPreset(static_cast<ArtifactCore::ProceduralTexturePreset>(99));
    EXPECT_EQ(static_cast<std::uint32_t>(effect.preset()), 5u);
    effect.setPropertyValue(ArtifactCore::UniString("Width"), QVariant(-10));
    EXPECT_EQ(effect.width(), 1);
}
