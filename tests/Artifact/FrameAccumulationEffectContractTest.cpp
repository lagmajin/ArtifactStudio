#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.FrameAccumulation;
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

TEST(FrameAccumulationEffectContractTest, AppliesPersistenceThenCurrentFrameBlend)
{
    const auto current = makeUniformSurface(0.2f);
    const auto previous = makeUniformSurface(0.6f);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    FrameAccumulationEffect effect;
    effect.setPersistence(0.5f);
    effect.setBlend(0.25f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.35f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.35f, 1.0e-6f);
}

TEST(FrameAccumulationEffectContractTest, MissingPreviousFrameSeedsFromCurrentImage)
{
    const auto current = makeUniformSurface(0.37f);
    ImageF32x4RGBAWithCache output;
    FrameAccumulationEffect effect;
    effect.setPersistence(0.8f);
    effect.setBlend(0.25f);

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = current.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_NEAR(after.r(), before.r(), 1.0e-6f);
            EXPECT_NEAR(after.g(), before.g(), 1.0e-6f);
            EXPECT_NEAR(after.b(), before.b(), 1.0e-6f);
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
}

TEST(FrameAccumulationEffectContractTest, FullBlendSelectsCurrentFrameRegardlessOfAccumulation)
{
    const auto current = makeUniformSurface(0.2f);
    const auto previous = makeUniformSurface(0.9f);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    FrameAccumulationEffect effect;
    effect.setPersistence(1.0f);
    effect.setBlend(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), 0.2f);
            EXPECT_FLOAT_EQ(actual.g(), 0.2f);
            EXPECT_FLOAT_EQ(actual.b(), 0.2f);
            EXPECT_FLOAT_EQ(actual.a(), 0.2f);
        }
    }
}
