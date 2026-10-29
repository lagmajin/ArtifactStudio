#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.MotionTrail;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 16;
constexpr float kAlpha = 0.6f;

float patternValue(int x, int y)
{
    return static_cast<float>((x * 37 + y * 17 + x * y * 3) % 101) / 100.0f;
}

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = patternValue(x, y);
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = kAlpha;
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

} // namespace

TEST(MotionTrailEffectContractTest, IdenticalFramesProduceNoTrailAndKeepAlpha)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current);
    EffectContext context;
    context.sampler = &sampler;

    MotionTrailEffect effect;
    effect.setTrailLength(4);
    effect.setDecay(0.6f);
    effect.setVelocityScale(1.0f);
    effect.setBlendMode(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            const float expected = patternValue(x, y);
            EXPECT_NEAR(pixel.r(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), kAlpha, 1.0e-6f);
        }
    }
}

TEST(MotionTrailEffectContractTest, MissingHistorySamplerPreservesCurrentFrame)
{
    const auto current = makePatternSurface();
    MotionTrailEffect effect;
    effect.setTrailLength(8);
    effect.setDecay(0.8f);
    effect.setVelocityScale(2.0f);
    effect.setBlendMode(1.0f);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            const float expected = patternValue(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), expected);
            EXPECT_FLOAT_EQ(pixel.g(), expected);
            EXPECT_FLOAT_EQ(pixel.b(), expected);
            EXPECT_FLOAT_EQ(pixel.a(), kAlpha);
        }
    }
}
