#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.PosterizeTime;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeUniformFrame(float value, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = value;
        pixels[i + 1] = value * 0.8f;
        pixels[i + 2] = value * 0.6f;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class FixedFrameSampler final : public IEffectFrameSampler {
public:
    explicit FixedFrameSampler(const ImageF32x4RGBAWithCache& sampledFrame,
                               bool available = true)
        : sampledFrame_(sampledFrame), available_(available)
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t frame,
        ImageF32x4RGBAWithCache& out) override
    {
        ++sampleCount;
        lastRequestedFrame = frame;
        out = sampledFrame_;
        return available_;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    int sampleCount = 0;
    std::int64_t lastRequestedFrame = -1;

private:
    ImageF32x4RGBAWithCache sampledFrame_;
    bool available_;
};

} // namespace

TEST(PosterizeTimeEffectContractTest, ReusesQuantizedFrameUntilNextSamplingBoundary)
{
    const auto sampledFrame = makeUniformFrame(0.25f, 0.4f);
    const auto firstInput = makeUniformFrame(0.6f, 0.7f);
    const auto heldInput = makeUniformFrame(0.9f, 0.8f);
    FixedFrameSampler sampler(sampledFrame);
    PosterizeTimeEffect effect;
    effect.setFrameRate(12.0f);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 2;
    context.sampler = &sampler;
    effect.setContext(context);

    ImageF32x4RGBAWithCache firstOutput;
    effect.applyCPUOnly(firstInput, firstOutput);
    EXPECT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.lastRequestedFrame, 2);
    EXPECT_NEAR(firstOutput.image().getPixel(1, 1).r(), 0.25f, 1.0e-6f);
    EXPECT_NEAR(firstOutput.image().getPixel(1, 1).a(), 0.4f, 1.0e-6f);

    context.compositionFrame = 3;
    effect.setContext(context);
    ImageF32x4RGBAWithCache heldOutput;
    effect.applyCPUOnly(heldInput, heldOutput);

    EXPECT_EQ(sampler.sampleCount, 1);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).r(), 0.25f, 1.0e-6f);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).g(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).b(), 0.15f, 1.0e-6f);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).a(), 0.4f, 1.0e-6f);
}

TEST(PosterizeTimeEffectContractTest, MissingSampleFallsBackToAndHoldsFirstInputFrame)
{
    const auto unavailableFrame = makeUniformFrame(0.25f, 0.4f);
    const auto firstInput = makeUniformFrame(0.6f, 0.7f);
    const auto nextInput = makeUniformFrame(0.9f, 0.8f);
    FixedFrameSampler sampler(unavailableFrame, false);
    PosterizeTimeEffect effect;
    effect.setFrameRate(12.0f);
    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 2;
    context.sampler = &sampler;
    effect.setContext(context);

    ImageF32x4RGBAWithCache firstOutput;
    effect.applyCPUOnly(firstInput, firstOutput);
    EXPECT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.lastRequestedFrame, 2);
    EXPECT_NEAR(firstOutput.image().getPixel(1, 1).r(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(firstOutput.image().getPixel(1, 1).a(), 0.7f, 1.0e-6f);

    context.compositionFrame = 3;
    effect.setContext(context);
    ImageF32x4RGBAWithCache heldOutput;
    effect.applyCPUOnly(nextInput, heldOutput);

    EXPECT_EQ(sampler.sampleCount, 1);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).r(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).g(), 0.48f, 1.0e-6f);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).b(), 0.36f, 1.0e-6f);
    EXPECT_NEAR(heldOutput.image().getPixel(1, 1).a(), 0.7f, 1.0e-6f);
}

TEST(PosterizeTimeEffectContractTest, SamplesAgainAtTheNextQuantizedFrameBoundary)
{
    const auto sampledFrame = makeUniformFrame(0.25f, 0.4f);
    const auto input = makeUniformFrame(0.9f, 0.8f);
    FixedFrameSampler sampler(sampledFrame);
    PosterizeTimeEffect effect;
    effect.setFrameRate(12.0f);
    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 2;
    context.sampler = &sampler;
    effect.setContext(context);

    ImageF32x4RGBAWithCache firstOutput;
    effect.applyCPUOnly(input, firstOutput);
    ASSERT_EQ(sampler.sampleCount, 1);
    ASSERT_EQ(sampler.lastRequestedFrame, 2);

    context.compositionFrame = 4;
    effect.setContext(context);
    ImageF32x4RGBAWithCache boundaryOutput;
    effect.applyCPUOnly(input, boundaryOutput);

    EXPECT_EQ(sampler.sampleCount, 2);
    EXPECT_EQ(sampler.lastRequestedFrame, 4);
    EXPECT_NEAR(boundaryOutput.image().getPixel(1, 1).r(), 0.25f, 1.0e-6f);
    EXPECT_NEAR(boundaryOutput.image().getPixel(1, 1).a(), 0.4f, 1.0e-6f);
}

TEST(PosterizeTimeEffectContractTest, FrameRateCannotFallBelowOneFramePerSecond)
{
    PosterizeTimeEffect effect;
    effect.setFrameRate(0.25f);

    EXPECT_FLOAT_EQ(effect.frameRate(), 1.0f);
}
