#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>

#include <QPointF>
#include <QRectF>
#include <QString>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Artifact.Effect.Context;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Time.Rational;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makePatternSurface()
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = 0.4f;
            pixels[offset + 1] = 0.5f;
            pixels[offset + 2] = 0.6f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    return ImageF32x4RGBAWithCache(image);
}

TEST(LofiEffectContractTest, OldTvPatternedInputProducesFiniteChangedOutput)
{
    constexpr int width = 48;
    constexpr int height = 32;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const float value = (x / 2 + y / 3) % 2 == 0 ? 0.2f : 0.8f;
            pixels[offset] = value;
            pixels[offset + 1] = 1.0f - value;
            pixels[offset + 2] = (x % 5) / 4.0f;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, output);

    const float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    bool changed = false;
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_TRUE(std::isfinite(outputPixels[channel]))
            << "channel=" << channel;
        changed = changed ||
                  std::abs(outputPixels[channel] - sourcePixels[channel]) > 1.0e-5f;
    }
    EXPECT_TRUE(changed);
}

TEST(LofiEffectContractTest, OldTvCpuReferencePreservesConfiguredComputeMode)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::GPU);

    ASSERT_TRUE(effect.supportsCPU());
    ASSERT_TRUE(effect.supportsGPU());
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(effect.computeMode(), ComputeMode::GPU);
    ASSERT_EQ(output.image().width(), source.image().width());
    ASSERT_EQ(output.image().height(), source.image().height());
    const float* pixels = output.image().rgba32fData();
    ASSERT_NE(pixels, nullptr);
    for (std::size_t channel = 0; channel < 8 * 8 * 4; ++channel) {
        EXPECT_TRUE(std::isfinite(pixels[channel])) << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvPreservesInputColorDescriptor)
{
    constexpr std::array<float, 16> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.2f, 0.3f, 0.4f, 0.5f,
        0.3f, 0.4f, 0.5f, 0.6f,
        0.4f, 0.5f, 0.6f, 0.7f,
    };
    const auto descriptor =
        SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 2, 2);
    image.setColorDescriptor(descriptor);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
}

TEST(LofiEffectContractTest, OldTvOutputDoesNotSharePixelStorageWithInput)
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        pixels[channel] = 0.2f + static_cast<float>(channel % 17) * 0.01f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, output);

    float* sourcePixels = source.image().rgba32fData();
    float* outputPixels = output.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(outputPixels, nullptr);
    const float originalOutputChannel0 = outputPixels[0];
    const float originalSourceChannel4 = sourcePixels[4];

    sourcePixels[0] = -17.0f;
    EXPECT_FLOAT_EQ(outputPixels[0], originalOutputChannel0);
    outputPixels[4] = 23.0f;
    EXPECT_FLOAT_EQ(sourcePixels[4], originalSourceChannel4);
}

TEST(LofiEffectContractTest, CommonEditablePropertiesReflectOldTvState)
{
    ArtifactOldTVEffect effect;
    const auto findProperty = [&effect](const QString& name) {
        const auto properties = effect.editableProperties();
        const auto found = std::find_if(
            properties.begin(), properties.end(), [&name](const auto& property) {
                return property && property->getName() == name;
            });
        return found == properties.end()
            ? SharedPtr<AbstractProperty>{}
            : *found;
    };

    const auto defaultMix = findProperty(QStringLiteral("Effect Mix"));
    const auto defaultEnabled = findProperty(QStringLiteral("Effect Enabled"));
    const auto defaultRegion = findProperty(
        QStringLiteral("Effect Region Enabled"));
    const auto defaultOverscan = findProperty(QStringLiteral("Allow Overscan"));
    ASSERT_TRUE(defaultMix);
    ASSERT_TRUE(defaultEnabled);
    ASSERT_TRUE(defaultRegion);
    ASSERT_TRUE(defaultOverscan);
    EXPECT_DOUBLE_EQ(defaultMix->getValue().toDouble(), 1.0);
    EXPECT_TRUE(defaultEnabled->getValue().toBool());
    EXPECT_FALSE(defaultRegion->getValue().toBool());
    EXPECT_FALSE(defaultOverscan->getValue().toBool());

    effect.setMix(0.4f);
    effect.setEnabled(false);
    effect.setAllowOverscan(true);
    effect.setEffectRegion(QRectF(2.0, 3.0, 4.0, 5.0));

    EXPECT_NEAR(findProperty(QStringLiteral("Effect Mix"))->getValue().toDouble(),
                0.4, 1.0e-6);
    EXPECT_FALSE(findProperty(QStringLiteral("Effect Enabled"))->getValue().toBool());
    EXPECT_TRUE(findProperty(QStringLiteral("Effect Region Enabled"))->getValue().toBool());
    EXPECT_TRUE(findProperty(QStringLiteral("Allow Overscan"))->getValue().toBool());
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region X"))->getValue().toDouble(), 2.0);
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region Y"))->getValue().toDouble(), 3.0);
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region Width"))->getValue().toDouble(), 4.0);
    EXPECT_DOUBLE_EQ(findProperty(QStringLiteral("Effect Region Height"))->getValue().toDouble(), 5.0);
}

