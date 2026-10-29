#include <gtest/gtest.h>

#include <array>
#include <cstddef>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.TimeDisplacement;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 16>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(TimeDisplacementEffectContractTest, MissingSamplerPreservesSourcePixels)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, 0.1f, 1.0f,
    };
    Artifact::TimeDisplacementEffect effect;
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel]) << "channel=" << channel;
    }
}

TEST(TimeDisplacementEffectContractTest, SettersAndPropertiesExposeTemporalControls)
{
    Artifact::TimeDisplacementEffect effect;
    effect.setMaxOffsetFrames(-3.0f);
    EXPECT_FLOAT_EQ(effect.maxOffsetFrames(), 0.0f);
    effect.setSourceChannel(Artifact::TimeDisplaceChannel::Alpha);
    effect.setFrameBlend(false);
    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 3u);
    EXPECT_FLOAT_EQ(properties[0].getValue().toFloat(), 0.0f);
    EXPECT_EQ(properties[1].getValue().toInt(),
              static_cast<int>(Artifact::TimeDisplaceChannel::Alpha));
    EXPECT_FALSE(properties[2].getValue().toBool());
}
