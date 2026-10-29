#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Feedback;
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
        requestedOffsets.push_back(offset);
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
    std::vector<std::int64_t> requestedOffsets;

private:
    ArtifactCore::ImageF32x4RGBAWithCache previous_;
};

void applyWithSampler(
    Artifact::FeedbackEffect& effect,
    PreviousFrameSampler& sampler,
    const Pixel& current,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(current), output);
}

} // namespace

TEST(FeedbackEffectContractTest, AddsDecayedPreviousRgbAndPreservesCurrentAlpha)
{
    PreviousFrameSampler sampler(makePixel(Pixel{0.8f, 0.4f, 0.2f, 1.0f}));
    Artifact::FeedbackEffect effect;
    effect.setAmount(0.5f);
    effect.setDecay(0.5f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, Pixel{0.2f, 0.4f, 0.6f, 0.3f}, output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1}));
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.5f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.65f, 1.0e-6f);
    EXPECT_FLOAT_EQ(result[3], 0.3f);
}

TEST(FeedbackEffectContractTest, ZeroAmountDoesNotReadHistory)
{
    PreviousFrameSampler sampler(makePixel(Pixel{0.8f, 0.8f, 0.8f, 1.0f}));
    Artifact::FeedbackEffect effect;
    effect.setAmount(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    const Pixel current{0.2f, 0.4f, 0.6f, 0.3f};

    applyWithSampler(effect, sampler, current, output);

    EXPECT_TRUE(sampler.requestedOffsets.empty());
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < current.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], current[channel]);
    }
}

TEST(FeedbackEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::FeedbackEffect effect;
    effect.setAmount(-1.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    effect.setAmount(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.amount(), 0.5f);
    effect.setDecay(2.0f);
    EXPECT_FLOAT_EQ(effect.decay(), 1.0f);
    effect.setZoom(0.0f);
    EXPECT_FLOAT_EQ(effect.zoom(), 0.01f);
    effect.setZoom(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.zoom(), 1.0f);
    effect.setRotation(200.0f);
    EXPECT_FLOAT_EQ(effect.rotation(), 180.0f);
}