TEST(LofiEffectContractTest, AnimatedMixControlsOldTvCompositeAtCompositionFrames)
{
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto mix = effect.editableProperty(QStringLiteral("Effect Mix"));
    ASSERT_TRUE(mix);
    mix->addKeyFrame(RationalTime(0, 30), 0.0);
    mix->addKeyFrame(RationalTime(30, 30), 1.0);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 0.0f);
    ImageF32x4RGBAWithCache atZero;
    effect.applyCPUOnly(source, atZero);
    const auto refreshedProperties = effect.editableProperties();
    EXPECT_TRUE(mix->hasKeyFrames());
    EXPECT_TRUE(std::find(refreshedProperties.begin(), refreshedProperties.end(), mix) !=
                refreshedProperties.end())
        << "refreshing editable properties must retain the animated Mix property";

    context.compositionFrame = 15;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 0.5f);
    ImageF32x4RGBAWithCache atHalf;
    effect.applyCPUOnly(source, atHalf);

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FLOAT_EQ(effect.mix(), 1.0f);
    ImageF32x4RGBAWithCache atOne;
    effect.applyCPUOnly(source, atOne);

    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache expectedFullEffect;
    fullEffect.applyCPUOnly(source, expectedFullEffect);
    const float* sourcePixels = source.image().rgba32fData();
    const float* zeroMixPixels = atZero.image().rgba32fData();
    const float* halfMixPixels = atHalf.image().rgba32fData();
    const float* fullMixPixels = atOne.image().rgba32fData();
    const float* expectedPixels = expectedFullEffect.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(zeroMixPixels, nullptr);
    ASSERT_NE(halfMixPixels, nullptr);
    ASSERT_NE(fullMixPixels, nullptr);
    ASSERT_NE(expectedPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(zeroMixPixels[channel], sourcePixels[channel])
            << "zero-mix channel=" << channel;
        EXPECT_NEAR(halfMixPixels[channel],
                    sourcePixels[channel] * 0.5f + expectedPixels[channel] * 0.5f,
                    1.0e-6f)
            << "half-mix channel=" << channel;
        EXPECT_FLOAT_EQ(fullMixPixels[channel], expectedPixels[channel])
            << "full-mix channel=" << channel;
    }
}

TEST(LofiEffectContractTest, AnimatedEnabledSwitchesOldTvBypassAtCompositionFrames)
{
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto enabled = effect.editableProperty(QStringLiteral("Effect Enabled"));
    ASSERT_TRUE(enabled);
    enabled->addKeyFrame(RationalTime(0, 30), false);
    enabled->addKeyFrame(RationalTime(30, 30), true);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_FALSE(effect.isEnabled());
    ImageF32x4RGBAWithCache bypassed;
    effect.applyCPUOnly(source, bypassed);

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_TRUE(effect.isEnabled());
    ImageF32x4RGBAWithCache enabledOutput;
    effect.applyCPUOnly(source, enabledOutput);

    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache expectedFullEffect;
    fullEffect.applyCPUOnly(source, expectedFullEffect);
    const float* sourcePixels = source.image().rgba32fData();
    const float* bypassedPixels = bypassed.image().rgba32fData();
    const float* actualEffectPixels = enabledOutput.image().rgba32fData();
    const float* expectedEffectPixels = expectedFullEffect.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(bypassedPixels, nullptr);
    ASSERT_NE(actualEffectPixels, nullptr);
    ASSERT_NE(expectedEffectPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(bypassedPixels[channel], sourcePixels[channel])
            << "bypassed channel=" << channel;
        EXPECT_FLOAT_EQ(actualEffectPixels[channel], expectedEffectPixels[channel])
            << "enabled channel=" << channel;
    }
}

TEST(LofiEffectContractTest, AnimatedRegionEnabledTogglesWithoutCoordinateTracks)
{
    ArtifactOldTVEffect effect;
    const auto enabled =
        effect.editableProperty(QStringLiteral("Effect Region Enabled"));
    ASSERT_TRUE(enabled);
    enabled->addKeyFrame(RationalTime(0, 30), true);
    enabled->addKeyFrame(RationalTime(30, 30), false);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_TRUE(effect.hasEffectRegion());

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FALSE(effect.hasEffectRegion());
}

