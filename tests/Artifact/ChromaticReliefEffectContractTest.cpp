#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.ChromaticRelief;
import Property.Abstract;
import Utils.String.UniString;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 3;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeChannelRamp()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[offset] = 0.1f * static_cast<float>(x + 1);
            pixels[offset + 1] = 0.1f * static_cast<float>(x + 1);
            pixels[offset + 2] = 0.1f * static_cast<float>(x + 1);
            pixels[offset + 3] = 0.15f + 0.1f * static_cast<float>(x);
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(ChromaticReliefEffectContractTest, MixZeroPreservesEveryInputChannel)
{
    const auto source = makeChannelRamp();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ChromaticReliefEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Mix"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const auto expected = source.image().getPixel(x, y);
            EXPECT_FLOAT_EQ(actual.r(), expected.r());
            EXPECT_FLOAT_EQ(actual.g(), expected.g());
            EXPECT_FLOAT_EQ(actual.b(), expected.b());
            EXPECT_FLOAT_EQ(actual.a(), expected.a());
        }
    }
}

TEST(ChromaticReliefEffectContractTest, DirectionOffsetsRedAndBlueInOppositeDirections)
{
    const auto source = makeChannelRamp();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ChromaticReliefEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Relief Amount"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("Edge Softness"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("Chromatic Offset"), QVariant(2.0));
    effect.setPropertyValue(ArtifactCore::UniString("Direction"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("Mix"), QVariant(1.0));

    effect.applyCPUOnly(source, output);

    const auto forward = output.image().getPixel(3, 1);
    EXPECT_NEAR(forward.r(), source.image().getPixel(5, 1).r(), 1.0e-6f);
    EXPECT_NEAR(forward.b(), source.image().getPixel(1, 1).b() * 1.025f, 1.0e-6f);
    EXPECT_FLOAT_EQ(forward.a(), source.image().getPixel(3, 1).a());

    effect.setPropertyValue(ArtifactCore::UniString("Direction"), QVariant(180.0));
    effect.applyCPUOnly(source, output);
    const auto reverse = output.image().getPixel(3, 1);
    EXPECT_NEAR(reverse.r(), source.image().getPixel(1, 1).r(), 1.0e-6f);
    EXPECT_NEAR(reverse.b(), source.image().getPixel(5, 1).b() * 1.025f, 1.0e-6f);
    EXPECT_FLOAT_EQ(reverse.a(), source.image().getPixel(3, 1).a());
}

TEST(ChromaticReliefEffectContractTest, PropertyValuesClampAndResetNonFiniteInputs)
{
    Artifact::ChromaticReliefEffect effect;

    effect.setPropertyValue(ArtifactCore::UniString("Relief Amount"), QVariant(-1.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[0].getValue().toFloat(), 0.0f);
    effect.setPropertyValue(ArtifactCore::UniString("Relief Amount"), QVariant(4.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[0].getValue().toFloat(), 3.0f);
    effect.setPropertyValue(ArtifactCore::UniString("Relief Amount"),
                            std::numeric_limits<double>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.getProperties()[0].getValue().toFloat(), 0.7f);

    effect.setPropertyValue(ArtifactCore::UniString("Chromatic Offset"), QVariant(50.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[1].getValue().toFloat(), 40.0f);
    effect.setPropertyValue(ArtifactCore::UniString("Edge Softness"), QVariant(-1.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[3].getValue().toFloat(), 0.0f);
    effect.setPropertyValue(ArtifactCore::UniString("Mix"), QVariant(2.0));
    EXPECT_FLOAT_EQ(effect.getProperties()[4].getValue().toFloat(), 1.0f);
}
