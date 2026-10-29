#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TimeWarp;
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

class OffsetFrameSampler final : public Artifact::IEffectFrameSampler {
public:
    explicit OffsetFrameSampler(std::vector<std::pair<std::int64_t, Pixel>> frames)
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
        for (const auto& [frameOffset, pixel] : frames_) {
            if (frameOffset == offset) {
                out = makePixel(pixel);
                return true;
            }
        }
        return false;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::vector<std::int64_t> requestedOffsets;

private:
    std::vector<std::pair<std::int64_t, Pixel>> frames_;
};

void applyWithSampler(
    Artifact::TimeWarpEffect& effect,
    OffsetFrameSampler& sampler,
    const Pixel& current,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(current), output);
}

} // namespace

TEST(TimeWarpEffectContractTest, BrightDriverSelectsFutureFrame)
{
    OffsetFrameSampler sampler({
        {1, Pixel{0.9f, 0.8f, 0.7f, 0.6f}},
    });
    Artifact::TimeWarpEffect effect;
    effect.setChannel(4);
    effect.setMaxOffsetFrames(2.0f);
    effect.setSmoothness(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, Pixel{0.2f, 0.3f, 0.4f, 0.75f}, output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{1}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], 0.9f);
    EXPECT_FLOAT_EQ(result[1], 0.8f);
    EXPECT_FLOAT_EQ(result[2], 0.7f);
    EXPECT_FLOAT_EQ(result[3], 0.6f);
}

TEST(TimeWarpEffectContractTest, DarkDriverSelectsPastFrameAndMissingSampleKeepsSource)
{
    OffsetFrameSampler sampler({});
    Artifact::TimeWarpEffect effect;
    effect.setChannel(4);
    effect.setMaxOffsetFrames(2.0f);
    effect.setSmoothness(0.0f);
    const Pixel current{0.2f, 0.3f, 0.4f, 0.25f};
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, current, output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < current.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], current[channel]);
    }
}

TEST(TimeWarpEffectContractTest, SamplesEachUniqueOffsetOnlyOnce)
{
    OffsetFrameSampler sampler({
        {1, Pixel{0.7f, 0.7f, 0.7f, 0.7f}},
    });
    Artifact::TimeWarpEffect effect;
    effect.setChannel(4);
    effect.setMaxOffsetFrames(2.0f);
    effect.setSmoothness(0.0f);
    constexpr std::array<float, 8> pixels{
        0.1f, 0.2f, 0.3f, 0.75f,
        0.4f, 0.5f, 0.6f, 0.75f,
    };
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 1);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);

    effect.applyCPUOnly(ArtifactCore::ImageF32x4RGBAWithCache(image), output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{1}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_FLOAT_EQ(result[channel], 0.7f);
        EXPECT_FLOAT_EQ(result[4 + channel], 0.7f);
    }
}

TEST(TimeWarpEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::TimeWarpEffect effect;
    effect.setMaxOffsetFrames(0.0f);
    EXPECT_FLOAT_EQ(effect.maxOffsetFrames(), 1.0f);
    effect.setChannel(-1);
    EXPECT_EQ(effect.channel(), 0);
    effect.setChannel(8);
    EXPECT_EQ(effect.channel(), 4);
    effect.setSmoothness(-1.0f);
    EXPECT_FLOAT_EQ(effect.smoothness(), 0.0f);
    effect.setSmoothness(2.0f);
    EXPECT_FLOAT_EQ(effect.smoothness(), 1.0f);
}