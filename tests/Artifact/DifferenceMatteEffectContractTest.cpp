#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
constexpr float kInputAlpha = 0.35f;

ImageF32x4RGBAWithCache makeCurrentFrame()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = x < 2 ? 0.6f : 0.9f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = kInputAlpha;
        }
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

ImageF32x4RGBAWithCache makeReferenceFrame()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.5f;
        pixels[i + 1] = 0.5f;
        pixels[i + 2] = 0.5f;
        pixels[i + 3] = 0.8f;
    }

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class ReferenceFrameSampler final : public IEffectFrameSampler {
public:
    explicit ReferenceFrameSampler(const ImageF32x4RGBAWithCache& reference)
        : reference_(reference)
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
        out = reference_;
        return frameOffset == -3;
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
    ImageF32x4RGBAWithCache reference_;
};

} // namespace

TEST(DifferenceMatteEffectContractTest, ThresholdsReferenceDifferenceAndSetsOpaqueMatte)
{
    const auto current = makeCurrentFrame();
    const auto reference = makeReferenceFrame();
    ReferenceFrameSampler sampler(reference);
    EffectContext context;
    context.sampler = &sampler;

    DifferenceMatteEffect effect;
    effect.setReferenceOffset(3);
    effect.setThreshold(0.25f);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -3);
    const auto belowThreshold = output.image().getPixel(0, 1);
    const auto aboveThreshold = output.image().getPixel(3, 1);
    EXPECT_FLOAT_EQ(belowThreshold.r(), 0.0f);
    EXPECT_FLOAT_EQ(belowThreshold.g(), 0.0f);
    EXPECT_FLOAT_EQ(belowThreshold.b(), 0.0f);
    EXPECT_FLOAT_EQ(belowThreshold.a(), 1.0f);
    EXPECT_FLOAT_EQ(aboveThreshold.r(), 1.0f);
    EXPECT_FLOAT_EQ(aboveThreshold.g(), 1.0f);
    EXPECT_FLOAT_EQ(aboveThreshold.b(), 1.0f);
    EXPECT_FLOAT_EQ(aboveThreshold.a(), 1.0f);
}

TEST(DifferenceMatteEffectContractTest, MissingReferencePreservesCurrentFrame)
{
    const auto current = makeCurrentFrame();
    const auto unavailableReference = makeReferenceFrame();
    ReferenceFrameSampler sampler(unavailableReference);
    EffectContext context;
    context.sampler = &sampler;
    DifferenceMatteEffect effect;
    effect.setReferenceOffset(2);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(current, output);

    EXPECT_EQ(sampler.lastFrameOffset, -2);
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

TEST(DifferenceMatteEffectContractTest, ThresholdBoundaryIsInclusive)
{
    const auto current = makeCurrentFrame();
    const auto reference = makeReferenceFrame();
    ReferenceFrameSampler sampler(reference);
    EffectContext context;
    context.sampler = &sampler;
    DifferenceMatteEffect effect;
    effect.setReferenceOffset(3);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    const float exactDifference = 0.6f - 0.5f;
    effect.setThreshold(exactDifference);
    effect.applyCPUOnly(current, output);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 1).r(), 1.0f);

    effect.setThreshold(std::nextafter(exactDifference, 1.0f));
    effect.applyCPUOnly(current, output);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 1).r(), 0.0f);
}
