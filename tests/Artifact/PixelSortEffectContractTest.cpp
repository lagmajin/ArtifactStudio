#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.PixelSort;
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
                              bool available = true)
        : frame_(frame), available_(available)
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
        return frameOffset == -1 && available_;
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
    bool available_;
};

} // namespace

TEST(PixelSortEffectContractTest, IdenticalFramesDoNotCreateMotionSortArtifacts)
{
    const auto source = makePatternSurface();
    SameFrameSampler sampler(source);
    EffectContext context;
    context.sampler = &sampler;

    PixelSortEffect effect;
    effect.setSortLength(12);
    effect.setSortKey(0.0f);
    effect.setSortOrder(0.0f);
    effect.setBlend(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

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

TEST(PixelSortEffectContractTest, MissingPreviousFramePreservesCurrentPixels)
{
    const auto source = makePatternSurface();
    SameFrameSampler sampler(source, false);
    EffectContext context;
    context.sampler = &sampler;
    PixelSortEffect effect;
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}

TEST(PixelSortEffectContractTest, SortControlsClampToSupportedRanges)
{
    PixelSortEffect effect;
    effect.setSortLength(1);
    effect.setSortKey(-0.5f);
    effect.setSortOrder(2.0f);
    effect.setBlend(-1.0f);

    EXPECT_EQ(effect.sortLength(), 2);
    EXPECT_FLOAT_EQ(effect.sortKey(), 0.0f);
    EXPECT_FLOAT_EQ(effect.sortOrder(), 1.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 0.0f);

    effect.setSortLength(100);
    effect.setSortKey(1.5f);
    effect.setSortOrder(-1.0f);
    effect.setBlend(2.0f);

    EXPECT_EQ(effect.sortLength(), 64);
    EXPECT_FLOAT_EQ(effect.sortKey(), 1.0f);
    EXPECT_FLOAT_EQ(effect.sortOrder(), 0.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
}