TEST(LofiEffectContractTest, AnimatedRegionCoordinatesMoveThePixelComposite)
{
    constexpr int width = 8;
    constexpr int height = 8;
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto enabled =
        effect.editableProperty(QStringLiteral("Effect Region Enabled"));
    const auto regionX = effect.editableProperty(QStringLiteral("Effect Region X"));
    const auto regionY = effect.editableProperty(QStringLiteral("Effect Region Y"));
    const auto regionWidth =
        effect.editableProperty(QStringLiteral("Effect Region Width"));
    const auto regionHeight =
        effect.editableProperty(QStringLiteral("Effect Region Height"));
    ASSERT_TRUE(enabled);
    ASSERT_TRUE(regionX);
    ASSERT_TRUE(regionY);
    ASSERT_TRUE(regionWidth);
    ASSERT_TRUE(regionHeight);
    enabled->addKeyFrame(RationalTime(0, 30), true);
    enabled->addKeyFrame(RationalTime(30, 30), true);
    regionX->addKeyFrame(RationalTime(0, 30), 2.0);
    regionX->addKeyFrame(RationalTime(30, 30), 3.0);
    regionY->addKeyFrame(RationalTime(0, 30), 1.0);
    regionY->addKeyFrame(RationalTime(30, 30), 2.0);
    regionWidth->addKeyFrame(RationalTime(0, 30), 3.0);
    regionWidth->addKeyFrame(RationalTime(30, 30), 4.0);
    regionHeight->addKeyFrame(RationalTime(0, 30), 2.0);
    regionHeight->addKeyFrame(RationalTime(30, 30), 3.0);

    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache fullEffectOutput;
    fullEffect.applyCPUOnly(source, fullEffectOutput);
    const float* sourcePixels = source.image().rgba32fData();
    const float* fullPixels = fullEffectOutput.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    ASSERT_NE(fullPixels, nullptr);

    const auto expectRegionOutput = [&](std::int64_t frame,
                                        const QRectF& expectedRegion) {
        EffectContext context;
        context.frameRate = 30.0;
        context.compositionFrame = frame;
        effect.setContext(context);
        ASSERT_TRUE(effect.hasEffectRegion());
        EXPECT_EQ(effect.effectRegion(), expectedRegion);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        const float* actual = output.image().rgba32fData();
        ASSERT_NE(actual, nullptr);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool inside = expectedRegion.contains(
                    QPointF(static_cast<qreal>(x) + 0.5,
                            static_cast<qreal>(y) + 0.5));
                const std::size_t offset =
                    static_cast<std::size_t>((y * width + x) * 4);
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    const float expected = inside
                        ? fullPixels[offset + channel]
                        : sourcePixels[offset + channel];
                    EXPECT_FLOAT_EQ(actual[offset + channel], expected)
                        << "frame=" << frame << " x=" << x << " y=" << y
                        << " channel=" << channel;
                }
            }
        }
    };

    expectRegionOutput(0, QRectF(2.0, 1.0, 3.0, 2.0));
    expectRegionOutput(15, QRectF(2.5, 1.5, 3.5, 2.5));
    expectRegionOutput(30, QRectF(3.0, 2.0, 4.0, 3.0));
}

