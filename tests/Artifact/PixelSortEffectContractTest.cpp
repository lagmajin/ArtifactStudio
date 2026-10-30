#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.PixelSort;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

using Pixels = std::array<float, 16>;

ArtifactCore::ImageF32x4RGBAWithCache makeImage(const Pixels& pixels)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

class MissingHistorySampler final : public Artifact::IEffectFrameSampler {
public:
    bool sampleCurrentLayerFrame(std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override { return false; }
    bool sampleCurrentLayerFrameRelative(std::int64_t offset,
        ArtifactCore::ImageF32x4RGBAWithCache&) override
    {
        requestedOffsets.push_back(offset);
        return false;
    }
    bool sampleNamedInput(const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override { return false; }

    std::vector<std::int64_t> requestedOffsets;
};

class FixedHistorySampler final : public Artifact::IEffectFrameSampler {
public:
    explicit FixedHistorySampler(
        ArtifactCore::ImageF32x4RGBAWithCache previous)
        : previous_(std::move(previous))
    {
    }

    bool sampleCurrentLayerFrame(std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override { return false; }
    bool sampleCurrentLayerFrameRelative(
        std::int64_t offset,
        ArtifactCore::ImageF32x4RGBAWithCache& output) override
    {
        requestedOffsets.push_back(offset);
        output = previous_;
        return true;
    }
    bool sampleNamedInput(const QString&, std::int64_t,
        ArtifactCore::ImageF32x4RGBAWithCache&) override { return false; }

    std::vector<std::int64_t> requestedOffsets;

private:
    ArtifactCore::ImageF32x4RGBAWithCache previous_;
};

void expectPixelsEqual(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                       const Pixels& expected)
{
    const float* actual = image.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
    }
}

} // namespace

TEST(PixelSortEffectContractTest, MissingHistoryPreservesInputPixels)
{
    const Pixels pixels{
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, 0.1f, 1.0f,
    };
    MissingHistorySampler sampler;
    Artifact::EffectContext context;
    context.sampler = &sampler;
    Artifact::PixelSortEffect effect;
    effect.setContext(context);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(makeImage(pixels), output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1}));
    expectPixelsEqual(output, pixels);
}

TEST(PixelSortEffectContractTest,
     PreviousFrameMotionDrivesDirectionalSorting)
{
    constexpr int width = 8;
    constexpr int height = 1;
    std::array<float, width * 4> currentPixels{};
    std::array<float, width * 4> previousPixels{};
    for (int x = 0; x < width; ++x) {
        const float value = x == 4 ? 1.0f : 0.0f;
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        currentPixels[offset] = value;
        currentPixels[offset + 1] = value;
        currentPixels[offset + 2] = value;
        currentPixels[offset + 3] = 0.6f;

        const float previousValue = x == 0 ? 1.0f : 0.0f;
        previousPixels[offset] = previousValue;
        previousPixels[offset + 1] = previousValue;
        previousPixels[offset + 2] = previousValue;
        previousPixels[offset + 3] = 0.6f;
    }

    ArtifactCore::ImageF32x4_RGBA currentImage;
    currentImage.setFromRGBA32F(currentPixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(currentImage);
    ArtifactCore::ImageF32x4_RGBA previousImage;
    previousImage.setFromRGBA32F(previousPixels.data(), width, height);
    FixedHistorySampler sampler(
        ArtifactCore::ImageF32x4RGBAWithCache(previousImage));
    Artifact::EffectContext context;
    context.sampler = &sampler;
    Artifact::PixelSortEffect effect;
    effect.setContext(context);
    effect.setSortLength(2);
    effect.setSortOrder(0.0f);
    effect.setBlend(1.0f);
    ArtifactCore::ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedOffsets, (std::vector<std::int64_t>{-1}));
    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(4, 0).a(), 0.6f);
    EXPECT_FLOAT_EQ(source.image().getPixel(4, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(4, 0).a(), 0.6f);
}

TEST(PixelSortEffectContractTest, SettersClampConfiguredRanges)
{
    Artifact::PixelSortEffect effect;
    effect.setSortLength(0);
    EXPECT_EQ(effect.sortLength(), 2);
    effect.setSortLength(100);
    EXPECT_EQ(effect.sortLength(), 64);
    effect.setSortKey(-1.0f);
    EXPECT_FLOAT_EQ(effect.sortKey(), 0.0f);
    effect.setSortOrder(2.0f);
    EXPECT_FLOAT_EQ(effect.sortOrder(), 1.0f);
    effect.setBlend(-1.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 0.0f);
    effect.setBlend(2.0f);
    EXPECT_FLOAT_EQ(effect.blend(), 1.0f);
}
