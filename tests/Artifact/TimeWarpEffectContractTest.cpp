#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TimeWarp;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeUniformSurface(float value, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value;
        pixels[i + 2] = value;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class OffsetFrameSampler final : public IEffectFrameSampler {
public:
    OffsetFrameSampler(
        const ImageF32x4RGBAWithCache& previous,
        const ImageF32x4RGBAWithCache& future)
        : previous_(previous), future_(future)
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
        if (sampleCount < static_cast<int>(requestedOffsets.size())) {
            requestedOffsets[static_cast<std::size_t>(sampleCount)] = frameOffset;
        }
        ++sampleCount;
        if (frameOffset == -2) {
            out = previous_;
            return true;
        }
        if (frameOffset == 0) {
            out = previous_;
            return true;
        }
        if (frameOffset == 2) {
            out = future_;
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

    std::array<std::int64_t, 2> requestedOffsets{};
    int sampleCount = 0;

private:
    ImageF32x4RGBAWithCache previous_;
    ImageF32x4RGBAWithCache future_;
};

} // namespace

TEST(TimeWarpEffectContractTest, BrightAlphaSamplesPositiveOffsetOnce)
{
    const auto current = makeUniformSurface(0.2f, 0.75f);
    const auto previous = makeUniformSurface(0.1f, 0.1f);
    const auto future = makeUniformSurface(0.9f, 0.95f);
    OffsetFrameSampler sampler(previous, future);
    EffectContext context;
    context.sampler = &sampler;

    TimeWarpEffect effect;
    effect.setMaxOffsetFrames(4.0f);
    effect.setChannel(4);
    effect.setSmoothness(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.requestedOffsets[0], 2);
    EXPECT_NEAR(output.image().getPixel(2, 2).r(), 0.9f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 2).a(), 0.95f, 1.0e-6f);
}

TEST(TimeWarpEffectContractTest, DarkAlphaSamplesNegativeOffsetOnce)
{
    const auto current = makeUniformSurface(0.8f, 0.25f);
    const auto previous = makeUniformSurface(0.1f, 0.05f);
    const auto future = makeUniformSurface(0.9f, 0.95f);
    OffsetFrameSampler sampler(previous, future);
    EffectContext context;
    context.sampler = &sampler;

    TimeWarpEffect effect;
    effect.setMaxOffsetFrames(4.0f);
    effect.setChannel(4);
    effect.setSmoothness(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.requestedOffsets[0], -2);
    EXPECT_NEAR(output.image().getPixel(2, 2).r(), 0.1f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 2).a(), 0.05f, 1.0e-6f);
}

TEST(TimeWarpEffectContractTest, MissingFrameSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformSurface(0.37f, 0.62f);
    ImageF32x4RGBAWithCache output;
    TimeWarpEffect effect;
    effect.setMaxOffsetFrames(12.0f);
    effect.setChannel(4);
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

TEST(TimeWarpEffectContractTest, ZeroOffsetSamplesTheCurrentCompositionFrame)
{
    const auto current = makeUniformSurface(0.5f, 0.3f);
    const auto frameAtZero = makeUniformSurface(0.8f, 0.9f);
    const auto unusedFuture = makeUniformSurface(0.1f, 0.1f);
    OffsetFrameSampler sampler(frameAtZero, unusedFuture);
    EffectContext context;
    context.sampler = &sampler;

    TimeWarpEffect effect;
    effect.setMaxOffsetFrames(4.0f);
    effect.setChannel(0);
    effect.setSmoothness(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.requestedOffsets[0], 0);
    EXPECT_NEAR(output.image().getPixel(2, 2).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 2).a(), 0.9f, 1.0e-6f);
}
