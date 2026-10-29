#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.OpticalFlowBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 32;
constexpr int kHeight = 16;
constexpr float kAlpha = 0.6f;

float patternValue(int x, int y)
{
    return static_cast<float>((x * 37 + y * 17 + x * y * 3) % 101) / 100.0f;
}

ImageF32x4RGBAWithCache makePatternSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = patternValue(x, y);
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = kAlpha;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class SameFrameSampler final : public IEffectFrameSampler {
public:
    explicit SameFrameSampler(const ImageF32x4RGBAWithCache& frame,
                              bool previousFrameAvailable = true)
        : frame_(frame), previousFrameAvailable_(previousFrameAvailable)
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
        if (!previousFrameAvailable_) {
            return false;
        }
        out = frame_;
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
    ImageF32x4RGBAWithCache frame_;
    bool previousFrameAvailable_ = true;
};

} // namespace

TEST(OpticalFlowBlurEffectContractTest, IdenticalFramesProduceNoBlurAndKeepAlpha)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current);
    EffectContext context;
    context.sampler = &sampler;

    OpticalFlowBlurEffect effect;
    effect.setBlurAmount(0.8f);
    effect.setSampleCount(12);
    effect.setFlowSmoothness(0.6f);
    effect.setVelocityScale(1.5f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            const float expected = patternValue(x, y);
            EXPECT_NEAR(pixel.r(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.g(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.b(), expected, 1.0e-6f);
            EXPECT_NEAR(pixel.a(), kAlpha, 1.0e-6f);
        }
    }
}

TEST(OpticalFlowBlurEffectContractTest, ZeroBlurAmountPreservesCurrentPixels)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current);
    EffectContext context;
    context.sampler = &sampler;
    OpticalFlowBlurEffect effect;
    effect.setBlurAmount(0.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
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

TEST(OpticalFlowBlurEffectContractTest, MissingPreviousFrameLeavesCurrentImageUnchanged)
{
    const auto current = makePatternSurface();
    SameFrameSampler sampler(current, false);
    EffectContext context;
    context.sampler = &sampler;

    OpticalFlowBlurEffect effect;
    effect.setBlurAmount(1.0f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -1);
    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* expected = current.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}
