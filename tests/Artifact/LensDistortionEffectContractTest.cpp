#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>

import Artifact.Effect.Abstract;
import Artifact.Effect.LensDistortion;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeSurface(const Pixels& rgba)
{
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

std::size_t pixelOffset(int x, int y)
{
    return (static_cast<std::size_t>(y) * kWidth + x) * 4;
}

Pixels makeMarkedPixels()
{
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = pixelOffset(x, y);
            const float value = static_cast<float>(y * kWidth + x);
            pixels[offset] = value;
            pixels[offset + 1] = value + 20.0f;
            pixels[offset + 2] = value + 40.0f;
            pixels[offset + 3] = 0.25f + value * 0.01f;
        }
    }
    return pixels;
}

} // namespace

TEST(LensDistortionEffectContractTest, NeutralSettingsPreserveEveryRgbaPixel)
{
    const Pixels pixels = makeMarkedPixels();
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::LensDistortionEffect effect;

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), kWidth);
    ASSERT_EQ(output.image().height(), kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto actual = output.image().getPixel(x, y);
            const std::size_t offset = pixelOffset(x, y);
            EXPECT_FLOAT_EQ(actual.r(), pixels[offset]);
            EXPECT_FLOAT_EQ(actual.g(), pixels[offset + 1]);
            EXPECT_FLOAT_EQ(actual.b(), pixels[offset + 2]);
            EXPECT_FLOAT_EQ(actual.a(), pixels[offset + 3]);
        }
    }
}

TEST(LensDistortionEffectContractTest, TransparentEdgesReplaceOutOfBoundsSamples)
{
    const Pixels pixels = makeMarkedPixels();
    const auto source = makeSurface(pixels);
    ArtifactCore::ImageF32x4RGBAWithCache wrappedOutput;
    ArtifactCore::ImageF32x4RGBAWithCache transparentOutput;
    Artifact::LensDistortionEffect wrapped;
    wrapped.setDistortion(50.0f);
    wrapped.setTransparentEdges(false);
    wrapped.applyCPUOnly(source, wrappedOutput);

    Artifact::LensDistortionEffect transparent;
    transparent.setDistortion(50.0f);
    transparent.setTransparentEdges(true);
    transparent.applyCPUOnly(source, transparentOutput);

    // Positive radial distortion sends the top-left output sample to (-2, -2).
    // The sampler wraps to (2, 2) unless transparent edges are requested.
    const auto wrappedCorner = wrappedOutput.image().getPixel(0, 0);
    const std::size_t centerOffset = pixelOffset(2, 2);
    EXPECT_FLOAT_EQ(wrappedCorner.r(), pixels[centerOffset]);
    EXPECT_FLOAT_EQ(wrappedCorner.g(), pixels[centerOffset + 1]);
    EXPECT_FLOAT_EQ(wrappedCorner.b(), pixels[centerOffset + 2]);
    EXPECT_FLOAT_EQ(wrappedCorner.a(), pixels[centerOffset + 3]);

    const auto transparentCorner = transparentOutput.image().getPixel(0, 0);
    EXPECT_FLOAT_EQ(transparentCorner.r(), 0.0f);
    EXPECT_FLOAT_EQ(transparentCorner.g(), 0.0f);
    EXPECT_FLOAT_EQ(transparentCorner.b(), 0.0f);
    EXPECT_FLOAT_EQ(transparentCorner.a(), 0.0f);
}

TEST(LensDistortionEffectContractTest, GpuSpatialDescriptorCarriesConfiguration)
{
    Artifact::LensDistortionEffect effect;
    effect.setDistortion(12.0f);
    effect.setRadialQuadratic(-8.0f);
    effect.setCenterX(0.2f);
    effect.setCenterY(0.7f);
    effect.setZoom(1.5f);
    effect.setInvertDistortion(true);
    effect.setTangentialX(0.3f);
    effect.setTangentialY(-0.4f);
    effect.setTransparentEdges(true);
    ArtifactCore::GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind,
              ArtifactCore::GpuSpatialEffectKind::LensDistortion);
    const std::array<float, 9> expected{
        12.0f, -8.0f, 0.2f, 0.7f, 1.5f, 1.0f, 0.3f, -0.4f, 1.0f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FLOAT_EQ(stack.nodes[0].parameters[i], expected[i]);
    }
}

TEST(LensDistortionEffectContractTest, SettersClampValuesAndResetNonFiniteNumbers)
{
    Artifact::LensDistortionEffect effect;
    effect.setDistortion(150.0f);
    EXPECT_FLOAT_EQ(effect.distortion(), 100.0f);
    effect.setDistortion(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.distortion(), 0.0f);
    effect.setRadialQuadratic(-150.0f);
    EXPECT_FLOAT_EQ(effect.radialQuadratic(), -100.0f);
    effect.setRadialQuadratic(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.radialQuadratic(), 0.0f);

    effect.setCenterX(-1.0f);
    EXPECT_FLOAT_EQ(effect.centerX(), 0.0f);
    effect.setCenterX(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.centerX(), 0.5f);
    effect.setCenterY(2.0f);
    EXPECT_FLOAT_EQ(effect.centerY(), 1.0f);
    effect.setCenterY(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.centerY(), 0.5f);

    effect.setTangentialX(-2.0f);
    EXPECT_FLOAT_EQ(effect.tangentialX(), -1.0f);
    effect.setTangentialX(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(effect.tangentialX(), 0.0f);
    effect.setTangentialY(2.0f);
    EXPECT_FLOAT_EQ(effect.tangentialY(), 1.0f);
    effect.setTangentialY(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.tangentialY(), 0.0f);

    effect.setZoom(-1.0f);
    EXPECT_FLOAT_EQ(effect.zoom(), 0.01f);
    effect.setZoom(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.zoom(), 1.0f);
}
