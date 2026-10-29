#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TrailFade;
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
            pixels[offset + 1] = value * 0.8f;
            pixels[offset + 2] = value * 0.6f;
            pixels[offset + 3] = 0.7f;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class SameFrameSampler final : public IEffectFrameSampler {
public:
    explicit SameFrameSampler(const ImageF32x4RGBAWithCache& frame)
        : frame_(frame)
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
};

class MissingPreviousFrameSampler final : public IEffectFrameSampler {
public:
    bool sampleCurrentLayerFrame(std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t frameOffset,
        ImageF32x4RGBAWithCache&) override
    {
        lastFrameOffset = frameOffset;
        return false;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::int64_t lastFrameOffset = 0;
};

} // namespace

TEST(TrailFadeEffectContractTest, IdenticalFramesProduceNoTrailAndKeepChannels)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current);
    EffectContext context;
    context.sampler = &sampler;

    TrailFadeEffect effect;
    effect.setTrailLength(8);
    effect.setFadePower(2.0f);
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
            EXPECT_NEAR(pixel.g(), value * 0.8f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), value * 0.6f, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), 0.7f, 1.0e-6f);
        }
    }
}

TEST(TrailFadeEffectContractTest, MissingPreviousFramePreservesCurrentImage)
{
    const auto current = makePatternSurface();
    MissingPreviousFrameSampler sampler;
    EffectContext context;
    context.sampler = &sampler;

    TrailFadeEffect effect;
    effect.setTrailLength(8);
    effect.setFadePower(2.0f);
    effect.setVelocityScale(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    ASSERT_EQ(output.width(), kWidth);
    ASSERT_EQ(output.height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const float value = patternValue(x, y);
            EXPECT_NEAR(actual.r(), value, 1.0e-6f);
            EXPECT_NEAR(actual.g(), value * 0.8f, 1.0e-6f);
            EXPECT_NEAR(actual.b(), value * 0.6f, 1.0e-6f);
            EXPECT_NEAR(actual.a(), 0.7f, 1.0e-6f);
        }
    }
}
