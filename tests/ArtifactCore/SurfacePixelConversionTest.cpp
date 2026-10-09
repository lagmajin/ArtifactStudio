#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

import Graphics.SurfaceColorContract;
import Image.SurfacePixelConversion;

using namespace ArtifactCore;

TEST(SurfacePixelConversionTest, DecodesSrgbFloatAndZerosTransparentRgb)
{
    constexpr std::array<float, 8> source = {
        0.5f, 0.25f, 0.125f, 0.5f,
        1.0f, 0.5f, 0.25f, 0.0f};
    auto descriptor = SurfaceColorDescriptor::legacyOpenCvBgra32Float();
    descriptor.channelOrder = SurfaceChannelOrder::RGBA;

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 2, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    EXPECT_EQ(converted.width, 2u);
    EXPECT_EQ(converted.height, 1u);
    EXPECT_EQ(converted.rowStride, sizeof(float) * 8u);
    EXPECT_EQ(converted.descriptor,
              SurfaceColorDescriptor::linearStraightRgba32Float());
    std::array<float, 8> pixels{};
    std::memcpy(pixels.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_NEAR(pixels[0], 0.21404114f, 1e-5f);
    EXPECT_NEAR(pixels[1], 0.05087609f, 1e-5f);
    EXPECT_NEAR(pixels[2], 0.01434987f, 1e-5f);
    EXPECT_FLOAT_EQ(pixels[3], 0.5f);
    EXPECT_FLOAT_EQ(pixels[4], 0.0f);
    EXPECT_FLOAT_EQ(pixels[5], 0.0f);
    EXPECT_FLOAT_EQ(pixels[6], 0.0f);
    EXPECT_FLOAT_EQ(pixels[7], 0.0f);
}

TEST(SurfacePixelConversionTest, ReordersAndUnpremultipliesBgraInput)
{
    constexpr std::array<float, 4> source = {0.125f, 0.25f, 0.375f, 0.5f};
    auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 4> pixel{};
    std::memcpy(pixel.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_FLOAT_EQ(pixel[0], 0.75f);
    EXPECT_FLOAT_EQ(pixel[1], 0.5f);
    EXPECT_FLOAT_EQ(pixel[2], 0.25f);
    EXPECT_FLOAT_EQ(pixel[3], 0.5f);
}

TEST(SurfacePixelConversionTest, PremultipliedTransparentInputZerosRgb)
{
    constexpr std::array<float, 4> source = {0.8f, 0.4f, 0.2f, 0.0f};
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 4> pixel{};
    std::memcpy(pixel.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_FLOAT_EQ(pixel[0], 0.0f);
    EXPECT_FLOAT_EQ(pixel[1], 0.0f);
    EXPECT_FLOAT_EQ(pixel[2], 0.0f);
    EXPECT_FLOAT_EQ(pixel[3], 0.0f);
}

TEST(SurfacePixelConversionTest, UnknownChannelOrderUsesLegacyBgraInterpretation)
{
    constexpr std::array<float, 4> source = {0.125f, 0.25f, 0.375f, 1.0f};
    auto descriptor = SurfaceColorDescriptor::legacyOpenCvBgra32Float(
        TransferFunction::Linear, SurfaceAlphaMode::Straight);
    descriptor.channelOrder = SurfaceChannelOrder::Unknown;

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 4> pixel{};
    std::memcpy(pixel.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_FLOAT_EQ(pixel[0], 0.375f);
    EXPECT_FLOAT_EQ(pixel[1], 0.25f);
    EXPECT_FLOAT_EQ(pixel[2], 0.125f);
    EXPECT_FLOAT_EQ(pixel[3], 1.0f);
}

TEST(SurfacePixelConversionTest, ConvertsByteInputAndForcesOpaqueAlpha)
{
    constexpr std::array<std::uint8_t, 4> source = {128, 64, 32, 0};
    auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
    descriptor.alphaMode = SurfaceAlphaMode::Opaque;

    const auto converted = convertSurfacePixels(
        nullptr, source.data(), 1, 1, descriptor,
        SurfacePixelTarget::Rgba8SrgbStraight);

    ASSERT_TRUE(converted.isValid());
    ASSERT_EQ(converted.bytes.size(), 4u);
    EXPECT_EQ(converted.descriptor,
              SurfaceColorDescriptor::encodedSrgbRgba8Straight());
    EXPECT_EQ(converted.bytes[0], 128u);
    EXPECT_EQ(converted.bytes[1], 64u);
    EXPECT_EQ(converted.bytes[2], 32u);
    EXPECT_EQ(converted.bytes[3], 255u);
}

TEST(SurfacePixelConversionTest, EncodesLinearFloatInputAsQuantizedSrgbBytes)
{
    constexpr std::array<float, 4> source = {0.5f, 0.25f, 0.125f, 0.5f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba8SrgbStraight);

    ASSERT_TRUE(converted.isValid());
    ASSERT_EQ(converted.bytes.size(), 4u);
    EXPECT_EQ(converted.descriptor,
              SurfaceColorDescriptor::encodedSrgbRgba8Straight());
    EXPECT_EQ(converted.bytes[0], 188u);
    EXPECT_EQ(converted.bytes[1], 137u);
    EXPECT_EQ(converted.bytes[2], 99u);
    EXPECT_EQ(converted.bytes[3], 128u);
}

TEST(SurfacePixelConversionTest, EncodesLinearStraightPixelsAsHalfFloat)
{
    constexpr std::array<float, 4> source = {0.25f, 0.125f, 0.0625f, 0.5f};
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba16LinearStraight);

    ASSERT_TRUE(converted.isValid());
    EXPECT_EQ(converted.rowStride, 8u);
    EXPECT_EQ(converted.descriptor,
              SurfaceColorDescriptor::linearStraightRgba16Float());
    std::array<std::uint16_t, 4> half{};
    std::memcpy(half.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_EQ(half[0], 0x3800u);
    EXPECT_EQ(half[1], 0x3400u);
    EXPECT_EQ(half[2], 0x3000u);
    EXPECT_EQ(half[3], 0x3800u);
}

TEST(SurfacePixelConversionTest, SanitizesNonFiniteChannelsAndClampsAlpha)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 8> source = {
        nan, infinity, -infinity, nan,
        0.5f, 0.25f, 0.125f, 2.0f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 2, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 8> pixels{};
    std::memcpy(pixels.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_FLOAT_EQ(pixels[0], 0.0f);
    EXPECT_FLOAT_EQ(pixels[1], 0.0f);
    EXPECT_FLOAT_EQ(pixels[2], 0.0f);
    EXPECT_FLOAT_EQ(pixels[3], 0.0f);
    EXPECT_FLOAT_EQ(pixels[4], 0.5f);
    EXPECT_FLOAT_EQ(pixels[5], 0.25f);
    EXPECT_FLOAT_EQ(pixels[6], 0.125f);
    EXPECT_FLOAT_EQ(pixels[7], 1.0f);
}

TEST(SurfacePixelConversionTest, RejectsInvalidDimensionsPointersAndPrimaries)
{
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    constexpr std::array<float, 4> source = {0.1f, 0.2f, 0.3f, 1.0f};

    EXPECT_FALSE(convertSurfacePixels(
        source.data(), nullptr, 0, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight).isValid());
    EXPECT_FALSE(convertSurfacePixels(
        source.data(), nullptr, 1, -1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight).isValid());
    EXPECT_FALSE(convertSurfacePixels(
        nullptr, nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight).isValid());

    auto unsupportedChannels = descriptor;
    unsupportedChannels.channelOrder = SurfaceChannelOrder::RGB;
    EXPECT_FALSE(convertSurfacePixels(
        source.data(), nullptr, 1, 1, unsupportedChannels,
        SurfacePixelTarget::Rgba32LinearStraight).isValid());
    unsupportedChannels.channelOrder = SurfaceChannelOrder::Gray;
    EXPECT_FALSE(convertSurfacePixels(
        source.data(), nullptr, 1, 1, unsupportedChannels,
        SurfacePixelTarget::Rgba32LinearStraight).isValid());

    auto unsupportedPrimaries = descriptor;
    unsupportedPrimaries.primaries = SurfaceColorPrimaries::Rec2020_D65;
    EXPECT_FALSE(convertSurfacePixels(
        source.data(), nullptr, 1, 1, unsupportedPrimaries,
        SurfacePixelTarget::Rgba32LinearStraight).isValid());
}
