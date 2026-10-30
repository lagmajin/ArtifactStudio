#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.PixelSort;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
using Pixels = std::array<float, kWidth * kHeight * 4>;

ArtifactCore::ImageF32x4RGBAWithCache makeRepeatedLumaRows()
{
    constexpr std::array<float, kWidth> values{0.8f, 0.2f, 0.6f, 0.4f};
    Pixels pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[offset] = values[static_cast<std::size_t>(x)];
            pixels[offset + 1] = values[static_cast<std::size_t>(x)];
            pixels[offset + 2] = values[static_cast<std::size_t>(x)];
            pixels[offset + 3] = 0.1f + 0.1f * static_cast<float>(x);
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ArtifactCore::ImageF32x4RGBAWithCache(image);
}

void setSortProperty(Artifact::PixelSortProEffect& effect,
                     const char* name, float value)
{
    effect.setPropertyValue(ArtifactCore::UniString(name), QVariant(value));
}

} // namespace

TEST(PixelSortProEffectContractTest, SortsEligibleRunsAscendingAndPreservesSourceAlpha)
{
    const auto source = makeRepeatedLumaRows();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "maxSegment", 4.0f);
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "keyMode", 0.0f);
    setSortProperty(effect, "descending", 0.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    constexpr std::array<float, kWidth> sorted{0.2f, 0.4f, 0.6f, 0.8f};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), sorted[static_cast<std::size_t>(x)], 1.0e-6f);
            EXPECT_NEAR(pixel.g(), sorted[static_cast<std::size_t>(x)], 1.0e-6f);
            EXPECT_NEAR(pixel.b(), sorted[static_cast<std::size_t>(x)], 1.0e-6f);
            EXPECT_FLOAT_EQ(pixel.a(), source.image().getPixel(x, y).a());
        }
    }
}

TEST(PixelSortProEffectContractTest, HueKeyModeSortsAnEligibleRunByColorHue)
{
    constexpr int width = 3;
    constexpr std::array<std::array<float, 3>, width> inputColors{{
        {{0.0f, 0.0f, 1.0f}}, // blue
        {{1.0f, 0.0f, 0.0f}}, // red
        {{0.0f, 1.0f, 0.0f}}, // green
    }};
    constexpr std::array<std::array<float, 3>, width> expectedColors{{
        {{1.0f, 0.0f, 0.0f}}, // hue 0
        {{0.0f, 1.0f, 0.0f}}, // hue 1/3
        {{0.0f, 0.0f, 1.0f}}, // hue 2/3
    }};
    std::array<float, width * 4> pixels{};
    for (int x = 0; x < width; ++x) {
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            pixels[offset + channel] =
                inputColors[static_cast<std::size_t>(x)][channel];
        }
        pixels[offset + 3] = 0.2f + 0.2f * static_cast<float>(x);
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, 1);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "maxSegment", static_cast<float>(width));
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "keyMode", 1.0f);
    setSortProperty(effect, "descending", 0.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < width; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const auto& expected = expectedColors[static_cast<std::size_t>(x)];
        EXPECT_FLOAT_EQ(actual.r(), expected[0]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.g(), expected[1]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.b(), expected[2]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.a(), pixels[static_cast<std::size_t>(x) * 4 + 3])
            << "x=" << x;
    }
}

TEST(PixelSortProEffectContractTest,
     PublishedPropertiesClampValuesAndKeepThresholdsOrdered)
{
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 999.0f);
    setSortProperty(effect, "maxSegment", 1.0f);
    setSortProperty(effect, "lowThreshold", 3.5f);
    setSortProperty(effect, "highThreshold", 2.0f);
    setSortProperty(effect, "keyMode", -1.0f);
    setSortProperty(effect, "descending", 1.5f);
    setSortProperty(effect, "edgeProtection", -1.0f);
    setSortProperty(effect, "mix", 2.0f);

    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 8u);
    constexpr std::array<const char*, 8> expectedNames{
        "angle", "maxSegment", "lowThreshold", "highThreshold",
        "keyMode", "descending", "edgeProtection", "mix"};
    constexpr std::array<float, 8> expectedValues{
        180.0f, 2.0f, 3.5f, 3.5f, 0.0f, 1.0f, 0.0f, 1.0f};
    for (std::size_t index = 0; index < properties.size(); ++index) {
        EXPECT_EQ(properties[index].name(), QString::fromLatin1(expectedNames[index]))
            << "property=" << index;
        EXPECT_FLOAT_EQ(properties[index].getValue().toFloat(), expectedValues[index])
            << "property=" << expectedNames[index];
    }
}

TEST(PixelSortProEffectContractTest, MixZeroLeavesRgbUnsorted)
{
    const auto source = makeRepeatedLumaRows();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 0.0f);

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

