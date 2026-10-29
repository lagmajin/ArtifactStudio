#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.FrameAverage;
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
    Artifact::FrameAverageEffect& effect,
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

TEST(FrameAverageEffectContractTest, AveragesCurrentAndDecayedHistoryWithSharedNormalization)
{
    FixedHistorySampler sampler({
        Pixel{0.8f, 0.8f, 0.8f, 0.2f},
        Pixel{0.4f, 0.4f, 0.4f, 0.8f},
    });
    Artifact::FrameAverageEffect effect;
    effect.setFrameCount(2);
    effect.setTemporalWeight(0.5f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, Pixel{0.2f, 0.2f, 0.2f, 0.4f}, output);

    EXPECT_EQ(sampler.requestedOffsets,
              (std::vector<std::int64_t>{-1, -2}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[3], 0.4f, 1.0e-6f);
}

TEST(FrameAverageEffectContractTest, ZeroTemporalWeightUsesCurrentFrameWithoutSampling)
{
    FixedHistorySampler sampler({Pixel{0.8f, 0.8f, 0.8f, 0.2f}});
    Artifact::FrameAverageEffect effect;
    effect.setFrameCount(1);
    effect.setTemporalWeight(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    const Pixel current{0.2f, 0.4f, 0.6f, 0.8f};

    applyWithSampler(effect, sampler, current, output);

    EXPECT_TRUE(sampler.requestedOffsets.empty());
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < current.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], current[channel]);
    }
}

TEST(FrameAverageEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::FrameAverageEffect effect;
    effect.setFrameCount(0);
    EXPECT_EQ(effect.frameCount(), 1);
    effect.setFrameCount(20);
    EXPECT_EQ(effect.frameCount(), 16);

    effect.setTemporalWeight(-1.0f);
    EXPECT_FLOAT_EQ(effect.temporalWeight(), 0.0f);
    effect.setTemporalWeight(2.0f);
    EXPECT_FLOAT_EQ(effect.temporalWeight(), 1.0f);
    effect.setTemporalWeight(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.temporalWeight(), 1.0f);
}