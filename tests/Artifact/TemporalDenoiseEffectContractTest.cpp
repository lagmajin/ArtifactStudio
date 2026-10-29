#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TemporalDenoise;
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

class FixedHistorySampler final : public Artifact::IEffectFrameSampler {
public:
    explicit FixedHistorySampler(std::vector<Pixel> frames)
        : frames_(std::move(frames))
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
        requestedOffsets.push_back(offset);
        if (offset >= 0 || static_cast<std::size_t>(-offset) > frames_.size()) {
            return false;
        }
        out = makePixel(frames_[static_cast<std::size_t>(-offset - 1)]);
        return true;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::vector<std::int64_t> requestedOffsets;

private:
    std::vector<Pixel> frames_;
};

void applyWithSampler(
    Artifact::TemporalDenoiseEffect& effect,
    FixedHistorySampler& sampler,
    const Pixel& current,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(current), output);
}

} // namespace

TEST(TemporalDenoiseEffectContractTest, BlendsStableHistoryByConfiguredStrength)
{
    FixedHistorySampler sampler({
        Pixel{0.4f, 0.4f, 0.4f, 0.2f},
        Pixel{0.4f, 0.4f, 0.4f, 0.2f},
    });
    Artifact::TemporalDenoiseEffect effect;
    effect.setFrameCount(2);
    effect.setStrength(0.5f);
    effect.setRejectSceneCuts(false);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, Pixel{0.6f, 0.6f, 0.6f, 0.8f}, output);

    EXPECT_EQ(sampler.requestedOffsets,
              (std::vector<std::int64_t>{-1, -2}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.5f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.5f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.5f, 1.0e-6f);
    EXPECT_NEAR(result[3], 0.5f, 1.0e-6f);
}

TEST(TemporalDenoiseEffectContractTest, SuppressesHistoryAtHighTemporalVariance)
{
    FixedHistorySampler sampler({
        Pixel{0.2f, 0.2f, 0.2f, 0.2f},
        Pixel{0.8f, 0.8f, 0.8f, 0.8f},
    });
    Artifact::TemporalDenoiseEffect effect;
    effect.setFrameCount(2);
    effect.setStrength(1.0f);
    effect.setVarianceThreshold(0.1f);
    effect.setMotionAdaptive(true);
    effect.setGhostSuppression(1.0f);
    effect.setRejectSceneCuts(false);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, Pixel{0.5f, 0.5f, 0.5f, 0.5f}, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], 0.5f);
    EXPECT_FLOAT_EQ(result[1], 0.5f);
    EXPECT_FLOAT_EQ(result[2], 0.5f);
    EXPECT_FLOAT_EQ(result[3], 0.5f);
}

TEST(TemporalDenoiseEffectContractTest, RejectsSceneCutAndKeepsCurrentFrame)
{
    FixedHistorySampler sampler({Pixel{0.1f, 0.1f, 0.1f, 0.2f}});
    Artifact::TemporalDenoiseEffect effect;
    effect.setFrameCount(1);
    effect.setStrength(1.0f);
    effect.setSceneCutThreshold(0.25f);
    effect.setRejectSceneCuts(true);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, Pixel{0.8f, 0.8f, 0.8f, 0.7f}, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], 0.8f);
    EXPECT_FLOAT_EQ(result[1], 0.8f);
    EXPECT_FLOAT_EQ(result[2], 0.8f);
    EXPECT_FLOAT_EQ(result[3], 0.7f);
}

TEST(TemporalDenoiseEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::TemporalDenoiseEffect effect;
    effect.setFrameCount(0);
    EXPECT_EQ(effect.frameCount(), 1);
    effect.setFrameCount(12);
    EXPECT_EQ(effect.frameCount(), 8);
    effect.setStrength(-1.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 0.0f);
    effect.setStrength(2.0f);
    EXPECT_FLOAT_EQ(effect.strength(), 1.0f);
    effect.setVarianceThreshold(-1.0f);
    EXPECT_FLOAT_EQ(effect.varianceThreshold(), 0.0f);
    effect.setGhostSuppression(2.0f);
    EXPECT_FLOAT_EQ(effect.ghostSuppression(), 1.0f);
    effect.setSceneCutThreshold(-1.0f);
    EXPECT_FLOAT_EQ(effect.sceneCutThreshold(), 0.0f);
}