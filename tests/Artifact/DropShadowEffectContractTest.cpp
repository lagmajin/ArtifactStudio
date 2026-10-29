#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <QColor>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.DropShadow;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 7;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeAlphaImpulse()
{
    Pixels pixels{};
    const std::size_t offset =
        (static_cast<std::size_t>(kHeight / 2) * kWidth + kWidth / 2) * 4;
    pixels[offset] = 0.8f;
    pixels[offset + 1] = 0.2f;
    pixels[offset + 2] = 0.1f;
    pixels[offset + 3] = 0.5f;
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void configureHardShadow(Artifact::DropShadowEffect& effect)
{
    effect.setShadowColor(QColor(0, 0, 0, 128));
    effect.setDistance(1.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(0.0f);
    effect.setOpacity(100.0f);
}

} // namespace

TEST(DropShadowEffectContractTest, AngleZeroOffsetsAlphaOnePixelLeft)
{
    const auto source = makeAlphaImpulse();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DropShadowEffect effect;
    configureHardShadow(effect);

    effect.applyCPUOnly(source, output);

    const auto shadow = output.image().getPixel(kWidth / 2 - 1, kHeight / 2);
    EXPECT_NEAR(shadow.a(), 0.5f * (128.0f / 255.0f), 1.0e-6f);
    EXPECT_NEAR(shadow.r(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(shadow.g(), 0.0f, 1.0e-6f);
    EXPECT_NEAR(shadow.b(), 0.0f, 1.0e-6f);

    const auto sourcePixel = output.image().getPixel(kWidth / 2, kHeight / 2);
    EXPECT_FLOAT_EQ(sourcePixel.r(), 0.8f);
    EXPECT_FLOAT_EQ(sourcePixel.g(), 0.2f);
    EXPECT_FLOAT_EQ(sourcePixel.b(), 0.1f);
    EXPECT_FLOAT_EQ(sourcePixel.a(), 0.5f);
    EXPECT_FLOAT_EQ(output.image().getPixel(kWidth / 2 + 1, kHeight / 2).a(), 0.0f);
}

TEST(DropShadowEffectContractTest, ShadowOnlyKeepsShiftedShadowAndDropsSource)
{
    const auto source = makeAlphaImpulse();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::DropShadowEffect effect;
    configureHardShadow(effect);
    effect.setShadowOnly(true);

    effect.applyCPUOnly(source, output);

    EXPECT_NEAR(output.image().getPixel(kWidth / 2 - 1, kHeight / 2).a(),
                0.5f * (128.0f / 255.0f), 1.0e-6f);
    const auto sourcePosition = output.image().getPixel(kWidth / 2, kHeight / 2);
    EXPECT_FLOAT_EQ(sourcePosition.r(), 0.0f);
    EXPECT_FLOAT_EQ(sourcePosition.g(), 0.0f);
    EXPECT_FLOAT_EQ(sourcePosition.b(), 0.0f);
    EXPECT_FLOAT_EQ(sourcePosition.a(), 0.0f);
}

TEST(DropShadowEffectContractTest, GpuDescriptorCarriesOffsetTintOpacityAndShadowOnly)
{
    Artifact::DropShadowEffect effect;
    effect.setShadowColor(QColor(64, 128, 192, 128));
    effect.setDistance(2.0f);
    effect.setAngle(0.0f);
    effect.setSoftness(3.0f);
    effect.setOpacity(40.0f);
    effect.setShadowOnly(true);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey, Artifact::DropShadowEffect::kGpuGenericKey);
    EXPECT_NEAR(stack.nodes[0].parameters[0], -2.0f, 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[1], 0.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 3.0f);
    EXPECT_NEAR(stack.nodes[0].parameters[3], 0.4f * (128.0f / 255.0f), 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[4], 64.0f / 255.0f, 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[5], 128.0f / 255.0f, 1.0e-6f);
    EXPECT_NEAR(stack.nodes[0].parameters[6], 192.0f / 255.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[7], 1.0f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1) | (1u << 2));
}
