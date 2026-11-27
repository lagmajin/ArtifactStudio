#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Creative;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

// The product-side fog effect (builtin.atmospheric_depth) is a depth-driven
// color mix, not the volumetric AtmosphereFogRenderer in ArtifactCore. This
// contract locks the equations implemented in
// Artifact/src/Effect/ArtifactCreativeEffects.cpp:
//
//   normalized = clamp((depth - nearDepth) / max(farDepth - nearDepth, 1e-5), 0, 1)
//   normalized = normalized ^ curve
//   fog       = (1 - exp(-density * normalized)) * mix
//   rgb       = lerp(source, fogColor + highlight * glow, fog)
//   alpha     = source alpha
//
// The expected values are recomputed here from the same equations instead of
// being copied from captured output, so the test fails when the runtime
// equations change.

struct Pixel {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct FogSettings {
    float nearDepth = 0.15f;
    float farDepth = 0.9f;
    float density = 1.1f;
    float curve = 1.5f;
    float mix = 1.0f;
    float red = 0.62f;
    float green = 0.72f;
    float blue = 0.82f;
};

ImageF32x4RGBAWithCache makeImage(int width, int height, const std::vector<Pixel>& pixels)
{
    std::vector<float> data;
    data.reserve(pixels.size() * 4u);
    for (const Pixel& pixel : pixels) {
        data.push_back(pixel.r);
        data.push_back(pixel.g);
        data.push_back(pixel.b);
        data.push_back(pixel.a);
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(data.data(), width, height);
    return ImageF32x4RGBAWithCache(image);
}

ImageF32x4RGBAWithCache makeGrayDepthImage(int width, int height, const std::vector<float>& depths)
{
    std::vector<Pixel> pixels;
    pixels.reserve(depths.size());
    for (float depth : depths) {
        pixels.push_back(Pixel{depth, depth, depth, 1.0f});
    }
    return makeImage(width, height, pixels);
}

float fogAmount(const FogSettings& settings, float depth)
{
    const float range = std::max(settings.farDepth - settings.nearDepth, 1.0e-5f);
    float normalized = std::clamp((depth - settings.nearDepth) / range, 0.0f, 1.0f);
    normalized = std::pow(normalized, settings.curve);
    return (1.0f - std::exp(-settings.density * normalized)) * settings.mix;
}

float rec709Luma(const Pixel& pixel)
{
    return pixel.r * 0.2126f + pixel.g * 0.7152f + pixel.b * 0.0722f;
}

void applySettings(ArtifactAtmosphericDepthEffect& effect, const FogSettings& settings)
{
    effect.setPropertyValue(UniString("nearDepth"), QVariant(settings.nearDepth));
    effect.setPropertyValue(UniString("farDepth"), QVariant(settings.farDepth));
    effect.setPropertyValue(UniString("density"), QVariant(settings.density));
    effect.setPropertyValue(UniString("curve"), QVariant(settings.curve));
    effect.setPropertyValue(UniString("depthSoftness"), QVariant(0.0));
    effect.setPropertyValue(UniString("glow"), QVariant(0.0));
    effect.setPropertyValue(UniString("mix"), QVariant(settings.mix));
    effect.setPropertyValue(UniString("red"), QVariant(settings.red));
    effect.setPropertyValue(UniString("green"), QVariant(settings.green));
    effect.setPropertyValue(UniString("blue"), QVariant(settings.blue));
}

class NamedDepthSampler final : public IEffectFrameSampler {
public:
    NamedDepthSampler(ImageF32x4RGBAWithCache depth, bool provide = true)
        : depth_(std::move(depth)), provide_(provide)
    {
    }

    bool sampleCurrentLayerFrame(std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(const QString& inputId, std::int64_t compositionFrame,
                           ImageF32x4RGBAWithCache& out) override
    {
        requestedInput = inputId;
        requestedFrame = compositionFrame;
        if (!provide_) {
            return false;
        }
        out = depth_;
        return true;
    }

    QString requestedInput;
    std::int64_t requestedFrame = -1;

private:
    ImageF32x4RGBAWithCache depth_;
    bool provide_ = true;
};

} // namespace

TEST(AtmosphericDepthEffectContractTest, NamedDepthInputDrivesDepthBasedMixAndPreservesAlpha)
{
    const std::vector<Pixel> sourcePixels{
        Pixel{0.0f, 0.0f, 0.0f, 1.0f},
        Pixel{0.8f, 0.2f, 0.1f, 0.5f},
        Pixel{0.1f, 0.5f, 0.9f, 0.25f},
        Pixel{1.0f, 1.0f, 1.0f, 0.75f},
    };
    const std::vector<float> depths{0.0f, 0.3f, 0.6f, 1.0f};

    const auto source = makeImage(4, 1, sourcePixels);
    const auto depthImage = makeGrayDepthImage(4, 1, depths);

    NamedDepthSampler sampler(depthImage);
    EffectContext context;
    context.compositionFrame = 42;
    context.sampler = &sampler;

    const FogSettings settings;
    ArtifactAtmosphericDepthEffect effect;
    applySettings(effect, settings);
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedInput, QStringLiteral("depth"));
    EXPECT_EQ(sampler.requestedFrame, 42);
    ASSERT_EQ(output.image().width(), 4);
    ASSERT_EQ(output.image().height(), 1);

    for (int x = 0; x < 4; ++x) {
        const float fog = fogAmount(settings, depths[static_cast<std::size_t>(x)]);
        const Pixel& before = sourcePixels[static_cast<std::size_t>(x)];
        const auto after = output.image().getPixel(x, 0);

        EXPECT_NEAR(after.r(), before.r + (settings.red - before.r) * fog, 1.0e-4f)
            << "x=" << x;
        EXPECT_NEAR(after.g(), before.g + (settings.green - before.g) * fog, 1.0e-4f)
            << "x=" << x;
        EXPECT_NEAR(after.b(), before.b + (settings.blue - before.b) * fog, 1.0e-4f)
            << "x=" << x;
        EXPECT_FLOAT_EQ(after.a(), before.a) << "x=" << x;
    }
}

TEST(AtmosphericDepthEffectContractTest, DepthInputUsesLogicalRedChannel)
{
    // The runtime documents that a colour depth input uses logical red.
    const std::vector<Pixel> sourcePixels{Pixel{0.2f, 0.4f, 0.6f, 1.0f}};
    const auto source = makeImage(1, 1, sourcePixels);

    ImageF32x4_RGBA depthImage;
    const std::vector<float> depthData{0.25f, 0.75f, 0.5f, 1.0f};
    depthImage.setFromRGBA32F(depthData.data(), 1, 1);
    const ImageF32x4RGBAWithCache depth(depthImage);

    NamedDepthSampler sampler(depth);
    EffectContext context;
    context.compositionFrame = 3;
    context.sampler = &sampler;

    const FogSettings settings;
    ArtifactAtmosphericDepthEffect effect;
    applySettings(effect, settings);
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const auto after = output.image().getPixel(0, 0);
    const Pixel& before = sourcePixels.front();

    const float redFog = fogAmount(settings, 0.25f);
    EXPECT_NEAR(after.r(), before.r + (settings.red - before.r) * redFog, 1.0e-4f);

    const float greenFog = fogAmount(settings, 0.75f);
    EXPECT_GT(std::abs(greenFog - redFog), 0.1f)
        << "The fixture must distinguish the red channel from the green channel.";
}

TEST(AtmosphericDepthEffectContractTest, LumaFallbackIsUsedWhenNamedDepthIsUnavailable)
{
    const std::vector<Pixel> sourcePixels{
        Pixel{0.2f, 0.6f, 0.9f, 1.0f},
        Pixel{0.9f, 0.1f, 0.1f, 0.5f},
    };
    const auto source = makeImage(2, 1, sourcePixels);

    NamedDepthSampler sampler(makeGrayDepthImage(2, 1, {0.0f, 0.0f}), false);
    EffectContext context;
    context.compositionFrame = 11;
    context.sampler = &sampler;

    const FogSettings settings;
    ArtifactAtmosphericDepthEffect effect;
    applySettings(effect, settings);
    effect.setPropertyValue(UniString("useLumaFallback"), QVariant(true));
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    EXPECT_EQ(sampler.requestedInput, QStringLiteral("depth"));

    for (int x = 0; x < 2; ++x) {
        const Pixel& before = sourcePixels[static_cast<std::size_t>(x)];
        const float fog = fogAmount(settings, rec709Luma(before));
        const auto after = output.image().getPixel(x, 0);

        EXPECT_NEAR(after.r(), before.r + (settings.red - before.r) * fog, 1.0e-4f)
            << "x=" << x;
        EXPECT_NEAR(after.g(), before.g + (settings.green - before.g) * fog, 1.0e-4f)
            << "x=" << x;
        EXPECT_NEAR(after.b(), before.b + (settings.blue - before.b) * fog, 1.0e-4f)
            << "x=" << x;
        EXPECT_FLOAT_EQ(after.a(), before.a) << "x=" << x;
    }
}

TEST(AtmosphericDepthEffectContractTest, MissingDepthSourceLeavesFrameUnchanged)
{
    const std::vector<Pixel> sourcePixels{
        Pixel{0.3f, 0.4f, 0.5f, 0.8f},
        Pixel{0.9f, 0.1f, 0.2f, 0.4f},
    };
    const auto source = makeImage(2, 1, sourcePixels);

    EffectContext context;
    context.compositionFrame = 5;
    context.sampler = nullptr;

    const FogSettings settings;
    ArtifactAtmosphericDepthEffect effect;
    applySettings(effect, settings);
    effect.setPropertyValue(UniString("useLumaFallback"), QVariant(false));
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 2);
    ASSERT_EQ(output.image().height(), 1);
    for (int x = 0; x < 2; ++x) {
        const auto before = source.image().getPixel(x, 0);
        const auto after = output.image().getPixel(x, 0);
        EXPECT_FLOAT_EQ(after.r(), before.r());
        EXPECT_FLOAT_EQ(after.g(), before.g());
        EXPECT_FLOAT_EQ(after.b(), before.b());
        EXPECT_FLOAT_EQ(after.a(), before.a());
    }
}

