#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.HexGrid;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 9;
constexpr int kHeight = 7;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeTransparentSurface()
{
    Pixels pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.3f;
        pixels[i + 1] = 0.6f;
        pixels[i + 2] = 0.9f;
        pixels[i + 3] = 0.25f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectPatternPixel(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                        int x, int y, float expected)
{
    const auto pixel = image.image().getPixel(x, y);
    EXPECT_FLOAT_EQ(pixel.r(), expected);
    EXPECT_FLOAT_EQ(pixel.g(), expected);
    EXPECT_FLOAT_EQ(pixel.b(), expected);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
}

} // namespace

TEST(HexGridEffectContractTest, CellInteriorAndGridLinesReplaceInputWithOpaquePattern)
{
    const auto source = makeTransparentSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::HexGridEffect effect;
    effect.setCellSize(8.0f);
    effect.setLineWidth(0.5f);
    effect.setAngle(0.0f);

    effect.applyCPUOnly(source, output);

    expectPatternPixel(output, 4, 3, 1.0f);
    expectPatternPixel(output, 0, 3, 0.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(4, 3).a(), 0.25f);

    effect.setLineWidth(8.0f);
    effect.applyCPUOnly(source, output);
    expectPatternPixel(output, 4, 3, 0.0f);
}

TEST(HexGridEffectContractTest, GpuDescriptorCarriesCellGeometryAndResolutionScaling)
{
    Artifact::HexGridEffect effect;
    effect.setCellSize(48.0f);
    effect.setLineWidth(3.0f);
    effect.setAngle(30.0f);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::HexGrid);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 48.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 3.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 30.0f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask,
              (1u << 0) | (1u << 1));
}

TEST(HexGridEffectContractTest, SettersClampGeometryAndResetNonFiniteValues)
{
    Artifact::HexGridEffect effect;
    effect.setCellSize(1.0f);
    EXPECT_FLOAT_EQ(effect.cellSize(), 4.0f);
    effect.setCellSize(2048.0f);
    EXPECT_FLOAT_EQ(effect.cellSize(), 1024.0f);
    effect.setCellSize(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.cellSize(), 32.0f);

    effect.setLineWidth(0.0f);
    EXPECT_FLOAT_EQ(effect.lineWidth(), 0.5f);
    effect.setLineWidth(2048.0f);
    EXPECT_FLOAT_EQ(effect.lineWidth(), 1024.0f);
    effect.setLineWidth(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.lineWidth(), 2.0f);

    effect.setAngle(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.angle(), 0.0f);
    effect.setAngle(450.0f);
    EXPECT_FLOAT_EQ(effect.angle(), 450.0f);
}
