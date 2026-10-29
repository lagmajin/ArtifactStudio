#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.DeBlink;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
constexpr float kAlpha = 0.55f;

ImageF32x4RGBAWithCache makeUniformFrame(float value)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = kAlpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class NeighborFrameSampler final : public IEffectFrameSampler {
public:
    NeighborFrameSampler(const ImageF32x4RGBAWithCache& previous,
                         const ImageF32x4RGBAWithCache& next)
        : previous_(previous), next_(next)
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t frameOffset,
        ImageF32x4RGBAWithCache& out) override
    {
        if (frameOffset == -1) {
            ++previousRequests;
            out = previous_;
            return true;
        }
        if (frameOffset == 1) {
            ++nextRequests;
            out = next_;
            return true;
        }
        return false;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    int previousRequests = 0;
    int nextRequests = 0;

private:
    ImageF32x4RGBAWithCache previous_;
    ImageF32x4RGBAWithCache next_;
};

} // namespace

TEST(DeBlinkEffectContractTest, CorrectsIsolatedExposureSpikeAndKeepsAlpha)
{
    const auto previous = makeUniformFrame(0.3f);
    const auto current = makeUniformFrame(0.8f);
    const auto next = makeUniformFrame(0.3f);
    NeighborFrameSampler sampler(previous, next);
    EffectContext context;
    context.sampler = &sampler;

    DeBlinkEffect effect;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.previousRequests, 1);
    EXPECT_EQ(sampler.nextRequests, 1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).g(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).b(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), kAlpha, 1.0e-6f);
}

TEST(DeBlinkEffectContractTest, LeavesNeighborDisagreementUncorrectedAtSceneCut)
{
    const auto previous = makeUniformFrame(0.1f);
    const auto current = makeUniformFrame(0.9f);
    const auto next = makeUniformFrame(0.8f);
    NeighborFrameSampler sampler(previous, next);
    EffectContext context;
    context.sampler = &sampler;

    DeBlinkEffect effect;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.9f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), kAlpha, 1.0e-6f);
}

TEST(DeBlinkEffectContractTest, MissingNeighborSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformFrame(0.8f);
    ImageF32x4RGBAWithCache output;
    DeBlinkEffect effect;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), 0.8f);
            EXPECT_FLOAT_EQ(pixel.g(), 0.8f);
            EXPECT_FLOAT_EQ(pixel.b(), 0.8f);
            EXPECT_FLOAT_EQ(pixel.a(), kAlpha);
        }
    }
}

TEST(DeBlinkEffectContractTest, ZeroStrengthKeepsExposureSpikeEvenWhenNeighborsAreAvailable)
{
    const auto previous = makeUniformFrame(0.3f);
    const auto current = makeUniformFrame(0.8f);
    const auto next = makeUniformFrame(0.3f);
    NeighborFrameSampler sampler(previous, next);
    EffectContext context;
    context.sampler = &sampler;
    DeBlinkEffect effect;
    effect.setStrength(0.0);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.previousRequests, 1);
    EXPECT_EQ(sampler.nextRequests, 1);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), 0.8f);
            EXPECT_FLOAT_EQ(actual.g(), 0.8f);
            EXPECT_FLOAT_EQ(actual.b(), 0.8f);
            EXPECT_FLOAT_EQ(actual.a(), kAlpha);
        }
    }
}