TEST(LofiEffectContractTest, AllowOverscanDoesNotExpandOldTvOutput)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache standardOutput;
    ImageF32x4RGBAWithCache overscanOutput;
    ArtifactOldTVEffect standardEffect;
    standardEffect.setComputeMode(ComputeMode::CPU);
    standardEffect.applyCPUOnly(source, standardOutput);

    ArtifactOldTVEffect overscanEffect;
    overscanEffect.setComputeMode(ComputeMode::CPU);
    overscanEffect.setAllowOverscan(true);
    ASSERT_TRUE(overscanEffect.allowOverscan());
    overscanEffect.applyCPUOnly(source, overscanOutput);

    EXPECT_EQ(overscanOutput.image().width(), source.image().width());
    EXPECT_EQ(overscanOutput.image().height(), source.image().height());
    EXPECT_EQ(overscanOutput.image().width(), standardOutput.image().width());
    EXPECT_EQ(overscanOutput.image().height(), standardOutput.image().height());
    const float* standardPixels = standardOutput.image().rgba32fData();
    const float* overscanPixels = overscanOutput.image().rgba32fData();
    ASSERT_NE(standardPixels, nullptr);
    ASSERT_NE(overscanPixels, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(overscanPixels[channel], standardPixels[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, AnimatedRegionDisableOverridesAnimatedCoordinates)
{
    const auto source = makePatternSurface();
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    const auto regionEnabled =
        effect.editableProperty(QStringLiteral("Effect Region Enabled"));
    const auto regionX = effect.editableProperty(QStringLiteral("Effect Region X"));
    const auto regionY = effect.editableProperty(QStringLiteral("Effect Region Y"));
    const auto regionWidth =
        effect.editableProperty(QStringLiteral("Effect Region Width"));
    const auto regionHeight =
        effect.editableProperty(QStringLiteral("Effect Region Height"));
    ASSERT_TRUE(regionEnabled);
    ASSERT_TRUE(regionX);
    ASSERT_TRUE(regionY);
    ASSERT_TRUE(regionWidth);
    ASSERT_TRUE(regionHeight);

    regionEnabled->addKeyFrame(RationalTime(0, 30), true);
    regionEnabled->addKeyFrame(RationalTime(30, 30), false);
    regionX->addKeyFrame(RationalTime(0, 30), 2.0);
    regionX->addKeyFrame(RationalTime(30, 30), 3.0);
    regionY->addKeyFrame(RationalTime(0, 30), 1.0);
    regionY->addKeyFrame(RationalTime(30, 30), 2.0);
    regionWidth->addKeyFrame(RationalTime(0, 30), 3.0);
    regionWidth->addKeyFrame(RationalTime(30, 30), 4.0);
    regionHeight->addKeyFrame(RationalTime(0, 30), 2.0);
    regionHeight->addKeyFrame(RationalTime(30, 30), 3.0);

    EffectContext context;
    context.frameRate = 30.0;
    context.compositionFrame = 0;
    effect.setContext(context);
    EXPECT_TRUE(effect.hasEffectRegion());
    EXPECT_EQ(effect.effectRegion(), QRectF(2.0, 1.0, 3.0, 2.0));

    context.compositionFrame = 30;
    effect.setContext(context);
    EXPECT_FALSE(effect.hasEffectRegion())
        << "animated region coordinates must not override Region Enabled=false";

    ImageF32x4RGBAWithCache disabledRegionOutput;
    effect.applyCPUOnly(source, disabledRegionOutput);
    ArtifactOldTVEffect fullEffect;
    fullEffect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache fullEffectOutput;
    fullEffect.applyCPUOnly(source, fullEffectOutput);
    const float* actual = disabledRegionOutput.image().rgba32fData();
    const float* expected = fullEffectOutput.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvEmptyInputClearsPreviouslyPopulatedOutput)
{
    const ImageF32x4RGBAWithCache source;
    ImageF32x4RGBAWithCache output = makePatternSurface();
    ASSERT_FALSE(output.image().isEmpty());
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.apply(source, output);

    EXPECT_TRUE(output.image().isEmpty());
    EXPECT_EQ(output.image().width(), 0);
    EXPECT_EQ(output.image().height(), 0);
}

TEST(LofiEffectContractTest, DisabledOldTvIsAnExactPassthrough)
{
    constexpr int width = 4;
    constexpr int height = 3;
    constexpr std::array<float, width * height * 4> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 1.0f, 0.0f, 1.2f,
        -0.1f, 0.25f, 0.75f, -0.3f,
        0.3f, 0.2f, 0.1f, 0.0f,
        0.7f, 0.8f, 0.9f, 0.6f,
        0.15f, 0.35f, 0.55f, 0.75f,
        0.25f, 0.45f, 0.65f, 0.85f,
        0.95f, 0.05f, 0.4f, 0.2f,
        0.33f, 0.66f, 0.99f, 0.12f,
        0.42f, 0.24f, 0.18f, 0.36f,
        0.8f, 0.4f, 0.2f, 0.1f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setEnabled(false);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel])
            << "output channel=" << channel;
        EXPECT_FLOAT_EQ(sourcePixels[channel], pixels[channel])
            << "source channel=" << channel;
    }
}

