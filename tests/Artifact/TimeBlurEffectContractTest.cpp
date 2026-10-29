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
import Artifact.Effect.Rasterizer.TimeBlur;
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

class BidirectionalFrameSampler final : public Artifact::IEffectFrameSampler {
public:
    BidirectionalFrameSampler(Pixel previous, Pixel next)
        : previous_(previous), next_(next)
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
        if (offset == -1) {
            out = makePixel(previous_);
            return true;
        }
        if (offset == 1) {
            out = makePixel(next_);
            return true;
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
    Pixel previous_;
    Pixel next_;
};

void applyAtTime(
    Artifact::TimeBlurEffect& effect,
    BidirectionalFrameSampler& sampler,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(Pixel{0.2f, 0.2f, 0.2f, 0.2f}), output);
}

} // namespace

TEST(TimeBlurEffectContractTest, CenteredModeGaussianWeightsPastAndFutureFrames)
{
    BidirectionalFrameSampler sampler(
        Pixel{0.8f, 0.8f, 0.8f, 0.8f},
        Pixel{0.4f, 0.4f, 0.4f, 0.4f});
    Artifact::TimeBlurEffect effect;
    effect.setSigma(1.0f);
    effect.setLookback(1);
    effect.setDirection(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyAtTime(effect, sampler, output);

    EXPECT_EQ(sampler.requestedOffsets,
              (std::vector<std::int64_t>{-1, 1}));
    const float weight = std::exp(-0.5f);
    const float expected = (0.2f + 0.8f * weight + 0.4f * weight)
                         / (1.0f + 2.0f * weight);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_NEAR(result[channel], expected, 1.0e-6f);
    }
}

TEST(TimeBlurEffectContractTest, TrailingModeSamplesPastFramesOnly)
{
    BidirectionalFrameSampler sampler(
        Pixel{0.8f, 0.8f, 0.8f, 0.8f},
        Pixel{0.0f, 0.0f, 0.0f, 0.0f});
    Artifact::TimeBlurEffect effect;
    effect.setSigma(1.0f);
    effect.setLookback(1);
    effect.setDirection(1.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyAtTime(effect, sampler, output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1}));
    const float weight = std::exp(-0.5f);
    const float expected = (0.2f + 0.8f * weight) / (1.0f + weight);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], expected, 1.0e-6f);
    EXPECT_NEAR(result[3], expected, 1.0e-6f);
}

TEST(TimeBlurEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::TimeBlurEffect effect;
    effect.setSigma(0.0f);
    EXPECT_FLOAT_EQ(effect.sigma(), 0.5f);
    effect.setLookback(0);
    EXPECT_EQ(effect.lookback(), 1);
    effect.setLookback(40);
    EXPECT_EQ(effect.lookback(), 32);
    effect.setDirection(-1.0f);
    EXPECT_FLOAT_EQ(effect.direction(), 0.0f);
    effect.setDirection(2.0f);
    EXPECT_FLOAT_EQ(effect.direction(), 1.0f);
}