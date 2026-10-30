#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Rasterizer.ApertureShapeBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 17;
constexpr int kHeight = 17;

ImageF32x4RGBAWithCache makeCheckerboard()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            const float value = ((x + y) % 2 == 0) ? 1.0f : 0.0f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.6f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class DepthMapSampler final : public IEffectFrameSampler {
public:
    explicit DepthMapSampler(const ImageF32x4RGBAWithCache& depth)
        : depth_(depth)
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t,
        ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(
        const QString& inputName,
        std::int64_t frame,
        ImageF32x4RGBAWithCache& out) override
    {
        requestedInput = inputName;
        requestedFrame = frame;
        out = depth_;
        return true;
    }

    QString requestedInput;
    std::int64_t requestedFrame = -1;

private:
    ImageF32x4RGBAWithCache depth_;
};

} // namespace

TEST(DepthBokehEffectContractTest, UsesLumaFallbackToDefocusAndPreservesAlpha)
{
    const auto source = makeCheckerboard();
    ImageF32x4RGBAWithCache output;
    DepthBokehEffect effect;
    effect.setPropertyValue(UniString("focusDistance"), QVariant(0.5));
    effect.setPropertyValue(UniString("focusRange"), QVariant(0.0));
    effect.setPropertyValue(UniString("maxRadius"), QVariant(4.0));
    effect.setPropertyValue(UniString("useLumaFallback"), QVariant(true));

    effect.applyCPUOnly(source, output);

    bool changedRgb = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            changedRgb = changedRgb || std::abs(after.r() - before.r()) > 1.0e-4f;
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
    EXPECT_TRUE(changedRgb);

    effect.setPropertyValue(UniString("useLumaFallback"), QVariant(false));
    ImageF32x4RGBAWithCache unchanged;
    effect.applyCPUOnly(source, unchanged);
    ASSERT_EQ(unchanged.image().width(), kWidth);
    ASSERT_EQ(unchanged.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = unchanged.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(after.r(), before.r());
            EXPECT_FLOAT_EQ(after.g(), before.g());
            EXPECT_FLOAT_EQ(after.b(), before.b());
            EXPECT_FLOAT_EQ(after.a(), before.a());
        }
    }
}

TEST(DepthBokehEffectContractTest, NamedDepthInputTakesPriorityOverLumaFallback)
{
    const auto source = makeCheckerboard();
    std::array<float, kWidth * kHeight * 4> depthPixels{};
    for (std::size_t pixel = 0; pixel < depthPixels.size(); pixel += 4) {
        depthPixels[pixel] = 0.5f;
        depthPixels[pixel + 1] = 0.5f;
        depthPixels[pixel + 2] = 0.5f;
        depthPixels[pixel + 3] = 1.0f;
    }
    ImageF32x4_RGBA depthImage;
    depthImage.setFromRGBA32F(depthPixels.data(), kWidth, kHeight);
    const ImageF32x4RGBAWithCache depth(depthImage);
    DepthMapSampler sampler(depth);
    EffectContext context;
    context.compositionFrame = 42;
    context.sampler = &sampler;

    ImageF32x4RGBAWithCache output;
    DepthBokehEffect effect;
    effect.setPropertyValue(UniString("focusDistance"), QVariant(0.5));
    effect.setPropertyValue(UniString("focusRange"), QVariant(0.0));
    effect.setPropertyValue(UniString("maxRadius"), QVariant(4.0));
    effect.setPropertyValue(UniString("useLumaFallback"), QVariant(true));
    effect.setContext(context);
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedInput, QStringLiteral("depth"));
    EXPECT_EQ(sampler.requestedFrame, 42);
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

TEST(DepthBokehEffectContractTest, MismatchedDepthDimensionsAreRejectedWithoutLumaFallback)
{
    const auto source = makeCheckerboard();
    constexpr int depthWidth = kWidth - 1;
    std::array<float, depthWidth * kHeight * 4> depthPixels{};
    for (std::size_t offset = 0; offset < depthPixels.size(); offset += 4) {
        depthPixels[offset] = 0.5f;
        depthPixels[offset + 1] = 0.5f;
        depthPixels[offset + 2] = 0.5f;
        depthPixels[offset + 3] = 1.0f;
    }
    ImageF32x4_RGBA depthImage;
    depthImage.setFromRGBA32F(depthPixels.data(), depthWidth, kHeight);
    const ImageF32x4RGBAWithCache depth(depthImage);
    DepthMapSampler sampler(depth);
    EffectContext context;
    context.compositionFrame = 17;
    context.sampler = &sampler;

    DepthBokehEffect effect;
    effect.setPropertyValue(UniString("focusDistance"), QVariant(0.5));
    effect.setPropertyValue(UniString("focusRange"), QVariant(0.0));
    effect.setPropertyValue(UniString("maxRadius"), QVariant(4.0));
    effect.setPropertyValue(UniString("useLumaFallback"), QVariant(false));
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedInput, QStringLiteral("depth"));
    EXPECT_EQ(sampler.requestedFrame, 17);
    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    const float* actual = output.image().rgba32fData();
    const float* expected = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    for (std::size_t channel = 0;
         channel < static_cast<std::size_t>(kWidth * kHeight * 4); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}
