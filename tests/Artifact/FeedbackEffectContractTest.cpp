#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.Feedback;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

ImageF32x4RGBAWithCache makeUniformFrame(
    float red, float green, float blue, float alpha)
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

class PreviousFrameSampler final : public IEffectFrameSampler {
public:
    explicit PreviousFrameSampler(const ImageF32x4RGBAWithCache& previous)
        : previous_(previous)
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
        ++sampleCount;
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

    int sampleCount = 0;
    std::int64_t lastFrameOffset = 0;

private:
    ImageF32x4RGBAWithCache previous_;
};

} // namespace

TEST(FeedbackEffectContractTest, AddsDecayedPreviousRgbAndKeepsCurrentAlpha)
{
    const auto current = makeUniformFrame(0.1f, 0.2f, 0.3f, 0.35f);
    const auto previous = makeUniformFrame(0.4f, 0.2f, 0.1f, 0.9f);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    FeedbackEffect effect;
    effect.setAmount(0.5f);
    effect.setDecay(0.5f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.sampleCount, 1);
    EXPECT_EQ(sampler.lastFrameOffset, -1);
    const auto pixel = output.image().getPixel(2, 2);
    EXPECT_NEAR(pixel.r(), 0.2f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.25f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.325f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.35f, 1.0e-6f);
}

TEST(FeedbackEffectContractTest, ZoomTransformsPreviousFrameSampleCoordinates)
{
    const auto current = makeUniformFrame(0.0f, 0.0f, 0.0f, 0.3f);
    std::array<float, kWidth * kHeight * 4> previousPixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = static_cast<float>(y * kWidth + x) / 15.0f;
            previousPixels[offset] = value;
            previousPixels[offset + 1] = value;
            previousPixels[offset + 2] = value;
            previousPixels[offset + 3] = 0.9f;
        }
    }
    ImageF32x4_RGBA previousImage;
    previousImage.setFromRGBA32F(previousPixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache previous(previousImage);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    FeedbackEffect effect;
    effect.setAmount(1.0f);
    effect.setDecay(1.0f);
    effect.setZoom(2.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const auto pixel = output.image().getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), 5.0f / 15.0f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 5.0f / 15.0f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 5.0f / 15.0f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.3f, 1.0e-6f);
    EXPECT_EQ(sampler.lastFrameOffset, -1);
}

TEST(FeedbackEffectContractTest, RotationTransformsPreviousFrameSampleCoordinates)
{
    const auto current = makeUniformFrame(0.0f, 0.0f, 0.0f, 0.3f);
    std::array<float, kWidth * kHeight * 4> previousPixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = static_cast<float>(y * kWidth + x) / 15.0f;
            previousPixels[offset] = value;
            previousPixels[offset + 1] = value;
            previousPixels[offset + 2] = value;
            previousPixels[offset + 3] = 0.9f;
        }
    }
    ImageF32x4_RGBA previousImage;
    previousImage.setFromRGBA32F(previousPixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache previous(previousImage);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;

    FeedbackEffect effect;
    effect.setAmount(1.0f);
    effect.setDecay(1.0f);
    effect.setRotation(90.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    const auto pixel = output.image().getPixel(0, 3);
    EXPECT_NEAR(pixel.r(), 1.0f / 15.0f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 1.0f / 15.0f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 1.0f / 15.0f, 1.0e-6f);
    EXPECT_NEAR(pixel.a(), 0.3f, 1.0e-6f);
    EXPECT_EQ(sampler.lastFrameOffset, -1);
}
