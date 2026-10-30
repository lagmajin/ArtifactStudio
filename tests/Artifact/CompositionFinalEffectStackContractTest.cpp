#include <gtest/gtest.h>

#include <QString>

#include <array>
#include <cstddef>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Effect.Abstract;
import Artifact.Render.CompositionViewDrawing;
import BrightnessEffect;
import Color.Float;
import Composition3D;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 3;
constexpr int kHeight = 2;
constexpr std::size_t kChannelCount =
    static_cast<std::size_t>(kWidth * kHeight * 4);

SharedPtr<ArtifactComposition3D> makeComposition()
{
    ArtifactCompositionInitParams params(
        UniString(QStringLiteral("Final effect stack contract")),
        FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kWidth, kHeight);
    params.setDurationFrames(1);
    return makeShared<ArtifactComposition3D>(
        CompositionID(QStringLiteral("final-effect-stack-contract")), params);
}

ImageF32x4_RGBA makeImage(const std::array<float, kChannelCount>& pixels)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return image;
}

} // namespace

TEST(CompositionFinalEffectStackContractTest,
     CompositionBufferMatchesDirectEffectAndDisabledStackIsUntouched)
{
    constexpr std::array<float, kChannelCount> sourcePixels{
        0.10f, 0.20f, 0.30f, 0.0f,
        0.25f, 0.35f, 0.45f, 0.2f,
        0.40f, 0.50f, 0.60f, 0.4f,
        0.55f, 0.65f, 0.75f, 0.6f,
        0.70f, 0.80f, 0.90f, 0.8f,
        0.85f, 0.95f, 1.00f, 1.0f};
    auto composition = makeComposition();
    ASSERT_TRUE(composition);

    auto stackBrightness = makeShared<BrightnessEffect>();
    ASSERT_TRUE(stackBrightness);
    stackBrightness->setBrightness(0.1f);
    composition->addEffect(stackBrightness);

    ImageF32x4_RGBA actual = makeImage(sourcePixels);
    ASSERT_TRUE(applyCompositionFinalEffectsToBuffer(
        composition.get(), actual));

    const ImageF32x4_RGBA source = makeImage(sourcePixels);
    const ImageF32x4RGBAWithCache sourceSurface(source);
    ImageF32x4RGBAWithCache directOutput;
    BrightnessEffect directBrightness;
    directBrightness.setBrightness(0.1f);
    directBrightness.applyCPUOnly(sourceSurface, directOutput);

    const float* actualPixels = actual.rgba32fData();
    const float* expectedPixels = directOutput.image().rgba32fData();
    ASSERT_NE(actualPixels, nullptr);
    ASSERT_NE(expectedPixels, nullptr);
    for (std::size_t channel = 0; channel < kChannelCount; ++channel) {
        EXPECT_FLOAT_EQ(actualPixels[channel], expectedPixels[channel])
            << "enabled composition stack channel=" << channel;
    }

    stackBrightness->setEnabled(false);
    ImageF32x4_RGBA disabled = makeImage(sourcePixels);
    EXPECT_FALSE(applyCompositionFinalEffectsToBuffer(
        composition.get(), disabled));
    const float* disabledPixels = disabled.rgba32fData();
    ASSERT_NE(disabledPixels, nullptr);
    for (std::size_t channel = 0; channel < kChannelCount; ++channel) {
        EXPECT_FLOAT_EQ(disabledPixels[channel], sourcePixels[channel])
            << "disabled composition stack channel=" << channel;
    }
}
