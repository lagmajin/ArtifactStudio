#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.TemporalDenoise;
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

class FixedFrameSampler final : public IEffectFrameSampler {
public:
    explicit FixedFrameSampler(const ImageF32x4RGBAWithCache& history)
        : history_(history)
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
        ++relativeSampleCount;
        lastFrameOffset = frameOffset;
        out = history_;
        return true;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    int relativeSampleCount = 0;
    std::int64_t lastFrameOffset = 0;

private:
    ImageF32x4RGBAWithCache history_;
};

} // namespace

TEST(TemporalDenoiseEffectContractTest, BlendsPreviousFrameAndRequestsNegativeRelativeOffsets)
{
    const auto current = makeUniformSurface(0.2f, 0.3f);
    const auto history = makeUniformSurface(0.8f, 0.7f);
    FixedFrameSampler sampler(history);
    EffectContext context;
    context.sampler = &sampler;

    TemporalDenoiseEffect effect;
    effect.setStrength(1.0f);
    effect.setFrameCount(1);
    effect.setMotionAdaptive(false);
    effect.setRejectSceneCuts(false);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.relativeSampleCount, 1);
    EXPECT_EQ(sampler.lastFrameOffset, -1);
    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.8f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.7f, 1.0e-6f);
}

TEST(TemporalDenoiseEffectContractTest, RejectsHistoryAcrossConfiguredSceneCut)
{
    const auto current = makeUniformSurface(0.1f, 0.4f);
    const auto history = makeUniformSurface(0.9f, 0.8f);
    FixedFrameSampler sampler(history);
    EffectContext context;
    context.sampler = &sampler;

    TemporalDenoiseEffect effect;
    effect.setStrength(1.0f);
    effect.setFrameCount(1);
    effect.setSceneCutThreshold(0.2f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_NEAR(output.image().getPixel(1, 1).r(), 0.1f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(1, 1).a(), 0.4f, 1.0e-6f);
}

TEST(TemporalDenoiseEffectContractTest, MissingFrameSamplerPreservesCurrentFrame)
{
    const auto current = makeUniformSurface(0.37f, 0.62f);
    ImageF32x4RGBAWithCache output;
    TemporalDenoiseEffect effect;
    effect.setStrength(1.0f);
    effect.setFrameCount(3);
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
