#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.FrameBlend;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeImage(
    const float* pixels, int width, int height)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, width, height);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class SingleFrameSampler final : public Artifact::IEffectFrameSampler {
public:
    explicit SingleFrameSampler(ArtifactCore::ImageF32x4RGBAWithCache previous)
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
    Artifact::FrameBlendEffect& effect,
    SingleFrameSampler* sampler,
    const ArtifactCore::ImageF32x4RGBAWithCache& source,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = sampler;
    effect.setContext(context);
    effect.applyCPUOnly(source, output);
}

} // namespace

TEST(FrameBlendEffectContractTest, BlendsAllChannelsAndExtendsSmallerPreviousFrameAtEdges)
{
    constexpr std::array<float, 8> currentPixels{
        0.2f, 0.4f, 0.6f, 0.8f,
        0.8f, 0.6f, 0.4f, 0.2f,
    };
    constexpr std::array<float, 4> previousPixel{0.6f, 0.2f, 0.4f, 0.4f};
    const auto source = makeImage(currentPixels.data(), 2, 1);
    SingleFrameSampler sampler(makeImage(previousPixel.data(), 1, 1));
    Artifact::FrameBlendEffect effect;
    effect.setBlend(0.25f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, &sampler, source, output);

    EXPECT_EQ(sampler.requestedOffset, -1);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result[0], 0.3f, 1.0e-6f);
    EXPECT_NEAR(result[1], 0.35f, 1.0e-6f);
    EXPECT_NEAR(result[2], 0.55f, 1.0e-6f);
    EXPECT_NEAR(result[3], 0.7f, 1.0e-6f);
    EXPECT_NEAR(result[4], 0.75f, 1.0e-6f);
    EXPECT_NEAR(result[5], 0.5f, 1.0e-6f);
    EXPECT_NEAR(result[6], 0.4f, 1.0e-6f);
    EXPECT_NEAR(result[7], 0.25f, 1.0e-6f);
}

TEST(FrameBlendEffectContractTest, ZeroBlendDoesNotSampleHistory)
{
    constexpr std::array<float, 4> pixels{0.25f, 0.5f, 0.75f, 0.4f};
    const auto source = makeImage(pixels.data(), 1, 1);
    SingleFrameSampler sampler(makeImage(pixels.data(), 1, 1));
    Artifact::FrameBlendEffect effect;
    effect.setBlend(0.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, &sampler, source, output);

    EXPECT_EQ(sampler.requestedOffset, 0);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel]);
    }
}

TEST(FrameBlendEffectContractTest, MissingPreviousFrameLeavesCurrentPixelsUnchanged)
{
    constexpr std::array<float, 4> pixels{0.25f, 0.5f, 0.75f, 0.4f};
    const auto source = makeImage(pixels.data(), 1, 1);
    SingleFrameSampler sampler(makeImage(pixels.data(), 1, 1));
    sampler.fail = true;
    Artifact::FrameBlendEffect effect;
    effect.setBlend(0.75f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    applyWithSampler(effect, &sampler, source, output);

    EXPECT_EQ(sampler.requestedOffset, -1);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel]);
    }
}

TEST(FrameBlendEffectContractTest, SetterClampsBlendRange)
{
    Artifact::FrameBlendEffect effect;
    effect.setBlend(-1.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 0.0f);
    effect.setBlend(2.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
    effect.setBlend(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
}