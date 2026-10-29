#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.MotionTrail;
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

class MissingHistorySampler final : public Artifact::IEffectFrameSampler {
public:
    bool sampleCurrentLayerFrame(std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override { return false; }
    bool sampleCurrentLayerFrameRelative(std::int64_t offset,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        requestedOffsets.push_back(offset);
        return false;
    }
    bool sampleNamedInput(const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override { return false; }

    std::vector<std::int64_t> requestedOffsets;
};

void expectPixelsEqual(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                       const Pixels& expected)
{
    const float* actual = image.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
    }
}

} // namespace

TEST(MotionTrailEffectContractTest, MissingHistoryPreservesInputPixels)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, 0.1f, 1.0f,
    };
    MissingHistorySampler sampler;
    Artifact::EffectContext context;
    context.sampler = &sampler;
    Artifact::MotionTrailEffect effect;
    effect.setContext(context);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1}));
    expectPixelsEqual(output, pixels);
}

TEST(MotionTrailEffectContractTest, NoSamplerPreservesInputPixels)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, 0.1f, 1.0f,
    };
    Artifact::MotionTrailEffect effect;
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    expectPixelsEqual(output, pixels);
}

TEST(MotionTrailEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::MotionTrailEffect effect;
    effect.setTrailLength(0);
    EXPECT_EQ(effect.trailLength(), 1);
    effect.setTrailLength(100);
    EXPECT_EQ(effect.trailLength(), 16);
    effect.setDecay(-1.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 0.0f);
    effect.setDecay(2.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 1.0f);
    effect.setVelocityScale(-1.0f);
    EXPECT_FLOAT_EQ(effect.velocityScale(), 0.0f);
    effect.setVelocityScale(20.0f);
    EXPECT_FLOAT_EQ(effect.velocityScale(), 16.0f);
    effect.setBlendMode(-1.0f);
    EXPECT_FLOAT_EQ(effect.blendMode(), 0.0f);
    effect.setBlendMode(2.0f);
    EXPECT_FLOAT_EQ(effect.blendMode(), 1.0f);
}
