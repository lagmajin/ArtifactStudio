#include <gtest/gtest.h>

#include <array>

import Artifact.Effect.Abstract;
import Artifact.Effect.Distort.DisplacementMap;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 12>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

Pixels makeNeutralMap()
{
    return Pixels{
        0.5f, 0.5f, 0.5f, 0.3f,
        0.5f, 0.5f, 0.5f, 0.6f,
        0.5f, 0.5f, 0.5f, 0.9f,
    };
}

} // namespace

TEST(DisplacementMapEffectContractTest, HalfChannelValuesAreNeutral)
{
    const Pixels pixels = makeNeutralMap();
    Artifact::DisplacementMapEffect effect;
    effect.setComputeMode(Artifact::ComputeMode::CPU);
    effect.setMaxHorizontal(100.0f);
    effect.setMaxVertical(-200.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    for (int x = 0; x < 3; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const auto expected = makeImage(pixels).image().getPixel(x, 0);
        EXPECT_FLOAT_EQ(actual.r(), expected.r());
        EXPECT_FLOAT_EQ(actual.g(), expected.g());
        EXPECT_FLOAT_EQ(actual.b(), expected.b());
        EXPECT_FLOAT_EQ(actual.a(), expected.a());
    }
}

TEST(DisplacementMapEffectContractTest, WrapAroundSelectsOppositeEdgeAtBoundary)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 1.0f,
        0.4f, 0.5f, 0.6f, 1.0f,
        0.7f, 0.8f, 0.9f, 1.0f,
    };
    Artifact::DisplacementMapEffect clamped;
    clamped.setComputeMode(Artifact::ComputeMode::CPU);
    clamped.setHorizontalChannel(Artifact::DisplaceChannel::Alpha);
    clamped.setMaxHorizontal(2.0f);
    Artifact::DisplacementMapEffect wrapped;
    wrapped.setComputeMode(Artifact::ComputeMode::CPU);
    wrapped.setHorizontalChannel(Artifact::DisplaceChannel::Alpha);
    wrapped.setMaxHorizontal(2.0f);
    wrapped.setWrapAround(true);
    ArtifactCore::ImageF32x4RGBAWithCache clampOutput;
    ArtifactCore::ImageF32x4RGBAWithCache wrapOutput;

    clamped.applyCPUOnly(makeImage(pixels), clampOutput);
    wrapped.applyCPUOnly(makeImage(pixels), wrapOutput);

    EXPECT_FLOAT_EQ(clampOutput.image().getPixel(2, 0).r(), 0.7f);
    EXPECT_FLOAT_EQ(wrapOutput.image().getPixel(2, 0).r(), 0.1f);
}