TEST(PixelSortProEffectContractTest,
     EdgeProtectionReducesMixAtStrongLocalLumaTransitions)
{
    constexpr int width = 4;
    constexpr std::array<float, width> inputValues{0.8f, 0.2f, 0.6f, 0.4f};
    constexpr std::array<float, width> sortedValues{0.2f, 0.4f, 0.6f, 0.8f};
    std::array<float, width * 4> pixels{};
    for (int x = 0; x < width; ++x) {
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        pixels[offset] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 1] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 2] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 3] = 0.25f + 0.1f * static_cast<float>(x);
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, 1);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "maxSegment", static_cast<float>(width));
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "edgeProtection", 1.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < width; ++x) {
        const float left = inputValues[static_cast<std::size_t>(std::max(0, x - 1))];
        const float right = inputValues[static_cast<std::size_t>(std::min(width - 1, x + 1))];
        const float edge = std::clamp(std::abs(right - left) * 4.0f, 0.0f, 1.0f);
        const float expected = inputValues[static_cast<std::size_t>(x)] * edge +
            sortedValues[static_cast<std::size_t>(x)] * (1.0f - edge);
        const auto actual = output.image().getPixel(x, 0);
        EXPECT_NEAR(actual.r(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_NEAR(actual.g(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_NEAR(actual.b(), expected, 1.0e-6f) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.a(), pixels[static_cast<std::size_t>(x) * 4 + 3])
            << "x=" << x;
    }
}

TEST(PixelSortProEffectContractTest,
     DescendingSortReversesLumaOrderAndKeepsAlphaAtItsOriginalPixel)
{
    const auto source = makeRepeatedLumaRows();
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "maxSegment", 4.0f);
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "keyMode", 0.0f);
    setSortProperty(effect, "descending", 1.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    constexpr std::array<float, kWidth> sorted{0.8f, 0.6f, 0.4f, 0.2f};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto pixel = output.image().getPixel(x, y);
            EXPECT_NEAR(pixel.r(), sorted[static_cast<std::size_t>(x)], 1.0e-6f);
            EXPECT_NEAR(pixel.g(), sorted[static_cast<std::size_t>(x)], 1.0e-6f);
            EXPECT_NEAR(pixel.b(), sorted[static_cast<std::size_t>(x)], 1.0e-6f);
            EXPECT_FLOAT_EQ(pixel.a(), source.image().getPixel(x, y).a());
        }
    }
}

TEST(PixelSortProEffectContractTest,
     ThresholdEndpointsSortOnlyTheirContiguousEligibleRun)
{
    constexpr int width = 8;
    constexpr int height = 1;
    constexpr std::array<float, width> inputValues{
        0.9f, 0.6f, 0.2f, 0.75f, 0.25f, 0.8f, 0.1f, 0.4f};
    constexpr std::array<float, width> expectedValues{
        0.9f, 0.6f, 0.2f, 0.25f, 0.75f, 0.8f, 0.1f, 0.4f};
    std::array<float, width * 4> pixels{};
    for (int x = 0; x < width; ++x) {
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        pixels[offset] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 1] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 2] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 3] = static_cast<float>(x + 1) / 10.0f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "maxSegment", static_cast<float>(width));
    setSortProperty(effect, "lowThreshold", 0.25f);
    setSortProperty(effect, "highThreshold", 0.75f);
    setSortProperty(effect, "keyMode", 0.0f);
    setSortProperty(effect, "descending", 0.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < width; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const std::size_t index = static_cast<std::size_t>(x);
        EXPECT_FLOAT_EQ(actual.r(), expectedValues[index]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.g(), expectedValues[index]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.b(), expectedValues[index]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.a(), pixels[index * 4 + 3]) << "x=" << x;
    }
}

TEST(PixelSortProEffectContractTest,
     MaximumSegmentSplitsLongEligibleRunsAtItsConfiguredLimit)
{
    constexpr int width = 6;
    constexpr int height = 1;
    constexpr std::array<float, width> inputValues{
        0.6f, 0.1f, 0.5f, 0.9f, 0.3f, 0.7f};
    constexpr std::array<float, width> expectedValues{
        0.1f, 0.5f, 0.6f, 0.3f, 0.7f, 0.9f};
    std::array<float, width * 4> pixels{};
    for (int x = 0; x < width; ++x) {
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        pixels[offset] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 1] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 2] = inputValues[static_cast<std::size_t>(x)];
        pixels[offset + 3] = static_cast<float>(x + 1) / 7.0f;
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 0.0f);
    setSortProperty(effect, "maxSegment", 3.0f);
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "keyMode", 0.0f);
    setSortProperty(effect, "descending", 0.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    for (int x = 0; x < width; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const std::size_t index = static_cast<std::size_t>(x);
        EXPECT_FLOAT_EQ(actual.r(), expectedValues[index]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.g(), expectedValues[index]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.b(), expectedValues[index]) << "x=" << x;
        EXPECT_FLOAT_EQ(actual.a(), pixels[index * 4 + 3]) << "x=" << x;
    }
}

TEST(PixelSortProEffectContractTest,
     AngledSortingPreservesOriginalAlphaAtEveryPixel)
{
    constexpr int width = 5;
    constexpr int height = 5;
    std::array<float, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4;
            const float value = static_cast<float>((x * 3 + y * 2) % 9) / 10.0f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] =
                static_cast<float>(1 + y * width + x) / 26.0f;
        }
    }
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const auto descriptor =
        ArtifactCore::SurfaceColorDescriptor::linearStraightRgba32Float();
    image.setColorDescriptor(descriptor);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::PixelSortProEffect effect;
    setSortProperty(effect, "angle", 45.0f);
    setSortProperty(effect, "maxSegment", 5.0f);
    setSortProperty(effect, "lowThreshold", 0.0f);
    setSortProperty(effect, "highThreshold", 1.0f);
    setSortProperty(effect, "edgeProtection", 0.0f);
    setSortProperty(effect, "mix", 1.0f);

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    EXPECT_EQ(source.image().colorDescriptor(), descriptor);
    EXPECT_EQ(output.image().colorDescriptor(), descriptor);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    for (std::size_t pixel = 0; pixel < source.image().totalPixels(); ++pixel) {
        EXPECT_FLOAT_EQ(actual[pixel * 4 + 3], pixels[pixel * 4 + 3])
            << "pixel=" << pixel;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(original[pixel * 4 + channel],
                            pixels[pixel * 4 + channel])
                << "source pixel=" << pixel << " channel=" << channel;
        }
    }
}