TEST(AtmosphericDepthEffectContractTest, ZeroMixLeavesFrameUnchanged)
{
    const std::vector<Pixel> sourcePixels{Pixel{0.4f, 0.5f, 0.6f, 0.9f}};
    const auto source = makeImage(1, 1, sourcePixels);
    const auto depthImage = makeGrayDepthImage(1, 1, {0.95f});

    NamedDepthSampler sampler(depthImage);
    EffectContext context;
    context.compositionFrame = 7;
    context.sampler = &sampler;

    FogSettings settings;
    settings.mix = 0.0f;
    ArtifactAtmosphericDepthEffect effect;
    applySettings(effect, settings);
    effect.setContext(context);

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(source, output);

    const auto before = source.image().getPixel(0, 0);
    const auto after = output.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(after.r(), before.r());
    EXPECT_FLOAT_EQ(after.g(), before.g());
    EXPECT_FLOAT_EQ(after.b(), before.b());
    EXPECT_FLOAT_EQ(after.a(), before.a());
}

TEST(AtmosphericDepthEffectContractTest, InvertedDepthMirrorsDepthResponse)
{
    const std::vector<Pixel> sourcePixels{Pixel{0.25f, 0.35f, 0.45f, 1.0f}};
    const auto source = makeImage(1, 1, sourcePixels);

    const FogSettings settings;
    EffectContext context;
    context.sampler = nullptr;

    NamedDepthSampler invertedSampler(makeGrayDepthImage(1, 1, {0.4f}));
    ArtifactAtmosphericDepthEffect invertedEffect;
    applySettings(invertedEffect, settings);
    invertedEffect.setPropertyValue(UniString("invertDepth"), QVariant(true));
    context.sampler = &invertedSampler;
    invertedEffect.setContext(context);

    NamedDepthSampler directSampler(makeGrayDepthImage(1, 1, {0.6f}));
    ArtifactAtmosphericDepthEffect directEffect;
    applySettings(directEffect, settings);
    context.sampler = &directSampler;
    directEffect.setContext(context);

    ImageF32x4RGBAWithCache invertedOutput;
    ImageF32x4RGBAWithCache directOutput;
    invertedEffect.applyCPUOnly(source, invertedOutput);
    directEffect.applyCPUOnly(source, directOutput);

    const auto inverted = invertedOutput.image().getPixel(0, 0);
    const auto direct = directOutput.image().getPixel(0, 0);
    EXPECT_NEAR(inverted.r(), direct.r(), 1.0e-4f);
    EXPECT_NEAR(inverted.g(), direct.g(), 1.0e-4f);
    EXPECT_NEAR(inverted.b(), direct.b(), 1.0e-4f);
    EXPECT_FLOAT_EQ(inverted.a(), direct.a());
}

