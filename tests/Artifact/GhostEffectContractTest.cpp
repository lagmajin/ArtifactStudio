#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Ghost;
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

class GhostHistorySampler final : public Artifact::IEffectFrameSampler {
public:
    explicit GhostHistorySampler(std::array<Pixel, 2> frames)
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
        if (offset != -1 && offset != -2) return false;
        if (failOffset == offset) return false;
        out = makePixel(frames_[static_cast<std::size_t>(-offset - 1)]);
        return true;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::int64_t failOffset = 0;
    std::vector<std::int64_t> requestedOffsets;

private:
    std::array<Pixel, 2> frames_;
};

void applyWithSampler(
    Artifact::GhostEffect& effect,
    GhostHistorySampler& sampler,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(Pixel{0.2f, 0.4f, 0.6f, 0.2f}), output);
}

} // namespace

TEST(GhostEffectContractTest, AddsDecayedHistoryAlphaWithoutChangingRgb)
{
    GhostHistorySampler sampler({
        Pixel{0.9f, 0.8f, 0.7f, 0.4f},
        Pixel{0.6f, 0.5f, 0.4f, 0.8f},
    });
    Artifact::GhostEffect effect;
    effect.setOpacity(0.5f);
    effect.setGhostCount(2);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, output);

    EXPECT_EQ(sampler.requestedOffsets,
              (std::vector<std::int64_t>{-1, -2}));
    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.2f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.6f);
    EXPECT_NEAR(pixel.a(), 0.68f, 1.0e-6f);
}

TEST(GhostEffectContractTest, SaturatesAccumulatedAlphaAndSkipsUnavailableFrames)
{
    GhostHistorySampler sampler({
        Pixel{0.1f, 0.2f, 0.3f, 1.0f},
        Pixel{0.4f, 0.5f, 0.6f, 1.0f},
    });
    sampler.failOffset = -2;
    Artifact::GhostEffect effect;
    effect.setOpacity(1.0f);
    effect.setGhostCount(2);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, sampler, output);

    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.2f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.6f);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
}

TEST(GhostEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::GhostEffect effect;
    effect.setOpacity(-1.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 0.0f);
    effect.setOpacity(2.0f);
    EXPECT_FLOAT_EQ(effect.opacity(), 1.0f);
    effect.setGhostCount(0);
    EXPECT_EQ(effect.ghostCount(), 1);
    effect.setGhostCount(20);
    EXPECT_EQ(effect.ghostCount(), 8);
}