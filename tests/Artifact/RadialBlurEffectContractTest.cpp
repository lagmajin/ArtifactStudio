#include <gtest/gtest.h>

#include <array>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.RadialBlur;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

ArtifactCore::ImageF32x4RGBAWithCache makeRamp()
{
    const std::array<float, 5 * 4> pixels{
        0.0f, 0.0f, 0.0f, 0.0f,
        0.2f, 0.2f, 0.2f, 0.2f,
        0.4f, 0.4f, 0.4f, 0.4f,
        0.6f, 0.6f, 0.6f, 0.6f,
        0.8f, 0.8f, 0.8f, 0.8f,
    };
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 5, 1);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

} // namespace

TEST(RadialBlurEffectContractTest, ZoomAveragesOutwardSamplesAndKeepsCenter)
{
    const auto source = makeRamp();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::RadialBlurEffect effect;
    effect.setAmount(2.0f);
    effect.setQuality(2);
    effect.setCenterX(0.8f);
    effect.setCenterY(0.0f);
    effect.setMode(0.0f);

    effect.applyCPUOnly(source, output);

    const auto center = output.image().getPixel(4, 0);
    EXPECT_FLOAT_EQ(center.r(), 0.8f);
    EXPECT_FLOAT_EQ(center.a(), 0.8f);

    // At x=3, the zoom taps land on source x=3 and x=2.
    const float expected = (0.6f + 0.6f + 0.4f) / 3.0f;
    const auto edge = output.image().getPixel(3, 0);
    EXPECT_NEAR(edge.r(), expected, 1.0e-6f);
    EXPECT_NEAR(edge.g(), expected, 1.0e-6f);
    EXPECT_NEAR(edge.b(), expected, 1.0e-6f);
    EXPECT_NEAR(edge.a(), expected, 1.0e-6f);
}

TEST(RadialBlurEffectContractTest, ZeroAmountIsAnIdentityForTheWholeSurface)
{
    const auto source = makeRamp();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::RadialBlurEffect effect;
    effect.setAmount(0.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < 5; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const auto expected = source.image().getPixel(x, 0);
        EXPECT_FLOAT_EQ(actual.r(), expected.r());
        EXPECT_FLOAT_EQ(actual.g(), expected.g());
        EXPECT_FLOAT_EQ(actual.b(), expected.b());
        EXPECT_FLOAT_EQ(actual.a(), expected.a());
    }
}

TEST(RadialBlurEffectContractTest, SettersClampQualityCentersAmountAndMode)
{
    Artifact::RadialBlurEffect effect;
    effect.setAmount(-1.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 0.0f);
    effect.setAmount(3.0f);
    EXPECT_FLOAT_EQ(effect.amount(), 2.0f);
    effect.setAmount(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.amount(), 0.5f);

    effect.setQuality(1);
    EXPECT_EQ(effect.quality(), 2);
    effect.setQuality(40);
    EXPECT_EQ(effect.quality(), 32);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);

    effect.setMode(2.0f);
    EXPECT_FLOAT_EQ(effect.mode(), 1.0f);
    effect.setMode(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.mode(), 0.0f);
}