TEST(LofiEffectContractTest, OldTvHandlesSinglePixelWideImages)
{
    constexpr int width = 1;
    constexpr int height = 5;
    constexpr std::array<float, width * height * 4> pixels{
        0.2f, 0.4f, 0.6f, 0.1f,
        0.3f, 0.5f, 0.7f, 0.3f,
        0.4f, 0.6f, 0.8f, 0.5f,
        0.5f, 0.7f, 0.9f, 0.7f,
        0.6f, 0.8f, 1.0f, 0.9f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(height);
         ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[pixel * 4 + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_GE(result[pixel * 4 + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[pixel * 4 + channel], 1.0f)
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}

TEST(LofiEffectContractTest, OldTvHandlesSinglePixelHighImagesDeterministically)
{
    constexpr int width = 5;
    constexpr int height = 1;
    constexpr std::array<float, width * height * 4> pixels{
        0.2f, 0.4f, 0.6f, 0.1f,
        0.3f, 0.5f, 0.7f, 0.3f,
        0.4f, 0.6f, 0.8f, 0.5f,
        0.5f, 0.7f, 0.9f, 0.7f,
        0.6f, 0.8f, 1.0f, 0.9f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;

    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, repeated);

    ASSERT_EQ(first.image().width(), width);
    ASSERT_EQ(first.image().height(), height);
    const float* result = first.image().rgba32fData();
    const float* repeatedResult = repeated.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(repeatedResult, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width); ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(result[pixel * 4 + channel]))
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_GE(result[pixel * 4 + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[pixel * 4 + channel], 1.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_FLOAT_EQ(result[pixel * 4 + channel],
                            repeatedResult[pixel * 4 + channel])
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3]);
    }
}

TEST(LofiEffectContractTest, OldTvPreservesAlphaOnSinglePixelImages)
{
    constexpr std::array<float, 4> pixels{0.35f, 0.65f, 0.9f, 0.42f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_TRUE(std::isfinite(result[channel])) << "channel=" << channel;
        EXPECT_GE(result[channel], 0.0f) << "channel=" << channel;
        EXPECT_LE(result[channel], 1.0f) << "channel=" << channel;
    }
    EXPECT_FLOAT_EQ(result[3], pixels[3]);
}

} // namespace

TEST(LofiEffectContractTest, OldTvKeepsAlphaAndSourceAndAppliesFourRowScanline)
{
    constexpr int width = 8;
    constexpr int height = 8;
    constexpr std::size_t channelCount = width * height * 4;
    const auto source = makePatternSurface();
    std::array<float, channelCount> original{};
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(sourcePixels, nullptr);
    std::copy_n(sourcePixels, channelCount, original.begin());

    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, first);

    ArtifactOldTVEffect repeatedEffect;
    repeatedEffect.setComputeMode(ComputeMode::CPU);
    repeatedEffect.apply(source, repeated);

    ASSERT_EQ(first.image().width(), width);
    ASSERT_EQ(first.image().height(), height);
    const float* output = first.image().rgba32fData();
    const float* repeatedOutput = repeated.image().rgba32fData();
    const float* unchangedSource = source.image().rgba32fData();
    ASSERT_NE(output, nullptr);
    ASSERT_NE(repeatedOutput, nullptr);
    ASSERT_NE(unchangedSource, nullptr);
    constexpr std::array<float, 3> baseBgra = {0.6f, 0.5f, 0.4f};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const float scanline = y % 4 == 0 ? 0.7f : 1.0f;
            const float noise = output[offset] - baseBgra[0] * scanline;
            EXPECT_GE(noise, -0.005f) << "x=" << x << " y=" << y;
            EXPECT_LE(noise, 0.005f) << "x=" << x << " y=" << y;
            for (int channel = 0; channel < 3; ++channel) {
                EXPECT_NEAR(output[offset + channel],
                            baseBgra[channel] * scanline + noise, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_FLOAT_EQ(output[offset + channel], repeatedOutput[offset + channel]);
                EXPECT_FLOAT_EQ(unchangedSource[offset + channel], original[offset + channel]);
            }
            EXPECT_FLOAT_EQ(output[offset + 3], original[offset + 3]);
            EXPECT_FLOAT_EQ(repeatedOutput[offset + 3], original[offset + 3]);
            EXPECT_FLOAT_EQ(unchangedSource[offset + 3], original[offset + 3]);
        }
    }
}

