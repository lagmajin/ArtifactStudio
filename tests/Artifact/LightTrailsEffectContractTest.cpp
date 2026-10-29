#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.LightTrails;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 16;

float patternValue(int x, int y)
{
    return static_cast<float>(y * kWidth + x) /
           static_cast<float>(kWidth * kHeight - 1);
}

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = patternValue(x, y);
            pixels[offset] = value;
            pixels[offset + 1] = value * 0.7f;
            pixels[offset + 2] = value * 0.4f;
            pixels[offset + 3] = 0.6f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class SameFrameSampler final : public IEffectFrameSampler {
public:
    explicit SameFrameSampler(const ImageF32x4RGBAWithCache& frame,
                              bool previousFrameAvailable = true)
        : frame_(frame), previousFrameAvailable_(previousFrameAvailable)
    {
    }

    bool sampleCurrentLayerFrame(std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t frameOffset,
        ImageF32x4RGBAWithCache& out) override
    {
        lastFrameOffset = frameOffset;
        if (!previousFrameAvailable_) {
            return false;
        }
        out = frame_;
        return frameOffset == -1;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::int64_t lastFrameOffset = 0;

private:
    ImageF32x4RGBAWithCache frame_;
    bool previousFrameAvailable_ = true;
};

} // namespace

TEST(LightTrailsEffectContractTest, IdenticalFramesProduceNoTrailAndKeepChannels)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current);
    EffectContext context;
    context.sampler = &sampler;

    LightTrailsEffect effect;
    effect.setTrailLength(6);
    effect.setDecay(0.6f);
    effect.setLuminanceThreshold(0.0f);
    effect.setVelocityScale(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            const float value = patternValue(x, y);
            EXPECT_NEAR(pixel.r(), value, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), value * 0.7f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), value * 0.4f, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), 0.6f, 1.0e-6f);
        }
    }
}

TEST(LightTrailsEffectContractTest, SettersClampTrailAndThresholdRanges)
{
    LightTrailsEffect effect;

    effect.setTrailLength(0);
    EXPECT_EQ(effect.trailLength(), 1);
    effect.setTrailLength(100);
    EXPECT_EQ(effect.trailLength(), 16);

    effect.setDecay(-0.5f);
    EXPECT_FLOAT_EQ(effect.decay(), 0.0f);
    effect.setDecay(1.5f);
    EXPECT_FLOAT_EQ(effect.decay(), 1.0f);

    effect.setLuminanceThreshold(-0.5f);
    EXPECT_FLOAT_EQ(effect.luminanceThreshold(), 0.0f);
    effect.setLuminanceThreshold(1.5f);
    EXPECT_FLOAT_EQ(effect.luminanceThreshold(), 1.0f);

    effect.setVelocityScale(-1.0f);
    EXPECT_FLOAT_EQ(effect.velocityScale(), 0.0f);
    effect.setVelocityScale(20.0f);
    EXPECT_FLOAT_EQ(effect.velocityScale(), 16.0f);
}

TEST(LightTrailsEffectContractTest, MissingPreviousFrameLeavesCurrentImageUnchanged)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current, false);
    EffectContext context;
    context.sampler = &sampler;

    LightTrailsEffect effect;
    effect.setTrailLength(8);
    effect.setDecay(0.8f);
    effect.setLuminanceThreshold(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* expected = current.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}
