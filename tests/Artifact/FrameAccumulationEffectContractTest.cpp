#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.FrameAccumulation;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixel = std::array<float, 4>;

ArtifactCore::ImageF32x4RGBAWithCache makePixel(const Pixel& pixel)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixel.data(), 1, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class PreviousFrameSampler final : public Artifact::IEffectFrameSampler {
public:
    explicit PreviousFrameSampler(ArtifactCore::ImageF32x4RGBAWithCache previous)
        : previous_(std::move(previous))
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t, ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t offset, ArtifactCore::ImageF32x4RGBAWithCache& out) override
    {
        requestedOffset = offset;
        if (fail) return false;
        out = previous_;
        return true;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool fail = false;
    std::int64_t requestedOffset = 0;

private:
    ArtifactCore::ImageF32x4RGBAWithCache previous_;
};

void applyWithSampler(
    Artifact::FrameAccumulationEffect& effect,
    PreviousFrameSampler* sampler,
    const Pixel& current,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(current), output);
}

} // namespace

TEST(FrameAccumulationEffectContractTest, AppliesPersistenceThenCurrentFrameBlend)
{
    PreviousFrameSampler sampler(makePixel(Pixel{0.4f, 0.2f, 0.8f, 0.6f}));
    Artifact::FrameAccumulationEffect effect;
    effect.setPersistence(0.5f);
    effect.setBlend(0.25f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, &sampler, Pixel{0.8f, 0.6f, 0.4f, 0.2f}, output);

    EXPECT_EQ(sampler.requestedOffset, -1);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.65f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.45f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.55f, 1.0e-6f);
    EXPECT_NEAR(result[3], 0.35f, 1.0e-6f);
}

TEST(FrameAccumulationEffectContractTest, SeedsFromCurrentFrameWhenHistoryIsUnavailable)
{
    PreviousFrameSampler sampler(makePixel(Pixel{0.1f, 0.1f, 0.1f, 0.1f}));
    sampler.fail = true;
    Artifact::FrameAccumulationEffect effect;
    effect.setPersistence(0.8f);
    effect.setBlend(0.0f);
    const Pixel current{0.2f, 0.4f, 0.6f, 0.8f};
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, &sampler, current, output);

    EXPECT_EQ(sampler.requestedOffset, -1);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < current.size(); ++channel) {
        EXPECT_NEAR(result[channel], current[channel], 1.0e-6f);
    }
}

TEST(FrameAccumulationEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::FrameAccumulationEffect effect;
    effect.setPersistence(-1.0f);
    EXPECT_FLOAT_EQ(effect.persistence(), 0.0f);
    effect.setPersistence(2.0f);
    EXPECT_FLOAT_EQ(effect.persistence(), 1.0f);
    effect.setPersistence(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.persistence(), 1.0f);

    effect.setBlend(-1.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 0.0f);
    effect.setBlend(2.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
}