TEST(LofiEffectContractTest,
     OldTvAppliesDeterministicClampedHorizontalJitterPerScanline)
{
    constexpr int width = 64;
    constexpr int height = 64;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float ramp = 0.1f + static_cast<float>(x) * 0.005f;
            pixels[offset + 0] = ramp;
            pixels[offset + 1] = ramp * 2.0f;
            pixels[offset + 2] = ramp * 3.0f;
            pixels[offset + 3] = 0.15f + static_cast<float>(x) * 0.01f +
                                 static_cast<float>(y) * 0.001f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache first;
    ImageF32x4RGBAWithCache repeated;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, first);
    effect.applyCPUOnly(source, repeated);

    const float* result = first.image().rgba32fData();
    const float* repeatedResult = repeated.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    ASSERT_NE(repeatedResult, nullptr);
    bool observedJitter = false;
    for (int y = 0; y < height; ++y) {
        const float scanline = y % 4 == 0 ? 0.7f : 1.0f;
        constexpr int referenceX = 6;
        const std::size_t referenceOffset =
            static_cast<std::size_t>((y * width + referenceX) * 4);
        const float referenceDelta =
            result[referenceOffset + 1] - result[referenceOffset];
        const int referenceSourceX = static_cast<int>(std::lround(
            referenceDelta / (0.005f * scanline)));
        const int rowShift = referenceSourceX - referenceX;
        ASSERT_LE(std::abs(rowShift), 4) << "y=" << y;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            const float redGreenDelta = result[offset + 1] - result[offset];
            const int sourceX = static_cast<int>(std::lround(
                redGreenDelta / (0.005f * scanline)));
            ASSERT_GE(sourceX, 0) << "x=" << x << " y=" << y;
            ASSERT_LT(sourceX, width) << "x=" << x << " y=" << y;
            const int shift = sourceX - x;
            EXPECT_EQ(shift, rowShift) << "x=" << x << " y=" << y;
            EXPECT_EQ(sourceX, std::clamp(x + rowShift, 0, width - 1))
                << "x=" << x << " y=" << y;
            EXPECT_LE(std::abs(rowShift), 5) << "y=" << y;
            const std::size_t sourceOffset =
                static_cast<std::size_t>((y * width + sourceX) * 4);
            EXPECT_FLOAT_EQ(result[offset + 3], pixels[sourceOffset + 3])
                << "x=" << x << " y=" << y << " sourceX=" << sourceX;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                EXPECT_FLOAT_EQ(result[offset + channel],
                                repeatedResult[offset + channel])
                    << "x=" << x << " y=" << y
                    << " channel=" << channel;
            }
        }
        observedJitter = observedJitter || rowShift != 0;
    }
    EXPECT_TRUE(observedJitter);
}

TEST(LofiEffectContractTest, OldTvSupportsInPlaceProcessingWithoutChangingAlpha)
{
    constexpr int width = 9;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / width;
            pixels[offset + 1] = static_cast<float>(y + 1) / height;
            pixels[offset + 2] = 0.25f;
            pixels[offset + 3] = static_cast<float>(x + y + 1) /
                                 static_cast<float>(width + height);
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    ImageF32x4RGBAWithCache inPlace(image);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(inPlace, inPlace);

    ASSERT_EQ(inPlace.image().width(), width);
    ASSERT_EQ(inPlace.image().height(), height);
    const float* output = inPlace.image().rgba32fData();
    ASSERT_NE(output, nullptr);
    bool colorChanged = false;
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width * height);
         ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_TRUE(std::isfinite(output[pixel * 4 + channel]));
            colorChanged = colorChanged ||
                std::abs(output[pixel * 4 + channel] - pixels[pixel * 4 + channel]) >
                    1.0e-5f;
        }
        EXPECT_FLOAT_EQ(output[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
    EXPECT_TRUE(colorChanged);
}

TEST(LofiEffectContractTest, OldTvClampsRgbAndPreservesOutOfRangeAlpha)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<float, width * height * 4> pixels{
        -0.5f, 1.5f, 0.25f, -0.2f,
        2.0f, -1.0f, 0.5f, 1.3f,
        -3.0f, 4.0f, 0.0f, 0.4f,
        1.2f, -0.3f, 3.0f, 0.8f,
        0.1f, 0.2f, 0.3f, -1.0f,
        0.8f, 0.7f, 0.6f, 2.0f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;

    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.apply(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_GE(result[pixel * 4 + channel], 0.0f)
                << "pixel=" << pixel << " channel=" << channel;
            EXPECT_LE(result[pixel * 4 + channel], 1.0f)
                << "pixel=" << pixel << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(result[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
    }
}

TEST(LofiEffectContractTest, OldTvAddsSharedNoiseWithoutChangingRgbChannelDifferences)
{
    constexpr int width = 7;
    constexpr int height = 5;
    constexpr std::array<float, 3> baseRgb{0.2f, 0.4f, 0.6f};
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = baseRgb[0];
            pixels[offset + 1] = baseRgb[1];
            pixels[offset + 2] = baseRgb[2];
            pixels[offset + 3] = 0.75f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);

    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (int y = 0; y < height; ++y) {
        const float scanline = y % 4 == 0 ? 0.7f : 1.0f;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            EXPECT_NEAR(result[offset + 1] - result[offset + 0],
                        (baseRgb[1] - baseRgb[0]) * scanline, 1.0e-6f)
                << "x=" << x << " y=" << y << " R-G";
            EXPECT_NEAR(result[offset + 2] - result[offset + 1],
                        (baseRgb[2] - baseRgb[1]) * scanline, 1.0e-6f)
                << "x=" << x << " y=" << y << " G-B";
            EXPECT_FLOAT_EQ(result[offset + 3], 0.75f)
                << "x=" << x << " y=" << y;
        }
    }
}

TEST(LofiEffectContractTest, OldTvZeroMixIsAnExactSourcePassthrough)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 2) / 7.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 9.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 12.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setMix(0.0f);
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

