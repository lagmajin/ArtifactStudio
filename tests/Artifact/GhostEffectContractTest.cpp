#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Ghost;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeUniformFrame(float red, float green, float blue, float alpha)
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = red;
        pixels[i + 1] = green;
        pixels[i + 2] = blue;
        pixels[i + 3] = alpha;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class AlphaHistorySampler final : public IEffectFrameSampler {
public:
    explicit AlphaHistorySampler(const ImageF32x4RGBAWithCache& history)
        : history_(history)
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
        requestedOffsets[requestCount++] = frameOffset;
        out = history_;
        return frameOffset >= -3 && frameOffset <= -1;
    }

    bool sampleNamedInput(
        const QString&,
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    std::array<std::int64_t, 8> requestedOffsets{};
    std::size_t requestCount = 0;

private:
    ImageF32x4RGBAWithCache history_;
};

} // namespace

TEST(GhostEffectContractTest, AddsOnlyHistoricalAlphaForRequestedFrames)
{
    const auto current = makeUniformFrame(0.2f, 0.3f, 0.4f, 0.2f);
    const auto history = makeUniformFrame(0.9f, 0.8f, 0.7f, 0.5f);
    AlphaHistorySampler sampler(history);
    EffectContext context;
    context.sampler = &sampler;

    GhostEffect effect;
    effect.setOpacity(0.4f);
    effect.setGhostCount(3);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    ASSERT_EQ(sampler.requestCount, 3u);
    EXPECT_EQ(sampler.requestedOffsets[0], -1);
    EXPECT_EQ(sampler.requestedOffsets[1], -2);
    EXPECT_EQ(sampler.requestedOffsets[2], -3);

    const auto pixel = output.image().getPixel(1, 1);
    EXPECT_NEAR(pixel.r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.4f, 1.0e-6f);
    constexpr float expectedAlpha = 0.2f + 0.5f * 0.4f * (1.0f + 0.7f + 0.49f);
    EXPECT_NEAR(pixel.a(), expectedAlpha, 1.0e-6f);
}

TEST(GhostEffectContractTest, MissingHistorySamplerPreservesCurrentFrame)
{
    const auto current = makeUniformFrame(0.2f, 0.3f, 0.4f, 0.2f);
    ImageF32x4RGBAWithCache output;
    GhostEffect effect;
    effect.setOpacity(0.8f);
    effect.setGhostCount(5);

    effect.applyCPUOnly(current, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), 0.2f);
            EXPECT_FLOAT_EQ(actual.g(), 0.3f);
            EXPECT_FLOAT_EQ(actual.b(), 0.4f);
            EXPECT_FLOAT_EQ(actual.a(), 0.2f);
        }
    }
}

TEST(GhostEffectContractTest, CapsAccumulatedHistoryAlphaAtOneWithoutChangingRgb)
{
    const auto current = makeUniformFrame(0.2f, 0.3f, 0.4f, 0.8f);
    const auto history = makeUniformFrame(0.9f, 0.8f, 0.7f, 0.5f);
    AlphaHistorySampler sampler(history);
    EffectContext context;
    context.sampler = &sampler;

    GhostEffect effect;
    effect.setOpacity(1.0f);
    effect.setGhostCount(3);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const auto pixel = output.image().getPixel(1, 1);
    EXPECT_NEAR(pixel.r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.3f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.4f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
}
