#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.Voronoi;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 8;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeColoredSurface()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[offset] = 0.1f * static_cast<float>(x + 1);
            pixels[offset + 1] = 0.2f * static_cast<float>(y + 1);
            pixels[offset + 2] = 0.3f;
            pixels[offset + 3] = 0.25f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void expectGrayValue(const ArtifactCore::ImageF32x4RGBAWithCache& image,
                     int x, int y, float expected)
{
    const auto pixel = image.image().getPixel(x, y);
    EXPECT_FLOAT_EQ(pixel.r(), expected);
    EXPECT_FLOAT_EQ(pixel.g(), expected);
    EXPECT_FLOAT_EQ(pixel.b(), expected);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
}

} // namespace

TEST(VoronoiEffectContractTest, ModesReportNearestAndSecondNearestCellDistances)
{
    const auto source = makeColoredSurface();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::VoronoiEffect effect;
    effect.setScale(10.0f);
    effect.setJitter(0.0f);
    effect.setSeed(0);

    effect.setMode(0);
    effect.applyCPUOnly(source, output);
    expectGrayValue(output, 0, 0, 0.0f);

    effect.setMode(1);
    effect.applyCPUOnly(source, output);
    expectGrayValue(output, 0, 0, 1.0f);

    effect.setMode(2);
    effect.applyCPUOnly(source, output);
    expectGrayValue(output, 0, 0, 1.0f);

    effect.setMode(3);
    effect.applyCPUOnly(source, output);
    expectGrayValue(output, 0, 0, 0.0f);
    EXPECT_FLOAT_EQ(source.image().getPixel(0, 0).a(), 0.25f);
}

TEST(VoronoiEffectContractTest, GpuDescriptorCarriesCellSettings)
{
    Artifact::VoronoiEffect effect;
    effect.setScale(25.0f);
    effect.setJitter(1.25f);
    effect.setMode(2);
    effect.setSeed(91);
    Artifact::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, Artifact::GpuSpatialEffectKind::Voronoi);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 25.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 1.25f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[2], 2.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[3], 91.0f);
    EXPECT_EQ(stack.nodes[0].resolutionScaledParameterMask, 1u << 0);
}

TEST(VoronoiEffectContractTest, SettersClampParametersAndResetNonFiniteValues)
{
    Artifact::VoronoiEffect effect;
    effect.setScale(0.0f);
    EXPECT_FLOAT_EQ(effect.scale(), 1.0f);
    effect.setScale(201.0f);
    EXPECT_FLOAT_EQ(effect.scale(), 200.0f);
    effect.setScale(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.scale(), 20.0f);

    effect.setJitter(-1.0f);
    EXPECT_FLOAT_EQ(effect.jitter(), 0.0f);
    effect.setJitter(3.0f);
    EXPECT_FLOAT_EQ(effect.jitter(), 2.0f);
    effect.setJitter(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.jitter(), 1.0f);

    effect.setMode(-1);
    EXPECT_EQ(effect.mode(), 0);
    effect.setMode(4);
    EXPECT_EQ(effect.mode(), 3);
    effect.setSeed(-1);
    EXPECT_EQ(effect.seed(), 0);
    effect.setSeed(10000);
    EXPECT_EQ(effect.seed(), 9999);
}