TEST(LofiEffectContractTest, OldTvHalfMixInterpolatesSourceAndEffectPixels)
{
    constexpr int width = 7;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 10.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 8.0f;
            pixels[offset + 2] = static_cast<float>(x + 2 * y + 1) / 16.0f;
            pixels[offset + 3] = static_cast<float>(x + y + 1) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache halfMix;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setMix(0.5f);
    effect.applyCPUOnly(source, halfMix);

    ASSERT_EQ(fullEffect.image().width(), width);
    ASSERT_EQ(fullEffect.image().height(), height);
    ASSERT_EQ(halfMix.image().width(), width);
    ASSERT_EQ(halfMix.image().height(), height);
    const float* fullEffectPixels = fullEffect.image().rgba32fData();
    const float* halfMixPixels = halfMix.image().rgba32fData();
    ASSERT_NE(fullEffectPixels, nullptr);
    ASSERT_NE(halfMixPixels, nullptr);
    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::size_t offset = pixel * 4 + channel;
            const float expected =
                pixels[offset] * 0.5f + fullEffectPixels[offset] * 0.5f;
            EXPECT_NEAR(halfMixPixels[offset], expected, 1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
}

TEST(LofiEffectContractTest, OldTvMixClampsFiniteValuesAndResetsNonFiniteValues)
{
    constexpr int width = 5;
    constexpr int height = 3;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 2) / 7.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 9.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 12.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ArtifactOldTVEffect fullEffectInstance;
    fullEffectInstance.setComputeMode(ComputeMode::CPU);
    fullEffectInstance.applyCPUOnly(source, fullEffect);
    const float* fullPixels = fullEffect.image().rgba32fData();
    ASSERT_NE(fullPixels, nullptr);

    struct MixCase {
        float requested;
        float effective;
    };
    const std::array<MixCase, 8> cases{{
        {-2.0f, 0.0f},
        {0.0f, 0.0f},
        {0.35f, 0.35f},
        {1.0f, 1.0f},
        {3.0f, 1.0f},
        {std::numeric_limits<float>::quiet_NaN(), 1.0f},
        {std::numeric_limits<float>::infinity(), 1.0f},
        {-std::numeric_limits<float>::infinity(), 1.0f},
    }};

    for (const auto& mixCase : cases) {
        SCOPED_TRACE(mixCase.requested);
        ArtifactOldTVEffect effect;
        effect.setComputeMode(ComputeMode::CPU);
        effect.setMix(mixCase.requested);
        EXPECT_FLOAT_EQ(effect.mix(), mixCase.effective);
        ImageF32x4RGBAWithCache output;
        effect.applyCPUOnly(source, output);
        const float* result = output.image().rgba32fData();
        ASSERT_NE(result, nullptr);
        for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
            const float expected = pixels[channel] * (1.0f - mixCase.effective) +
                                   fullPixels[channel] * mixCase.effective;
            EXPECT_NEAR(result[channel], expected, 1.0e-6f)
                << "channel=" << channel;
        }
    }
}

