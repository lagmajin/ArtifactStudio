#include <gtest/gtest.h>

#include <QString>
#include <QVariant>

#include <array>
#include <cmath>
#include <cstdint>

import Artifact.Effect.Rasterizer.ApertureShapeBlur;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

class FixedDepthInputSampler final : public IEffectFrameSampler {
public:
    FixedDepthInputSampler(int width, int height, float depth)
        : width_(width), height_(height), depth_(depth)
    {
    }

    bool sampleCurrentLayerFrame(
        std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(
        const QString& inputId,
        std::int64_t compositionFrame,
        ImageF32x4RGBAWithCache& out) override
    {
        requestedInput = inputId;
        requestedFrame = compositionFrame;
        if (inputId != QStringLiteral("depth")) {
            return false;
        }
        std::array<float, 17 * 17 * 4> pixels{};
        for (std::size_t pixel = 0;
             pixel < static_cast<std::size_t>(width_) * height_; ++pixel) {
            pixels[pixel * 4] = depth_;
            pixels[pixel * 4 + 3] = 1.0f;
        }
        ImageF32x4_RGBA image;
        image.setFromRGBA32F(pixels.data(), width_, height_);
        out = ImageF32x4RGBAWithCache(image);
        return true;
    }

    QString requestedInput;
    std::int64_t requestedFrame = -1;

private:
    int width_;
    int height_;
    float depth_;
};

} // namespace

