#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.FrameAverage;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 3;

ImageF32x4RGBAWithCache makeUniformSurface(float value)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = value;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class TwoFrameSampler final : public IEffectFrameSampler {
public:
    TwoFrameSampler(
        const ImageF32x4RGBAWithCache& nearest,
        const ImageF32x4RGBAWithCache& older)
        : frames_{nearest, older}
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
        if (frameOffset > -1 || frameOffset < -2) {
            return false;
        }
        requestedOffsets[static_cast<std::size_t>(sampleCount++)] = frameOffset;
        out = frames_[static_cast<std::size_t>(-frameOffset - 1)];
        return true;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::array<std::int64_t, 2> requestedOffsets{};
    int sampleCount = 0;

private:
    std::array<ImageF32x4RGBAWithCache, 2> frames_;
};

} // namespace

TEST(FrameAverageEffectContractTest, AppliesGeometricWeightsToCurrentAndPreviousFrames)
{
    const auto current = makeUniformSurface(0.2f);
    const auto nearest = makeUniformSurface(0.8f);
    const auto older = makeUniformSurface(0.4f);
    TwoFrameSampler sampler(nearest, older);
    EffectContext context;
    context.sampler = &sampler;

    FrameAverageEffect effect;
    effect.setFrameCount(2);
    effect.setTemporalWeight(0.5f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.sampleCount, 2);
    EXPECT_EQ(sampler.requestedOffsets[0], -1);
    EXPECT_EQ(sampler.requestedOffsets[1], -2);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.4f, 1.0e-6f);
}

TEST(FrameAverageEffectContractTest, MissingSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformSurface(0.37f);
    ImageF32x4RGBAWithCache output;
    FrameAverageEffect effect;
    effect.applyCPUOnly(current, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = current.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}

TEST(FrameAverageEffectContractTest, ZeroTemporalWeightSkipsPreviousFrameSampling)
{
    const auto current = makeUniformSurface(0.37f);
    const auto previous = makeUniformSurface(0.9f);
    TwoFrameSampler sampler(previous, previous);
    EffectContext context;
    context.sampler = &sampler;

    FrameAverageEffect effect;
    effect.setFrameCount(2);
    effect.setTemporalWeight(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.sampleCount, 0);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = current.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}
