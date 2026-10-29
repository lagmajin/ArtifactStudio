#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TimeBlur;
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
        if (frameOffset == -1) {
            out = previous_;
            return true;
        }
        if (frameOffset == 1) {
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

TEST(TimeBlurEffectContractTest, CenteredModeUsesGaussianWeightedPastAndFutureFrames)
{
    constexpr float sigma = 0.5f;
    const auto current = makeUniformSurface(0.2f, 0.3f);
    const auto previous = makeUniformSurface(0.1f, 0.2f);
    const auto future = makeUniformSurface(0.8f, 0.9f);
    OffsetFrameSampler sampler(previous, future);
    EffectContext context;
    context.sampler = &sampler;

    TimeBlurEffect effect;
    effect.setSigma(sigma);
    effect.setLookback(1);
    effect.setDirection(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const float weight = std::exp(-2.0f);
    const float expectedColor = (0.2f + weight * (0.1f + 0.8f)) /
                                (1.0f + 2.0f * weight);
    const float expectedAlpha = (0.3f + weight * (0.2f + 0.9f)) /
                                (1.0f + 2.0f * weight);
    EXPECT_EQ(sampler.sampleCount, 2);
    EXPECT_EQ(sampler.requestedOffsets[0], -1);
    EXPECT_EQ(sampler.requestedOffsets[1], 1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), expectedColor, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), expectedAlpha, 1.0e-6f);
}

TEST(TimeBlurEffectContractTest, TrailingModeSamplesOnlyPastFrames)
{
    constexpr float sigma = 0.5f;
    const auto current = makeUniformSurface(0.2f, 0.3f);
    const auto previous = makeUniformSurface(0.1f, 0.2f);
    const auto future = makeUniformSurface(0.8f, 0.9f);
    OffsetFrameSampler sampler(previous, future);
    EffectContext context;
    context.sampler = &sampler;

    TimeBlurEffect effect;
    effect.setSigma(sigma);
    effect.setLookback(1);
    effect.setDirection(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const float weight = std::exp(-2.0f);
    const float expectedColor = (0.2f + weight * 0.1f) / (1.0f + weight);
    const float expectedAlpha = (0.3f + weight * 0.2f) / (1.0f + weight);
    EXPECT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.requestedOffsets[0], -1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), expectedColor, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), expectedAlpha, 1.0e-6f);
}

TEST(TimeBlurEffectContractTest, DirectionBoundaryAtHalfStillSamplesPastAndFuture)
{
    constexpr float sigma = 0.5f;
    const auto current = makeUniformSurface(0.2f, 0.3f);
    const auto previous = makeUniformSurface(0.1f, 0.2f);
    const auto future = makeUniformSurface(0.8f, 0.9f);
    OffsetFrameSampler sampler(previous, future);
    EffectContext context;
    context.sampler = &sampler;

    TimeBlurEffect effect;
    effect.setSigma(sigma);
    effect.setLookback(1);
    effect.setDirection(0.5f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const float weight = std::exp(-2.0f);
    const float expected = (0.2f + weight * (0.1f + 0.8f)) /
                           (1.0f + 2.0f * weight);
    EXPECT_EQ(sampler.sampleCount, 2);
    EXPECT_EQ(sampler.requestedOffsets[0], -1);
    EXPECT_EQ(sampler.requestedOffsets[1], 1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), expected, 1.0e-6f);
}

TEST(TimeBlurEffectContractTest, MissingFrameSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformSurface(0.37f, 0.62f);
    ImageF32x4RGBAWithCache output;
    TimeBlurEffect effect;
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
