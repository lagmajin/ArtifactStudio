#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TemporalMedian;
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
    explicit FixedHistorySampler(std::array<Pixel, 3> frames)
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
        if (offset > -1 || offset < -3) {
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
    std::array<Pixel, 3> frames_;
};

void applyWithSampler(
    Artifact::TemporalMedianEffect& effect,
    FixedHistorySampler& sampler,
    ArtifactCore::ImageF32x4RGBAWithCache& output,
    const Pixel& current)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(current), output);
}

} // namespace

TEST(TemporalMedianEffectContractTest, ReplacesEveryChannelWithPastFrameMedian)
{
    FixedHistorySampler sampler({
        Pixel{0.1f, 0.8f, 0.2f, 0.1f},
        Pixel{0.5f, 0.2f, 0.9f, 0.3f},
        Pixel{0.3f, 0.5f, 0.4f, 0.8f},
    });
    Artifact::TemporalMedianEffect effect;
    effect.setFrameCount(3);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    applyWithSampler(effect, sampler, output, Pixel{0.9f, 0.4f, 0.6f, 0.7f});

    EXPECT_EQ(sampler.requestedOffsets,
              (std::vector<std::int64_t>{-1, -2, -3}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], 0.3f);
    EXPECT_FLOAT_EQ(result[1], 0.5f);
    EXPECT_FLOAT_EQ(result[2], 0.4f);
    EXPECT_FLOAT_EQ(result[3], 0.3f);
}

TEST(TemporalMedianEffectContractTest, LogicalRedSelectionBlendsOnlyRedBackingChannel)
{
    FixedHistorySampler sampler({
        Pixel{0.1f, 0.8f, 0.2f, 0.1f},
        Pixel{0.5f, 0.2f, 0.9f, 0.3f},
        Pixel{0.3f, 0.5f, 0.4f, 0.8f},
    });
    Artifact::TemporalMedianEffect effect;
    effect.setFrameCount(3);
    effect.setChannel(1);
    effect.setBlend(0.5f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    applyWithSampler(effect, sampler, output, Pixel{0.9f, 0.4f, 0.6f, 0.7f});

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], 0.9f);
    EXPECT_FLOAT_EQ(result[1], 0.4f);
    EXPECT_NEAR(result[2], (0.6f + 0.4f) * 0.5f, 1.0e-6f);
    EXPECT_FLOAT_EQ(result[3], 0.7f);
}

TEST(TemporalMedianEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::TemporalMedianEffect effect;
    effect.setFrameCount(0);
    EXPECT_EQ(effect.frameCount(), 3);
    effect.setFrameCount(20);
    EXPECT_EQ(effect.frameCount(), 16);

    effect.setBlend(-1.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 0.0f);
    effect.setBlend(2.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
    effect.setBlend(std::numeric_limits<float>::quiet_NaN());
    EXPECT_TRUE(std::isnan(effect.blend()));

    effect.setChannel(-1);
    EXPECT_EQ(effect.channel(), 0);
    effect.setChannel(8);
    EXPECT_EQ(effect.channel(), 4);
}