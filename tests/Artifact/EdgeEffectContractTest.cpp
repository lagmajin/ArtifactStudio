#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Rasterizer.Edge;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

class PreviousFrameSampler final : public IEffectFrameSampler {
public:
    explicit PreviousFrameSampler(const ImageF32x4RGBAWithCache& previous)
        : previous_(previous)
    {
    }

    bool sampleCurrentLayerFrame(std::int64_t,
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

    bool sampleNamedInput(const QString&,
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

TEST(EdgeEffectContractTest, SobelEdgeFindsStepAndLeavesImageBorderUntouched)
{
    constexpr int width = 5;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float value = x < 2 ? 0.0f : 1.0f;
            const int offset = (y * width + x) * 4;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.5f;
        }
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    EdgeEffect effect;
    effect.setMode(0.0f);
    effect.setIntensity(1.0f);
    effect.setThreshold(0.1f);

    effect.applyCPUOnly(source, output);

    EXPECT_GT(output.image().getPixel(2, 2).r(), 0.1f);
    EXPECT_NEAR(output.image().getPixel(3, 2).r(), 0.0f, 1.0e-6f);
    const auto border = output.image().getPixel(0, 2);
    EXPECT_NEAR(border.r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(border.a(), 0.5f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(2, 2).a(), 1.0f, 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(2, 2).r(), 1.0f, 1.0e-6f);
}

TEST(EdgeEffectContractTest, InvertedFlatImageTurnsInteriorWhiteAndKeepsBorder)
{
    constexpr int width = 5;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel * 4] = 0.25f;
        pixels[pixel * 4 + 1] = 0.25f;
        pixels[pixel * 4 + 2] = 0.25f;
        pixels[pixel * 4 + 3] = 0.4f;
    }
    const auto source = makeSurface(pixels.data(), width, height);
    ImageF32x4RGBAWithCache output;
    EdgeEffect effect;
    effect.setMode(0.0f);
    effect.setIntensity(1.0f);
    effect.setThreshold(0.1f);
    effect.setInvert(1.0f);

    effect.applyCPUOnly(source, output);

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), 1.0f, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), 1.0f, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), 1.0f, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), 1.0f, 1.0e-6f);
        }
    }
    EXPECT_NEAR(output.image().getPixel(0, 2).r(), 0.25f, 1.0e-6f);
    EXPECT_NEAR(output.image().getPixel(0, 2).a(), 0.4f, 1.0e-6f);
}

TEST(EdgeEffectContractTest, MotionModeOutputsThresholdedDifferenceFromPreviousFrame)
{
    constexpr int width = 3;
    constexpr int height = 3;
    std::array<float, width * height * 4> currentPixels{};
    std::array<float, width * height * 4> previousPixels{};
    for (int pixel = 0; pixel < width * height; ++pixel) {
        currentPixels[pixel * 4] = 0.8f;
        currentPixels[pixel * 4 + 1] = 0.8f;
        currentPixels[pixel * 4 + 2] = 0.8f;
        currentPixels[pixel * 4 + 3] = 0.4f;
        previousPixels[pixel * 4] = 0.2f;
        previousPixels[pixel * 4 + 1] = 0.2f;
        previousPixels[pixel * 4 + 2] = 0.2f;
        previousPixels[pixel * 4 + 3] = 0.7f;
    }
    const auto current = makeSurface(currentPixels.data(), width, height);
    const auto previous = makeSurface(previousPixels.data(), width, height);
    PreviousFrameSampler sampler(previous);
    EffectContext context;
    context.sampler = &sampler;
    EdgeEffect effect;
    effect.setMode(1.0f);
    effect.setIntensity(1.0f);
    effect.setThreshold(0.1f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    const auto edge = output.image().getPixel(1, 1);
    EXPECT_NEAR(edge.r(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(edge.g(), 0.6f, 1.0e-6f);
    EXPECT_NEAR(edge.b(), 0.6f, 1.0e-6f);
    EXPECT_FLOAT_EQ(edge.a(), 1.0f);
    EXPECT_FLOAT_EQ(current.image().getPixel(1, 1).r(), 0.8f);
}
\n
