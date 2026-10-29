#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TemporalMedian;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 3;

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

class ThreeFrameSampler final : public IEffectFrameSampler {
public:
    ThreeFrameSampler(
        const ImageF32x4RGBAWithCache& first,
        const ImageF32x4RGBAWithCache& second,
        const ImageF32x4RGBAWithCache& third)
        : frames_{first, second, third}
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
        if (frameOffset > -1 || frameOffset < -3) {
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

    std::array<std::int64_t, 3> requestedOffsets{};
    int sampleCount = 0;

private:
    std::array<ImageF32x4RGBAWithCache, 3> frames_;
};

} // namespace

TEST(TemporalMedianEffectContractTest, UsesMedianAcrossRequestedHistoryFrames)
{
    const auto current = makeUniformSurface(0.2f, 0.2f);
    const auto first = makeUniformSurface(0.9f, 0.9f);
    const auto second = makeUniformSurface(0.1f, 0.1f);
    const auto third = makeUniformSurface(0.7f, 0.5f);
    ThreeFrameSampler sampler(first, second, third);
    EffectContext context;
    context.sampler = &sampler;

    TemporalMedianEffect effect;
    effect.setFrameCount(3);
    effect.setBlend(1.0f);
    effect.setChannel(0);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(sampler.sampleCount, 3);
    EXPECT_EQ(sampler.requestedOffsets[0], -1);
    EXPECT_EQ(sampler.requestedOffsets[1], -2);
    EXPECT_EQ(sampler.requestedOffsets[2], -3);
    const auto pixel = output.image().getPixel(1, 1);
    EXPECT_NEAR(pixel.r(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.5f, 1.0e-6f);
}

TEST(TemporalMedianEffectContractTest, MissingFrameSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformSurface(0.37f, 0.62f);
    ImageF32x4RGBAWithCache output;
    TemporalMedianEffect effect;
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

TEST(TemporalMedianEffectContractTest, RedChannelModeLeavesOtherCurrentChannelsUntouched)
{
    const auto current = makeUniformSurface(0.2f, 0.2f);
    const auto first = makeUniformSurface(0.9f, 0.9f);
    const auto second = makeUniformSurface(0.1f, 0.1f);
    const auto third = makeUniformSurface(0.7f, 0.5f);
    ThreeFrameSampler sampler(first, second, third);
    EffectContext context;
    context.sampler = &sampler;

    TemporalMedianEffect effect;
    effect.setFrameCount(3);
    effect.setBlend(1.0f);
    effect.setChannel(1);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const auto pixel = output.image().getPixel(1, 1);
    EXPECT_NEAR(pixel.r(), 0.7f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.2f, 1.0e-6f);
}
