#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Glow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 16;
constexpr int kHeight = 16;

ImageF32x4RGBAWithCache makeBrightSurface()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.9f;
        pixels[i + 1] = 0.9f;
        pixels[i + 2] = 0.9f;
        pixels[i + 3] = 0.4f;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(VolumetricShineEffectContractTest, AddsThresholdedRaysAndKeepsAlpha)
{
    const auto source = makeBrightSurface();
    ImageF32x4RGBAWithCache output;
    VolumetricShineEffect effect;
    effect.setPropertyValue(UniString("threshold"), QVariant(0.1));
    effect.setPropertyValue(UniString("intensity"), QVariant(3.0));
    effect.setPropertyValue(UniString("rayLength"), QVariant(1.0));
    effect.setPropertyValue(UniString("samples"), QVariant(48));

    effect.applyCPUOnly(source, output);

    const auto before = source.image().getPixel(8, 8);
    const auto after = output.image().getPixel(8, 8);
    EXPECT_GT(after.r() + after.g() + after.b(),
              before.r() + before.g() + before.b());
    EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
    EXPECT_NEAR(source.image().getPixel(8, 8).r(), 0.9f, 1.0e-6f);
    EXPECT_TRUE(effect.supportsGPU());
    EXPECT_EQ(effect.gpuGenericKey(), VolumetricShineEffect::kGpuGenericKey);
}

TEST(VolumetricShineEffectContractTest, ZeroIntensityPreservesSourceRgba)
{
    const auto source = makeBrightSurface();
    ImageF32x4RGBAWithCache output;
    VolumetricShineEffect effect;
    effect.setPropertyValue(UniString("intensity"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    const float* outputPixels = output.image().rgba32fData();
    const float* sourcePixels = source.image().rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    ASSERT_NE(sourcePixels, nullptr);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(kWidth * kHeight); ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto index = pixel * 4 + channel;
            EXPECT_NEAR(outputPixels[index], sourcePixels[index], 1.0e-6f)
                << "pixel=" << pixel << " channel=" << channel;
        }
    }
}

TEST(VolumetricShineEffectContractTest, RoiHintRequiresTheFullFrame)
{
    VolumetricShineEffect effect;
    const auto hint = effect.roiHint();

    EXPECT_EQ(hint.kind, EffectROIHintKind::Glow);
    EXPECT_TRUE(hint.requiresFullFrame);
}
\n