TEST(LofiEffectContractTest, OldTvEffectRegionCompositesOnlyInsidePixelBounds)
{
    constexpr int width = 6;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 8.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 6.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 12.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache regionalEffect;
    ArtifactOldTVEffect fullEffectInstance;
    fullEffectInstance.setComputeMode(ComputeMode::CPU);
    fullEffectInstance.applyCPUOnly(source, fullEffect);
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setMix(0.5f);
    effect.setEffectRegion(QRectF(2.0, 1.0, 2.0, 2.0));
    effect.applyCPUOnly(source, regionalEffect);

    ASSERT_EQ(fullEffect.image().width(), width);
    ASSERT_EQ(fullEffect.image().height(), height);
    ASSERT_EQ(regionalEffect.image().width(), width);
    ASSERT_EQ(regionalEffect.image().height(), height);
    const float* fullEffectPixels = fullEffect.image().rgba32fData();
    const float* regionalPixels = regionalEffect.image().rgba32fData();
    const float* unchangedSource = source.image().rgba32fData();
    ASSERT_NE(fullEffectPixels, nullptr);
    ASSERT_NE(regionalPixels, nullptr);
    ASSERT_NE(unchangedSource, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool inside = x >= 2 && x < 4 && y >= 1 && y < 3;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = inside
                    ? pixels[offset + channel] * 0.5f +
                          fullEffectPixels[offset + channel] * 0.5f
                    : pixels[offset + channel];
                EXPECT_NEAR(regionalPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
                EXPECT_FLOAT_EQ(unchangedSource[offset + channel], pixels[offset + channel])
                    << "source x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
}

TEST(LofiEffectContractTest, OldTvEffectRegionClipsToImageBounds)
{
    constexpr int width = 5;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<float>(x + 1) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 6.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 10.0f;
            pixels[offset + 3] = 0.65f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache clippedEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setEffectRegion(QRectF(-2.0, -1.0, 4.0, 3.0));
    effect.applyCPUOnly(source, clippedEffect);

    const float* fullPixels = fullEffect.image().rgba32fData();
    const float* clippedPixels = clippedEffect.image().rgba32fData();
    ASSERT_NE(fullPixels, nullptr);
    ASSERT_NE(clippedPixels, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool insideClippedRegion = x < 2 && y < 2;
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = insideClippedRegion
                    ? fullPixels[offset + channel]
                    : pixels[offset + channel];
                EXPECT_NEAR(clippedPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
}

TEST(LofiEffectContractTest, OldTvRegionOutsideImageLeavesSourceUntouched)
{
    constexpr int width = 5;
    constexpr int height = 4;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = static_cast<float>(x + 1) / 7.0f;
            pixels[offset + 1] = static_cast<float>(y + 1) / 6.0f;
            pixels[offset + 2] = static_cast<float>(x + y + 1) / 10.0f;
            pixels[offset + 3] = static_cast<float>(x + 2 * y + 1) / 16.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ImageF32x4RGBAWithCache source(image);
    ImageF32x4RGBAWithCache output;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setEffectRegion(QRectF(width + 2.0, height + 1.0, 3.0, 2.0));
    ASSERT_TRUE(effect.hasEffectRegion());

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

TEST(LofiEffectContractTest, OldTvEffectRegionUsesPixelCentersForFractionalBounds)
{
    constexpr int width = 8;
    constexpr int height = 8;
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache fractionalEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);
    effect.setEffectRegion(QRectF(2.25, 1.25, 1.0, 1.5));
    effect.applyCPUOnly(source, fractionalEffect);

    const float* original = source.image().rgba32fData();
    const float* fullPixels = fullEffect.image().rgba32fData();
    const float* fractionalPixels = fractionalEffect.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(fullPixels, nullptr);
    ASSERT_NE(fractionalPixels, nullptr);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Pixel centers are (x + 0.5, y + 0.5). The fractional region
            // contains only x=2 and y=1,2.
            const bool inside = x == 2 && (y == 1 || y == 2);
            const std::size_t offset =
                static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const float expected = inside
                    ? fullPixels[offset + channel]
                    : original[offset + channel];
                EXPECT_NEAR(fractionalPixels[offset + channel], expected, 1.0e-6f)
                    << "x=" << x << " y=" << y << " channel=" << channel;
            }
        }
    }
}

TEST(LofiEffectContractTest, InvalidEffectRegionClearsPreviousRegion)
{
    const auto source = makePatternSurface();
    ImageF32x4RGBAWithCache fullEffect;
    ImageF32x4RGBAWithCache restoredEffect;
    ArtifactOldTVEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.applyCPUOnly(source, fullEffect);

    effect.setEffectRegion(QRectF(2.0, 2.0, 2.0, 2.0));
    ASSERT_TRUE(effect.hasEffectRegion());
    effect.setEffectRegion(QRectF(2.0, 2.0, 0.0, 2.0));
    EXPECT_FALSE(effect.hasEffectRegion());
    effect.applyCPUOnly(source, restoredEffect);

    const float* expected = fullEffect.image().rgba32fData();
    const float* actual = restoredEffect.image().rgba32fData();
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(actual, nullptr);
    ASSERT_EQ(restoredEffect.image().width(), fullEffect.image().width());
    ASSERT_EQ(restoredEffect.image().height(), fullEffect.image().height());
    constexpr std::size_t channelCount = 8 * 8 * 4;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel])
            << "channel=" << channel;
    }
}
