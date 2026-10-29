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
import Artifact.Effect.Rasterizer.DeBlink;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeSolidFrame(float value)
{
    Pixels pixels{};
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(kWidth * kHeight); ++pixel) {
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = value;
        pixels[pixel * 4 + 2] = value;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class NeighborFrameSampler final : public Artifact::IEffectFrameSampler {
public:
    NeighborFrameSampler(
        ArtifactCore::ImageF32x4RGBAWithCache previous,
        ArtifactCore::ImageF32x4RGBAWithCache next)
        : previous_(std::move(previous)), next_(std::move(next))
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
            out = previous_;
            return true;
        }
        if (offset == 1) {
            out = next_;
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
    ArtifactCore::ImageF32x4RGBAWithCache previous_;
    ArtifactCore::ImageF32x4RGBAWithCache next_;
};

void applyWithNeighbors(
    Artifact::DeBlinkEffect& effect,
    const ArtifactCore::ImageF32x4RGBAWithCache& source,
    NeighborFrameSampler& sampler,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(source, output);
}

} // namespace

TEST(DeBlinkEffectContractTest, CorrectsSingleFrameAnomalyTowardMatchingNeighbors)
{
    const auto source = makeSolidFrame(0.8f);
    NeighborFrameSampler sampler(makeSolidFrame(0.4f), makeSolidFrame(0.4f));
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DeBlinkEffect effect;

    applyWithNeighbors(effect, source, sampler, output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1, 1}));
    const auto corrected = output.image().getPixel(2, 2);
    EXPECT_NEAR(corrected.r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(corrected.g(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(corrected.b(), 0.4f, 1.0e-6f);
    EXPECT_FLOAT_EQ(corrected.a(), 0.6f);
}

TEST(DeBlinkEffectContractTest, PreservesFrameWhenNeighborDisagreementSignalsSceneCut)
{
    const auto source = makeSolidFrame(0.5f);
    NeighborFrameSampler sampler(makeSolidFrame(0.1f), makeSolidFrame(0.8f));
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DeBlinkEffect effect;

    applyWithNeighbors(effect, source, sampler, output);

    const auto unchanged = output.image().getPixel(2, 2);
    EXPECT_FLOAT_EQ(unchanged.r(), 0.5f);
    EXPECT_FLOAT_EQ(unchanged.g(), 0.5f);
    EXPECT_FLOAT_EQ(unchanged.b(), 0.5f);
    EXPECT_FLOAT_EQ(unchanged.a(), 0.6f);
}

TEST(DeBlinkEffectContractTest, SettersClampAndResetNonFiniteValues)
{
    Artifact::DeBlinkEffect effect;
    effect.setThreshold(-1.0);
    EXPECT_DOUBLE_EQ(effect.threshold(), 0.0);
    effect.setThreshold(std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(effect.threshold(), 0.06);

    effect.setStrength(2.0);
    EXPECT_DOUBLE_EQ(effect.strength(), 1.0);
    effect.setStrength(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(effect.strength(), 1.0);

    effect.setSceneCut(-1.0);
    EXPECT_DOUBLE_EQ(effect.sceneCut(), 0.0);
    effect.setMaxGain(100.0);
    EXPECT_DOUBLE_EQ(effect.maxGain(), 16.0);
}