TEST(SpatialImageEffectContractTest,
     DepthBokehWithoutDepthOrLumaFallbackPreservesInputExactly)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        pixels[pixel * 4] = static_cast<float>(pixel) / 16.0f;
        pixels[pixel * 4 + 1] = static_cast<float>(pixel + 1) / 20.0f;
        pixels[pixel * 4 + 2] = static_cast<float>(pixel + 2) / 24.0f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel + 3) / 24.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("useLumaFallback")), QVariant(false));
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(SpatialImageEffectContractTest,
     DepthBokehLumaFallbackKeepsUniformInFocusPixelsSharp)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        pixels[pixel * 4] = 0.5f;
        pixels[pixel * 4 + 1] = 0.5f;
        pixels[pixel * 4 + 2] = 0.5f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel + 1) / 36.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("focusDistance")), QVariant(0.5));
    effect.setPropertyValue(
        UniString(QStringLiteral("focusRange")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(SpatialImageEffectContractTest,
     DepthBokehSamplesNamedDepthInputAtCompositionFrame)
{
    constexpr int width = 17;
    constexpr int height = 17;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr std::size_t center =
        static_cast<std::size_t>(height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.8f;
        pixels[pixel * 4 + 2] = 0.8f;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    pixels[center * 4] = 1.0f;
    pixels[center * 4 + 1] = 1.0f;
    pixels[center * 4 + 2] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    FixedDepthInputSampler sampler(width, height, 0.5f);

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("focusDistance")), QVariant(0.5));
    effect.setPropertyValue(
        UniString(QStringLiteral("focusRange")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("maxRadius")), QVariant(8.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
    EffectContext context;
    context.compositionFrame = 42;
    context.sampler = &sampler;
    effect.setContext(context);
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedInput, QStringLiteral("depth"));
    EXPECT_EQ(sampler.requestedFrame, 42);
    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(SpatialImageEffectContractTest,
     DepthBokehFallsBackToLumaWhenNamedDepthSizeDoesNotMatch)
{
    constexpr int width = 17;
    constexpr int height = 17;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr std::size_t center =
        static_cast<std::size_t>(height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.8f;
        pixels[pixel * 4 + 2] = 0.8f;
        pixels[pixel * 4 + 3] = 0.6f;
    }
    pixels[center * 4] = 1.0f;
    pixels[center * 4 + 1] = 1.0f;
    pixels[center * 4 + 2] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fallbackOutput;
    ImageF32x4RGBAWithCache mismatchedDepthOutput;
    FixedDepthInputSampler mismatchedSampler(1, 1, 0.5f);

    const auto configureEffect = [](DepthBokehEffect& effect) {
        effect.setPropertyValue(
            UniString(QStringLiteral("focusDistance")), QVariant(0.5));
        effect.setPropertyValue(
            UniString(QStringLiteral("focusRange")), QVariant(0.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("maxRadius")), QVariant(8.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
    };
    DepthBokehEffect fallbackEffect;
    configureEffect(fallbackEffect);
    fallbackEffect.applyCPUOnly(source, fallbackOutput);

    DepthBokehEffect mismatchedEffect;
    configureEffect(mismatchedEffect);
    EffectContext context;
    context.compositionFrame = 42;
    context.sampler = &mismatchedSampler;
    mismatchedEffect.setContext(context);
    mismatchedEffect.applyCPUOnly(source, mismatchedDepthOutput);

    EXPECT_EQ(mismatchedSampler.requestedInput, QStringLiteral("depth"));
    EXPECT_EQ(mismatchedSampler.requestedFrame, 42);
    const float* fallbackPixels = fallbackOutput.image().rgba32fData();
    const float* mismatchedPixels =
        mismatchedDepthOutput.image().rgba32fData();
    ASSERT_NE(fallbackPixels, nullptr);
    ASSERT_NE(mismatchedPixels, nullptr);
    EXPECT_NE(fallbackPixels[center * 4], pixels[center * 4])
        << "the no-sampler reference must exercise the luma fallback blur";
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(mismatchedPixels[channel], fallbackPixels[channel])
            << "channel=" << channel;
    }
}

TEST(SpatialImageEffectContractTest,
     DepthBokehLumaFallbackBlursOutOfFocusRgbAndPreservesAlpha)
{
    constexpr int width = 33;
    constexpr int height = 33;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr std::size_t center =
        static_cast<std::size_t>(height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        pixels[pixel * 4] = 0.8f;
        pixels[pixel * 4 + 1] = 0.8f;
        pixels[pixel * 4 + 2] = 0.8f;
        pixels[pixel * 4 + 3] = 0.3f +
            0.6f * static_cast<float>(pixel) /
                static_cast<float>(pixelCount - 1);
    }
    pixels[center * 4] = 1.0f;
    pixels[center * 4 + 1] = 1.0f;
    pixels[center * 4 + 2] = 1.0f;

    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("focusDistance")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("focusRange")), QVariant(0.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("maxRadius")), QVariant(8.0));
    effect.setPropertyValue(
        UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    const std::size_t adjacent = center + 1;
    EXPECT_LT(result[center * 4], pixels[center * 4]);
    EXPECT_GT(result[adjacent * 4], pixels[adjacent * 4]);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}

TEST(SpatialImageEffectContractTest,
     DepthBokehRoutesForegroundAndBackgroundBlurToTheirDepthSides)
{
    constexpr int width = 33;
    constexpr int height = 17;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    std::array<float, pixelCount * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            const float value = x < width / 2 ? 0.2f : 0.8f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 0.75f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache foregroundOnly;
    ImageF32x4RGBAWithCache backgroundOnly;

    const auto applyDirectionalBlur = [&source](
        const float foreground, const float background,
        ImageF32x4RGBAWithCache& output) {
        DepthBokehEffect effect;
        effect.setPropertyValue(
            UniString(QStringLiteral("focusDistance")), QVariant(0.5));
        effect.setPropertyValue(
            UniString(QStringLiteral("focusRange")), QVariant(0.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("foregroundBlur")), QVariant(foreground));
        effect.setPropertyValue(
            UniString(QStringLiteral("backgroundBlur")), QVariant(background));
        effect.setPropertyValue(
            UniString(QStringLiteral("maxRadius")), QVariant(8.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
        effect.applyCPUOnly(source, output);
    };
    applyDirectionalBlur(1.0f, 0.0f, foregroundOnly);
    applyDirectionalBlur(0.0f, 1.0f, backgroundOnly);

    ASSERT_EQ(foregroundOnly.image().width(), width);
    ASSERT_EQ(foregroundOnly.image().height(), height);
    ASSERT_EQ(backgroundOnly.image().width(), width);
    ASSERT_EQ(backgroundOnly.image().height(), height);
    const float* foregroundPixels = foregroundOnly.image().rgba32fData();
    const float* backgroundPixels = backgroundOnly.image().rgba32fData();
    ASSERT_NE(foregroundPixels, nullptr);
    ASSERT_NE(backgroundPixels, nullptr);
    bool foregroundChanged = false;
    bool backgroundChanged = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            const bool foregroundSide = x < width / 2;
            const float foregroundDelta = std::abs(
                foregroundPixels[offset] - pixels[offset]);
            const float backgroundDelta = std::abs(
                backgroundPixels[offset] - pixels[offset]);
            if (foregroundSide) {
                foregroundChanged = foregroundChanged ||
                    foregroundDelta > 1.0e-5f;
                EXPECT_FLOAT_EQ(backgroundPixels[offset], pixels[offset]);
            } else {
                backgroundChanged = backgroundChanged ||
                    backgroundDelta > 1.0e-5f;
                EXPECT_FLOAT_EQ(foregroundPixels[offset], pixels[offset]);
            }
            EXPECT_FLOAT_EQ(foregroundPixels[offset + 3], pixels[offset + 3]);
            EXPECT_FLOAT_EQ(backgroundPixels[offset + 3], pixels[offset + 3]);
        }
    }
    EXPECT_TRUE(foregroundChanged);
    EXPECT_TRUE(backgroundChanged);
}

TEST(SpatialImageEffectContractTest,
     DepthBokehPreservesInputWhenNamedDepthIsUnavailableAndFallbackIsDisabled)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        pixels[pixel * 4] = static_cast<float>(pixel + 1) / 20.0f;
        pixels[pixel * 4 + 1] = static_cast<float>(pixel + 2) / 24.0f;
        pixels[pixel * 4 + 2] = static_cast<float>(pixel + 3) / 28.0f;
        pixels[pixel * 4 + 3] = static_cast<float>(pixel + 4) / 32.0f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    FixedDepthInputSampler unavailableSampler(width, height, 0.5f);

    DepthBokehEffect effect;
    effect.setPropertyValue(
        UniString(QStringLiteral("depthInput")), QStringLiteral("missing-depth"));
    effect.setPropertyValue(
        UniString(QStringLiteral("useLumaFallback")), QVariant(false));
    EffectContext context;
    context.compositionFrame = 73;
    context.sampler = &unavailableSampler;
    effect.setContext(context);
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(unavailableSampler.requestedInput,
              QStringLiteral("missing-depth"));
    EXPECT_EQ(unavailableSampler.requestedFrame, 73);
    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "channel=" << channel;
    }
}

TEST(SpatialImageEffectContractTest,
     DepthBokehClampsNamedDepthSamplesToUnitRange)
{
    constexpr int width = 17;
    constexpr int height = 17;
    constexpr std::size_t pixelCount =
        static_cast<std::size_t>(width) * height;
    constexpr std::size_t center =
        static_cast<std::size_t>(height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const float value = (pixel % 2 == 0) ? 0.2f : 0.65f;
        pixels[pixel * 4] = value;
        pixels[pixel * 4 + 1] = value;
        pixels[pixel * 4 + 2] = value;
        pixels[pixel * 4 + 3] = 0.8f;
    }
    pixels[center * 4] = 1.0f;
    pixels[center * 4 + 1] = 1.0f;
    pixels[center * 4 + 2] = 1.0f;
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache negativeDepth;
    ImageF32x4RGBAWithCache zeroDepth;
    ImageF32x4RGBAWithCache aboveDepth;
    ImageF32x4RGBAWithCache unitDepth;

    const auto renderWithDepth = [&source](
        const float depth, ImageF32x4RGBAWithCache& output) {
        FixedDepthInputSampler sampler(width, height, depth);
        DepthBokehEffect effect;
        effect.setPropertyValue(
            UniString(QStringLiteral("focusDistance")), QVariant(0.5));
        effect.setPropertyValue(
            UniString(QStringLiteral("focusRange")), QVariant(0.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("foregroundBlur")), QVariant(0.25));
        effect.setPropertyValue(
            UniString(QStringLiteral("backgroundBlur")), QVariant(1.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("maxRadius")), QVariant(8.0));
        effect.setPropertyValue(
            UniString(QStringLiteral("highlightBoost")), QVariant(0.0));
        EffectContext context;
        context.compositionFrame = 73;
        context.sampler = &sampler;
        effect.setContext(context);
        effect.applyCPUOnly(source, output);
    };
    renderWithDepth(-2.0f, negativeDepth);
    renderWithDepth(0.0f, zeroDepth);
    renderWithDepth(3.0f, aboveDepth);
    renderWithDepth(1.0f, unitDepth);

    const float* negativePixels = negativeDepth.image().rgba32fData();
    const float* zeroPixels = zeroDepth.image().rgba32fData();
    const float* abovePixels = aboveDepth.image().rgba32fData();
    const float* unitPixels = unitDepth.image().rgba32fData();
    ASSERT_NE(negativePixels, nullptr);
    ASSERT_NE(zeroPixels, nullptr);
    ASSERT_NE(abovePixels, nullptr);
    ASSERT_NE(unitPixels, nullptr);
    bool foregroundBackgroundDiffer = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(negativePixels[channel], zeroPixels[channel])
            << "negative sample channel=" << channel;
        EXPECT_FLOAT_EQ(abovePixels[channel], unitPixels[channel])
            << "above-unit sample channel=" << channel;
        foregroundBackgroundDiffer = foregroundBackgroundDiffer ||
            std::abs(zeroPixels[channel] - unitPixels[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(foregroundBackgroundDiffer)
        << "foreground and background blur settings must make the clamp endpoints observable";
}

TEST(SpatialImageEffectContractTest, ApertureBlurSpreadsRgbAndPreservesAlphaAndInput)
{
    constexpr int width = 33;
    constexpr int height = 33;
    constexpr int pixelCount = width * height;
    constexpr int center = (height / 2) * width + width / 2;
    std::array<float, pixelCount * 4> pixels{};
    for (int index = 0; index < pixelCount; ++index) {
        pixels[index * 4 + 3] = 0.25f + 0.5f *
            static_cast<float>(index % width) / static_cast<float>(width - 1);
    }
    pixels[center * 4] = 1.0f;

    ImageF32x4_RGBA sourceImage;
    sourceImage.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(sourceImage);
    ImageF32x4RGBAWithCache output;
    ApertureShapeBlurEffect effect;
    effect.setPropertyValue(UniString(QStringLiteral("Radius")), QVariant(8.0));
    effect.setPropertyValue(UniString(QStringLiteral("Edge Brightness")), QVariant(0.0));
    effect.setPropertyValue(UniString(QStringLiteral("Highlight Boost")), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    EXPECT_EQ(output.image().colorDescriptor(), source.image().colorDescriptor());
    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    EXPECT_FLOAT_EQ(source.image().getPixel(width / 2, height / 2).r(), 1.0f);

    int affectedPixels = 0;
    float centerRed = 0.0f;
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    for (int index = 0; index < pixelCount; ++index) {
        const float red = outputPixels[index * 4];
        const float green = outputPixels[index * 4 + 1];
        const float blue = outputPixels[index * 4 + 2];
        const float alpha = outputPixels[index * 4 + 3];
        ASSERT_TRUE(std::isfinite(red)) << "pixel=" << index;
        ASSERT_TRUE(std::isfinite(green)) << "pixel=" << index;
        ASSERT_TRUE(std::isfinite(blue)) << "pixel=" << index;
        ASSERT_TRUE(std::isfinite(alpha)) << "pixel=" << index;
        if (red > 1e-5f) {
            ++affectedPixels;
        }
        EXPECT_FLOAT_EQ(sourcePixels[index * 4], pixels[index * 4])
            << "source red pixel=" << index;
        EXPECT_FLOAT_EQ(sourcePixels[index * 4 + 1], pixels[index * 4 + 1])
            << "source green pixel=" << index;
        EXPECT_FLOAT_EQ(sourcePixels[index * 4 + 2], pixels[index * 4 + 2])
            << "source blue pixel=" << index;
        EXPECT_FLOAT_EQ(sourcePixels[index * 4 + 3], pixels[index * 4 + 3])
            << "source alpha pixel=" << index;
        EXPECT_NEAR(alpha, pixels[index * 4 + 3], 1e-6f) << "pixel=" << index;
        if (index == center) {
            centerRed = red;
        }
    }

    EXPECT_GT(affectedPixels, 1);
    EXPECT_GT(centerRed, 0.0f);
    EXPECT_LT(centerRed, 1.0f);
}
