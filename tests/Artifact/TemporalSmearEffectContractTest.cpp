#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TemporalSmear;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 16;
constexpr float kAlpha = 0.7f;

ImageF32x4RGBAWithCache makeRamp(bool shifted)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int sourceX = shifted ? std::clamp(x - 4, 0, kWidth - 1) : x;
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = static_cast<float>(sourceX) /
                               static_cast<float>(kWidth - 1);
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

class PreviousFrameSampler final : public IEffectFrameSampler {
public:
    explicit PreviousFrameSampler(const ImageF32x4RGBAWithCache& previous)
        : previous_(previous)
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
        out = previous_;
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
    ImageF32x4RGBAWithCache previous_;
};

} // namespace

TEST(TemporalSmearEffectContractTest, PreviousFrameMotionSmearsRampWithJitterDisabled)
{
    const auto current = makeRamp(false);
    const auto previous = makeRamp(true);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    TemporalSmearEffect effect;
    effect.setSmearAmount(1.0f);
    effect.setSampleCount(2);
    effect.setSampleJitter(0.0f);
    effect.setVelocityScale(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    EXPECT_LT(output.image().getPixel(12, 4).r(), current.image().getPixel(12, 4).r());
    EXPECT_NEAR(output.image().getPixel(12, 4).a(), kAlpha, 1.0e-6f);
}

TEST(TemporalSmearEffectContractTest, MissingFrameSamplerPreservesCurrentFrame)
{
    const auto current = makeRamp(false);
    ImageF32x4RGBAWithCache output;
    TemporalSmearEffect effect;
    effect.setSmearAmount(1.0f);
    effect.setSampleCount(8);

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
