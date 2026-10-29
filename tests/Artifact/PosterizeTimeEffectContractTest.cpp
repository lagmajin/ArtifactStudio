#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.PosterizeTime;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixel = std::array<float, 4>;

ArtifactCore::ImageF32x4RGBAWithCache makePixel(const Pixel& pixel)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixel.data(), 1, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class AbsoluteFrameSampler final : public Artifact::IEffectFrameSampler {
public:
    explicit AbsoluteFrameSampler(
        std::vector<std::pair<std::int64_t, Pixel>> frames)
        : frames_(std::move(frames))
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t frame, ArtifactCore::ImageF32x4RGBAWithCache& out) override
    {
        requestedFrames.push_back(frame);
        for (const auto& [number, pixel] : frames_) {
            if (number == frame) {
                out = makePixel(pixel);
                return true;
            }
        }
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t, ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(
        const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::vector<std::int64_t> requestedFrames;

private:
    std::vector<std::pair<std::int64_t, Pixel>> frames_;
};

void evaluateAtFrame(
    Artifact::PosterizeTimeEffect& effect,
    AbsoluteFrameSampler& sampler,
    std::int64_t frame,
    ArtifactCore::ImageF32x4RGBAWithCache& output)
{
    Artifact::EffectContext context;
    context.sampler = &sampler;
    context.frameRate = 30.0;
    context.compositionFrame = frame;
    effect.setContext(context);
    effect.applyCPUOnly(makePixel(Pixel{0.9f, 0.9f, 0.9f, 1.0f}), output);
}

} // namespace

TEST(PosterizeTimeEffectContractTest, HoldsSampledFrameWithinOutputRateBucket)
{
    AbsoluteFrameSampler sampler({
        {15, Pixel{0.2f, 0.3f, 0.4f, 0.5f}},
        {18, Pixel{0.6f, 0.7f, 0.8f, 0.9f}},
    });
    Artifact::PosterizeTimeEffect effect;
    effect.setFrameRate(10.0f);
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache sameBucket;
    ArtifactCore::ImageF32x4RGBAWithCache nextBucket;

    evaluateAtFrame(effect, sampler, 17, first);
    evaluateAtFrame(effect, sampler, 16, sameBucket);
    evaluateAtFrame(effect, sampler, 18, nextBucket);

    EXPECT_EQ(sampler.requestedFrames,
              (std::vector<std::int64_t>{15, 18}));
    const float* firstPixels = first.image().rgba32fData();
    const float* heldPixels = sameBucket.image().rgba32fData();
    const float* nextPixels = nextBucket.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(heldPixels, nullptr);
    ASSERT_NE(nextPixels, nullptr);
    EXPECT_FLOAT_EQ(firstPixels[0], 0.2f);
    EXPECT_FLOAT_EQ(heldPixels[0], 0.2f);
    EXPECT_FLOAT_EQ(nextPixels[0], 0.6f);
    EXPECT_FLOAT_EQ(nextPixels[3], 0.9f);
}

TEST(PosterizeTimeEffectContractTest, MissingBucketSampleFallsBackToSourceAndHoldsIt)
{
    AbsoluteFrameSampler sampler({});
    Artifact::PosterizeTimeEffect effect;
    effect.setFrameRate(10.0f);
    ArtifactCore::ImageF32x4RGBAWithCache first;
    ArtifactCore::ImageF32x4RGBAWithCache sameBucket;
    evaluateAtFrame(effect, sampler, 17, first);
    evaluateAtFrame(effect, sampler, 16, sameBucket);

    EXPECT_EQ(sampler.requestedFrames, (std::vector<std::int64_t>{15}));
    const float* firstPixels = first.image().rgba32fData();
    const float* heldPixels = sameBucket.image().rgba32fData();
    ASSERT_NE(firstPixels, nullptr);
    ASSERT_NE(heldPixels, nullptr);
    EXPECT_FLOAT_EQ(firstPixels[0], 0.9f);
    EXPECT_FLOAT_EQ(heldPixels[0], 0.9f);
}

TEST(PosterizeTimeEffectContractTest, FrameRateHasOneFramePerSecondMinimum)
{
    Artifact::PosterizeTimeEffect effect;
    effect.setFrameRate(0.0f);
    EXPECT_FLOAT_EQ(effect.frameRate(), 1.0f);
    effect.setFrameRate(-12.0f);
    EXPECT_FLOAT_EQ(effect.frameRate(), 1.0f);
}