TEST(AtmosphericDepthEffectContractTest, DepthIsClampedToUnitRange)
{
    const std::vector<Pixel> sourcePixels{Pixel{0.3f, 0.3f, 0.3f, 1.0f}};
    const auto source = makeImage(1, 1, sourcePixels);

    const FogSettings settings;

    NamedDepthSampler overSampler(makeGrayDepthImage(1, 1, {2.0f}));
    EffectContext context;
    context.sampler = &overSampler;
    ArtifactAtmosphericDepthEffect overEffect;
    applySettings(overEffect, settings);
    overEffect.setContext(context);

    NamedDepthSampler clampedSampler(makeGrayDepthImage(1, 1, {1.0f}));
    ArtifactAtmosphericDepthEffect clampedEffect;
    applySettings(clampedEffect, settings);
    context.sampler = &clampedSampler;
    clampedEffect.setContext(context);

    NamedDepthSampler underSampler(makeGrayDepthImage(1, 1, {-1.0f}));
    ArtifactAtmosphericDepthEffect underEffect;
    applySettings(underEffect, settings);
    context.sampler = &underSampler;
    underEffect.setContext(context);

    NamedDepthSampler zeroSampler(makeGrayDepthImage(1, 1, {0.0f}));
    ArtifactAtmosphericDepthEffect zeroEffect;
    applySettings(zeroEffect, settings);
    context.sampler = &zeroSampler;
    zeroEffect.setContext(context);

    ImageF32x4RGBAWithCache overOutput;
    ImageF32x4RGBAWithCache clampedOutput;
    ImageF32x4RGBAWithCache underOutput;
    ImageF32x4RGBAWithCache zeroOutput;
    overEffect.applyCPUOnly(source, overOutput);
    clampedEffect.applyCPUOnly(source, clampedOutput);
    underEffect.applyCPUOnly(source, underOutput);
    zeroEffect.applyCPUOnly(source, zeroOutput);

    const auto over = overOutput.image().getPixel(0, 0);
    const auto clamped = clampedOutput.image().getPixel(0, 0);
    const auto under = underOutput.image().getPixel(0, 0);
    const auto zero = zeroOutput.image().getPixel(0, 0);

    EXPECT_NEAR(over.r(), clamped.r(), 1.0e-4f);
    EXPECT_NEAR(over.g(), clamped.g(), 1.0e-4f);
    EXPECT_NEAR(over.b(), clamped.b(), 1.0e-4f);

    EXPECT_NEAR(under.r(), zero.r(), 1.0e-4f);
    EXPECT_NEAR(under.g(), zero.g(), 1.0e-4f);
    EXPECT_NEAR(under.b(), zero.b(), 1.0e-4f);
}

TEST(AtmosphericDepthEffectContractTest, EffectIdentityMatchesCatalogEntry)
{
    ArtifactAtmosphericDepthEffect effect;
    EXPECT_EQ(effect.effectID().toQString(), QStringLiteral("builtin.atmospheric_depth"));
    EXPECT_EQ(effect.displayName().toQString(), QStringLiteral("Atmospheric Depth"));
}
