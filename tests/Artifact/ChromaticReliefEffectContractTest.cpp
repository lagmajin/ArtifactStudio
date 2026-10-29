#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ChromaticRelief;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 5;
constexpr float kAlpha = 0.7f;

ImageF32x4RGBAWithCache makeHorizontalGrayRamp()
{
    std::array<float, kWidth * kHeight * 4> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            const float value = static_cast<float>(x) / static_cast<float>(kWidth - 1);
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

} // namespace

TEST(ChromaticReliefEffectContractTest, DirectionalReliefSeparatesColorChannelsAndKeepsAlpha)
{
    const auto source = makeHorizontalGrayRamp();
    ImageF32x4RGBAWithCache output;
    ChromaticReliefEffect effect;
    effect.setPropertyValue(UniString("Relief Amount"), QVariant(0.7f));
    effect.setPropertyValue(UniString("Chromatic Offset"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Direction"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Edge Softness"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Mix"), QVariant(1.0f));

    effect.applyCPUOnly(source, output);

    const auto original = source.image().getPixel(4, 2);
    const auto relieved = output.image().getPixel(4, 2);
    EXPECT_GT(std::abs(relieved.r() - relieved.b()), 0.1f);
    EXPECT_NEAR(relieved.a(), kAlpha, 1.0e-6f);
    EXPECT_NEAR(original.r(), original.g(), 1.0e-6f);
    EXPECT_NEAR(original.g(), original.b(), 1.0e-6f);
}

TEST(ChromaticReliefEffectContractTest, ZeroMixPreservesSourceDespiteConfiguredRelief)
{
    const auto source = makeHorizontalGrayRamp();
    ImageF32x4RGBAWithCache output;
    ChromaticReliefEffect effect;
    effect.setPropertyValue(UniString("Relief Amount"), QVariant(2.0f));
    effect.setPropertyValue(UniString("Chromatic Offset"), QVariant(8.0f));
    effect.setPropertyValue(UniString("Direction"), QVariant(37.0f));
    effect.setPropertyValue(UniString("Mix"), QVariant(0.0f));

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto before = source.image().getPixel(x, y);
            const auto after = output.image().getPixel(x, y);
            EXPECT_NEAR(after.r(), before.r(), 1.0e-6f);
            EXPECT_NEAR(after.g(), before.g(), 1.0e-6f);
            EXPECT_NEAR(after.b(), before.b(), 1.0e-6f);
            EXPECT_NEAR(after.a(), before.a(), 1.0e-6f);
        }
    }
}

TEST(ChromaticReliefEffectContractTest, LargeOffsetClampsChannelSamplesAtImageEdges)
{
    const auto source = makeHorizontalGrayRamp();
    ImageF32x4RGBAWithCache edgeOffsetOutput;
    ImageF32x4RGBAWithCache beyondEdgeOffsetOutput;
    ChromaticReliefEffect effect;
    effect.setPropertyValue(UniString("Relief Amount"), QVariant(0.7f));
    effect.setPropertyValue(UniString("Direction"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Edge Softness"), QVariant(0.0f));
    effect.setPropertyValue(UniString("Mix"), QVariant(1.0f));

    effect.setPropertyValue(UniString("Chromatic Offset"), QVariant(8.0f));
    effect.applyCPUOnly(source, edgeOffsetOutput);
    effect.setPropertyValue(UniString("Chromatic Offset"), QVariant(40.0f));
    effect.applyCPUOnly(source, beyondEdgeOffsetOutput);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto atEdge = edgeOffsetOutput.image().getPixel(x, y);
            const auto beyondEdge = beyondEdgeOffsetOutput.image().getPixel(x, y);
            EXPECT_NEAR(atEdge.r(), beyondEdge.r(), 1.0e-6f);
            EXPECT_NEAR(atEdge.g(), beyondEdge.g(), 1.0e-6f);
            EXPECT_NEAR(atEdge.b(), beyondEdge.b(), 1.0e-6f);
            EXPECT_NEAR(atEdge.a(), beyondEdge.a(), 1.0e-6f);
        }
    }
}
\n
