#include <gtest/gtest.h>

#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

import Artifact.Effect.Abstract;
import FloatRGBA;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import PosterizeEffect;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

ImageF32x4RGBAWithCache makeSurface(const float* rgba, int width, int height)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba, width, height);
    return ImageF32x4RGBAWithCache(image);
}

void expectPixelNear(const ImageF32x4RGBAWithCache& surface, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1.0e-6f)
{
    const FloatRGBA actual = surface.image().getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

} // namespace

TEST(PosterizeEffectContractTest, QuantizesChannelsWithNearestLevelAndKeepsAlpha)
{
    const float pixels[] = {
        0.0f, 0.49f, 0.5f, 0.25f,
        1.0f, 0.2f, 0.8f, 0.75f,
    };
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    PosterizeEffect effect;
    effect.setLevels(4.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0,
                    FloatRGBA(0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 0.25f));
    expectPixelNear(output, 1, 0,
                    FloatRGBA(1.0f, 1.0f / 3.0f, 2.0f / 3.0f, 0.75f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.0f, 0.49f, 0.5f, 0.25f));
}

TEST(PosterizeEffectContractTest, TwoLevelsUseHalfUpBoundary)
{
    const float pixels[] = {0.499f, 0.5f, 0.501f, 0.4f};
    const auto source = makeSurface(pixels, 1, 1);
    ImageF32x4RGBAWithCache output;
    PosterizeEffect effect;
    effect.setLevels(2.0f);

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, FloatRGBA(0.0f, 1.0f, 1.0f, 0.4f));
}

TEST(PosterizeEffectContractTest, LevelSetterClampsRangeAndResetsNonFiniteInput)
{
    PosterizeEffect effect;
    effect.setLevels(1.0f);
    EXPECT_FLOAT_EQ(effect.levels(), 2.0f);
    effect.setLevels(100.0f);
    EXPECT_FLOAT_EQ(effect.levels(), 64.0f);
    effect.setLevels(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(effect.levels(), 4.0f);
}

TEST(PosterizeEffectContractTest, GpuSpatialDescriptorCarriesConfiguredLevels)
{
    PosterizeEffect effect;
    effect.setLevels(12.0f);
    GpuSpatialEffectStack stack;

    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("posterize"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 12.0f);
}

TEST(PosterizeEffectContractTest, ExperimentalContourPosterHasCpuFallbackAndSpatialNode)
{
    ExperimentalContourPosterEffect effect;
    EXPECT_TRUE(effect.supportsCPU());
    EXPECT_TRUE(effect.supportsGPU());
    effect.setLevels(9.0f);
    GpuSpatialEffectStack stack;
    ASSERT_TRUE(effect.appendGpuSpatialNodes(stack));
    ASSERT_EQ(stack.count, 1u);
    EXPECT_EQ(stack.nodes[0].kind, GpuSpatialEffectKind::Generic);
    EXPECT_EQ(stack.nodes[0].genericKey,
              gpuGenericKeyFromString("effect.colorcorrection.contour_poster.v1"));
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[0], 9.0f);
    EXPECT_FLOAT_EQ(stack.nodes[0].parameters[1], 1.0e-6f);
}

TEST(PosterizeEffectContractTest, ExperimentalContourPosterUsesCommonMixPropertyOnCpu)
{
    const float pixels[] = {
        0.18f, 0.12f, 0.06f, 1.0f,
        0.42f, 0.21f, 0.08f, 0.5f,
    };
    const auto source = makeSurface(pixels, 2, 1);
    ImageF32x4RGBAWithCache output;
    ExperimentalContourPosterEffect effect;
    effect.setPropertyValue(UniString("Effect Mix"), QVariant(0.0));

    effect.applyCPUOnly(source, output);

    expectPixelNear(output, 0, 0, source.image().getPixel(0, 0));
    expectPixelNear(output, 1, 0, source.image().getPixel(1, 0));
}

TEST(PosterizeEffectContractTest, ExperimentalContourPosterCanCaptureResearchCpuFixture)
{
    const char* inputPath = std::getenv("ARTIFACT_CONTOUR_POSTER_INPUT_NPY");
    const char* outputPath = std::getenv("ARTIFACT_CONTOUR_POSTER_OUTPUT_NPY");
    if (!inputPath || !outputPath) GTEST_SKIP() << "Research capture paths were not supplied";

    std::ifstream input(inputPath, std::ios::binary);
    ASSERT_TRUE(input.good());
    auto read = [&input](std::size_t count) {
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const int byte = input.get();
            if (byte < 0) throw std::runtime_error("Unexpected EOF in NPY header");
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(byte))
                     << (8u * index);
        }
        return value;
    };
    char magic[6]{};
    ASSERT_TRUE(input.read(magic, sizeof(magic)).good());
    EXPECT_EQ(std::string(magic, sizeof(magic)), std::string("\x93NUMPY", 6));
    const auto major = static_cast<unsigned char>(input.get());
    const auto minor = static_cast<unsigned char>(input.get());
    ASSERT_EQ(major, 1u);
    ASSERT_EQ(minor, 0u);
    const auto headerLength = read(2);
    std::string header(headerLength, '\0');
    ASSERT_TRUE(input.read(header.data(), static_cast<std::streamsize>(header.size())).good());
    ASSERT_NE(header.find("'<f4'"), std::string::npos);
    ASSERT_NE(header.find("False"), std::string::npos);
    const auto shapeKey = header.find("'shape': (");
    ASSERT_NE(shapeKey, std::string::npos);
    std::istringstream shape(header.substr(shapeKey + 10));
    std::size_t height = 0, width = 0, channels = 0;
    char separator = 0;
    shape >> height >> separator >> width >> separator >> channels;
    ASSERT_GT(height, 0u);
    ASSERT_GT(width, 0u);
    ASSERT_EQ(channels, 4u);
    std::vector<float> pixels(height * width * 4u);
    ASSERT_TRUE(input.read(reinterpret_cast<char*>(pixels.data()),
                           static_cast<std::streamsize>(pixels.size() * sizeof(float))).good());
    ASSERT_EQ(std::char_traits<char>::eof(), input.peek());

    const auto source = makeSurface(pixels.data(), static_cast<int>(width),
                                    static_cast<int>(height));
    ImageF32x4RGBAWithCache output;
    ExperimentalContourPosterEffect effect;
    effect.applyCPUOnly(source, output);
    const auto& image = output.image();
    std::ofstream result(outputPath, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(result.good());
    const auto* outputPixels = image.rgba32fData();
    ASSERT_NE(outputPixels, nullptr);
    for (std::size_t pixel = 0; pixel < height * width; ++pixel) {
        const auto value = image.getPixel(static_cast<int>(pixel % width),
                                          static_cast<int>(pixel / width));
        const float rgba[] = {value.r(), value.g(), value.b(), value.a()};
        result.write(reinterpret_cast<const char*>(rgba), sizeof(rgba));
    }
    ASSERT_TRUE(result.good());
}
