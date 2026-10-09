#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

import Graphics.SurfaceColorContract;
import Image.SurfacePixelConversion;
import Color.TransferFunction;

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

TEST(SurfacePixelConversionTest,
     UnknownTransferUsesLegacySrgbDecodeOnlyInsideNormalizedRange)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 12> source = {
        0.5f, 0.25f, 0.75f, 1.0f,
        -0.25f, 1.5f, 2.0f, 1.0f,
        nan, infinity, -infinity, 1.0f,
    };
    auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    descriptor.transferKnown = false;

    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 3, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);
    ASSERT_TRUE(converted.isValid());
    std::array<float, 12> actual{};
    std::memcpy(actual.data(), converted.bytes.data(), converted.bytes.size());

    const auto decodeLegacy = [](const float encoded) {
        if (encoded < 0.0f || encoded > 1.0f) return encoded;
        const double value = encoded;
        return static_cast<float>(value <= 0.04045
            ? value / 12.92
            : std::pow((value + 0.055) / 1.055, 2.4));
    };
    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(actual[channel], decodeLegacy(source[channel]), 1.0e-6f);
        EXPECT_FLOAT_EQ(actual[4 + channel], source[4 + channel]);
        EXPECT_FLOAT_EQ(actual[8 + channel], 0.0f);
    }
    EXPECT_FLOAT_EQ(actual[3], 1.0f);
    EXPECT_FLOAT_EQ(actual[7], 1.0f);
    EXPECT_FLOAT_EQ(actual[11], 1.0f);
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

TEST(SurfacePixelConversionTest, PremultipliedFloatInputConvertsConsistentlyToEveryTarget)
{
    constexpr std::array<float, 8> source = {
        0.1f, 0.05f, 0.025f, 0.25f,
        0.9f, 0.7f, 0.4f, 0.0f,
    };
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();

    const auto linear = convertSurfacePixels(
        source.data(), nullptr, 2, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);
    ASSERT_TRUE(linear.isValid());
    std::array<float, 8> linearPixels{};
    std::memcpy(linearPixels.data(), linear.bytes.data(), linear.bytes.size());
    EXPECT_NEAR(linearPixels[0], 0.4f, 1.0e-6f);
    EXPECT_NEAR(linearPixels[1], 0.2f, 1.0e-6f);
    EXPECT_NEAR(linearPixels[2], 0.1f, 1.0e-6f);
    EXPECT_FLOAT_EQ(linearPixels[3], 0.25f);
    EXPECT_FLOAT_EQ(linearPixels[4], 0.0f);
    EXPECT_FLOAT_EQ(linearPixels[5], 0.0f);
    EXPECT_FLOAT_EQ(linearPixels[6], 0.0f);
    EXPECT_FLOAT_EQ(linearPixels[7], 0.0f);

    const auto half = convertSurfacePixels(
        source.data(), nullptr, 2, 1, descriptor,
        SurfacePixelTarget::Rgba16LinearStraight);
    ASSERT_TRUE(half.isValid());
    EXPECT_EQ(half.descriptor, SurfaceColorDescriptor::linearStraightRgba16Float());
    std::array<std::uint16_t, 8> halfPixels{};
    std::memcpy(halfPixels.data(), half.bytes.data(), half.bytes.size());
    EXPECT_EQ(halfPixels[0], 0x3666u);
    EXPECT_EQ(halfPixels[1], 0x3266u);
    EXPECT_EQ(halfPixels[2], 0x2e66u);
    EXPECT_EQ(halfPixels[3], 0x3400u);
    EXPECT_EQ(halfPixels[4], 0x0000u);
    EXPECT_EQ(halfPixels[5], 0x0000u);
    EXPECT_EQ(halfPixels[6], 0x0000u);
    EXPECT_EQ(halfPixels[7], 0x0000u);

    const auto bytes = convertSurfacePixels(
        source.data(), nullptr, 2, 1, descriptor,
        SurfacePixelTarget::Rgba8SrgbStraight);
    ASSERT_TRUE(bytes.isValid());
    EXPECT_EQ(bytes.descriptor, SurfaceColorDescriptor::encodedSrgbRgba8Straight());
    constexpr std::array<std::uint8_t, 8> expectedBytes = {
        170, 124, 89, 64,
        0, 0, 0, 0,
    };
    EXPECT_EQ(bytes.bytes.size(), expectedBytes.size());
    for (std::size_t index = 0; index < expectedBytes.size(); ++index) {
        EXPECT_EQ(bytes.bytes[index], expectedBytes[index]) << "channel " << index;
    }
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

TEST(SurfacePixelConversionTest,
     OpaqueFloatInputForcesFullAlphaForEveryOutputTarget)
{
    const std::array<float, 4> source = {
        0.2f, 0.4f, 0.6f, std::numeric_limits<float>::quiet_NaN()};
    auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    descriptor.alphaMode = SurfaceAlphaMode::Opaque;

    const auto floatOutput = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);
    ASSERT_TRUE(floatOutput.isValid());
    std::array<float, 4> floats{};
    std::memcpy(floats.data(), floatOutput.bytes.data(), floatOutput.bytes.size());
    EXPECT_FLOAT_EQ(floats[0], source[0]);
    EXPECT_FLOAT_EQ(floats[1], source[1]);
    EXPECT_FLOAT_EQ(floats[2], source[2]);
    EXPECT_FLOAT_EQ(floats[3], 1.0f);

    const auto halfOutput = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba16LinearStraight);
    ASSERT_TRUE(halfOutput.isValid());
    std::array<std::uint16_t, 4> half{};
    std::memcpy(half.data(), halfOutput.bytes.data(), halfOutput.bytes.size());
    EXPECT_EQ(half, (std::array<std::uint16_t, 4>{0x3266u, 0x3666u, 0x38CDu, 0x3C00u}));

    const auto byteOutput = convertSurfacePixels(
        source.data(), nullptr, 1, 1, descriptor,
        SurfacePixelTarget::Rgba8SrgbStraight);
    ASSERT_TRUE(byteOutput.isValid());
    EXPECT_EQ(byteOutput.descriptor,
              SurfaceColorDescriptor::encodedSrgbRgba8Straight());
    EXPECT_EQ(byteOutput.bytes,
              (std::vector<std::uint8_t>{124u, 170u, 203u, 255u}));
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

TEST(SurfacePixelConversionTest, ClampsOutOfRangeLinearChannelsWhenEncodingBytes)
{
    constexpr std::array<float, 12> source = {
        -0.25f, 0.0f, 1.0f, 0.5f,
        0.5f, 2.0f, 0.25f, 1.5f,
        0.25f, 0.5f, 0.75f, -0.5f,
    };
    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 3, 1,
        SurfaceColorDescriptor::linearStraightRgba32Float(),
        SurfacePixelTarget::Rgba8SrgbStraight);

    ASSERT_TRUE(converted.isValid());
    ASSERT_EQ(converted.bytes.size(), 12u);
    EXPECT_EQ(converted.bytes[0], 0u);
    EXPECT_EQ(converted.bytes[1], 0u);
    EXPECT_EQ(converted.bytes[2], 255u);
    EXPECT_EQ(converted.bytes[3], 128u);
    EXPECT_EQ(converted.bytes[4], 188u);
    EXPECT_EQ(converted.bytes[5], 255u);
    EXPECT_EQ(converted.bytes[6], 137u);
    EXPECT_EQ(converted.bytes[7], 255u);
    EXPECT_EQ(converted.bytes[8], 0u);
    EXPECT_EQ(converted.bytes[9], 0u);
    EXPECT_EQ(converted.bytes[10], 0u);
    EXPECT_EQ(converted.bytes[11], 0u);
}

TEST(SurfacePixelConversionTest, AlphaByteQuantizationChangesAcrossHalfCodeBoundary)
{
    const float justBelowHalf = std::nextafter(0.5f, 0.0f);
    const float justAboveHalf = std::nextafter(0.5f, 1.0f);
    const std::array<float, 12> source = {
        0.0f, 0.0f, 0.0f, justBelowHalf,
        0.0f, 0.0f, 0.0f, 0.5f,
        0.0f, 0.0f, 0.0f, justAboveHalf,
    };
    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 3, 1,
        SurfaceColorDescriptor::linearStraightRgba32Float(),
        SurfacePixelTarget::Rgba8SrgbStraight);

    ASSERT_TRUE(converted.isValid());
    EXPECT_EQ(converted.bytes[3], 127u);
    EXPECT_EQ(converted.bytes[7], 128u);
    EXPECT_EQ(converted.bytes[11], 128u);
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

TEST(SurfacePixelConversionTest, HalfFloatEncodingRoundsAtNormalAndSubnormalBoundaries)
{
    constexpr float normalMidpoint = 1.00048828125f;
    constexpr float halfSubnormalMidpoint = 0x1p-25f;
    const std::array<float, 12> source = {
        std::nextafter(normalMidpoint, 0.0f), normalMidpoint,
        std::nextafter(normalMidpoint, 2.0f), 1.0f,
        std::nextafter(halfSubnormalMidpoint, 0.0f), halfSubnormalMidpoint,
        0x1p-24f, 1.0f,
        65504.0f, 65520.0f, -0.0f, 1.0f};
    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 3, 1,
        SurfaceColorDescriptor::linearStraightRgba32Float(),
        SurfacePixelTarget::Rgba16LinearStraight);

    ASSERT_TRUE(converted.isValid());
    ASSERT_EQ(converted.bytes.size(), 24u);
    std::array<std::uint16_t, 12> half{};
    std::memcpy(half.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_EQ(half[0], 0x3c00u);
    EXPECT_EQ(half[1], 0x3c01u);
    EXPECT_EQ(half[2], 0x3c01u);
    EXPECT_EQ(half[3], 0x3c00u);
    EXPECT_EQ(half[4], 0x0000u);
    EXPECT_EQ(half[5], 0x0001u);
    EXPECT_EQ(half[6], 0x0001u);
    EXPECT_EQ(half[7], 0x3c00u);
    EXPECT_EQ(half[8], 0x7bffu);
    EXPECT_EQ(half[9], 0x7c00u);
    EXPECT_EQ(half[10], 0x8000u);
    EXPECT_EQ(half[11], 0x3c00u);
}

TEST(SurfacePixelConversionTest, EveryHalfFloatBitPatternRoundTripsThroughFloatTarget)
{
    const auto decodeHalfReference = [](const std::uint16_t bits) {
        const bool negative = (bits & 0x8000u) != 0;
        const std::uint16_t exponent = (bits >> 10u) & 0x1fu;
        const std::uint16_t mantissa = bits & 0x03ffu;
        double value = 0.0;
        if (exponent == 0) {
            value = std::ldexp(static_cast<double>(mantissa), -24);
        } else if (exponent == 0x1fu) {
            value = mantissa == 0
                ? std::numeric_limits<double>::infinity()
                : std::numeric_limits<double>::quiet_NaN();
        } else {
            value = std::ldexp(1.0 + static_cast<double>(mantissa) / 1024.0,
                               static_cast<int>(exponent) - 15);
        }
        return negative ? -value : value;
    };

    std::array<float, 4> source{};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    for (std::uint32_t bits = 0; bits <= 0xffffu; ++bits) {
        const auto half = static_cast<std::uint16_t>(bits);
        source[0] = static_cast<float>(decodeHalfReference(half));
        source[1] = 0.25f;
        source[2] = 0.5f;
        source[3] = 1.0f;

        const auto converted = convertSurfacePixels(
            source.data(), nullptr, 1, 1, descriptor,
            SurfacePixelTarget::Rgba16LinearStraight);
        ASSERT_TRUE(converted.isValid()) << "half bits 0x" << std::hex << bits;
        std::uint16_t result = 0;
        std::memcpy(&result, converted.bytes.data(), sizeof(result));

        const std::uint16_t exponent = (half >> 10u) & 0x1fu;
        if (exponent == 0x1fu) {
            EXPECT_EQ(result, 0u) << "half bits 0x" << std::hex << bits;
        } else {
            EXPECT_EQ(result, half) << "half bits 0x" << std::hex << bits;
        }
    }
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

TEST(SurfacePixelConversionTest, DecodesSrgbByteInputToLinearFloat)
{
    constexpr std::array<std::uint8_t, 4> source = {128, 64, 32, 128};
    const auto converted = convertSurfacePixels(
        nullptr, source.data(), 1, 1,
        SurfaceColorDescriptor::encodedSrgbRgba8Straight(),
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 4> pixel{};
    std::memcpy(pixel.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_NEAR(pixel[0], 0.2158605f, 1.0e-6f);
    EXPECT_NEAR(pixel[1], 0.0512695f, 1.0e-6f);
    EXPECT_NEAR(pixel[2], 0.0144438f, 1.0e-6f);
    EXPECT_NEAR(pixel[3], 128.0f / 255.0f, 1.0e-6f);
}

TEST(SurfacePixelConversionTest, ByteBgraInputReordersAndRoundTripsThroughTargets)
{
    constexpr std::array<std::uint8_t, 8> source = {
        32, 64, 128, 128,
        10, 20, 240, 255,
    };
    auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;

    const auto linear = convertSurfacePixels(
        nullptr, source.data(), 2, 1, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);
    ASSERT_TRUE(linear.isValid());
    ASSERT_EQ(linear.rowStride, sizeof(float) * 8u);
    std::array<float, 8> linearPixels{};
    std::memcpy(linearPixels.data(), linear.bytes.data(), linear.bytes.size());
    EXPECT_NEAR(linearPixels[0], ColorTransferFunction::srgbToLinear(128.0f / 255.0f), 1e-6f);
    EXPECT_NEAR(linearPixels[1], ColorTransferFunction::srgbToLinear(64.0f / 255.0f), 1e-6f);
    EXPECT_NEAR(linearPixels[2], ColorTransferFunction::srgbToLinear(32.0f / 255.0f), 1e-6f);
    EXPECT_NEAR(linearPixels[3], 128.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(linearPixels[4], ColorTransferFunction::srgbToLinear(240.0f / 255.0f), 1e-6f);
    EXPECT_NEAR(linearPixels[5], ColorTransferFunction::srgbToLinear(20.0f / 255.0f), 1e-6f);
    EXPECT_NEAR(linearPixels[6], ColorTransferFunction::srgbToLinear(10.0f / 255.0f), 1e-6f);
    EXPECT_FLOAT_EQ(linearPixels[7], 1.0f);

    const auto encoded = convertSurfacePixels(
        nullptr, source.data(), 2, 1, descriptor,
        SurfacePixelTarget::Rgba8SrgbStraight);
    ASSERT_TRUE(encoded.isValid());
    EXPECT_EQ(encoded.descriptor, SurfaceColorDescriptor::encodedSrgbRgba8Straight());
    ASSERT_EQ(encoded.bytes.size(), source.size());
    EXPECT_EQ(encoded.bytes[0], 128u);
    EXPECT_EQ(encoded.bytes[1], 64u);
    EXPECT_EQ(encoded.bytes[2], 32u);
    EXPECT_EQ(encoded.bytes[3], 128u);
    EXPECT_EQ(encoded.bytes[4], 240u);
    EXPECT_EQ(encoded.bytes[5], 20u);
    EXPECT_EQ(encoded.bytes[6], 10u);
    EXPECT_EQ(encoded.bytes[7], 255u);
}

TEST(SurfacePixelConversionTest,
     MultiRowBgraBytesPreservePixelOrderAndLinearRgbaRowStride)
{
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr std::array<std::uint8_t, width * height * 4> source = {
        32u, 64u, 128u, 128u,  10u, 20u, 240u, 255u,  250u, 140u, 30u, 0u,
        0u, 255u, 16u, 64u,    180u, 90u, 45u, 192u, 1u, 2u, 3u, 255u,
    };
    auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    const auto converted = convertSurfacePixels(
        nullptr, source.data(), width, height, descriptor,
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    EXPECT_EQ(converted.width, width);
    EXPECT_EQ(converted.height, height);
    EXPECT_EQ(converted.rowStride, width * 4u * sizeof(float));
    EXPECT_EQ(converted.bytes.size(),
              static_cast<std::size_t>(width * height * 4) * sizeof(float));
    EXPECT_EQ(converted.descriptor,
              SurfaceColorDescriptor::linearStraightRgba32Float());

    std::array<float, width * height * 4> actual{};
    std::memcpy(actual.data(), converted.bytes.data(), converted.bytes.size());
    const auto decodeSrgb = [](const std::uint8_t code) {
        const double encoded = static_cast<double>(code) / 255.0;
        return encoded <= 0.04045
            ? encoded / 12.92
            : std::pow((encoded + 0.055) / 1.055, 2.4);
    };

    for (int pixel = 0; pixel < width * height; ++pixel) {
        const std::size_t input = static_cast<std::size_t>(pixel) * 4u;
        const std::size_t output = input;
        const float alpha = static_cast<float>(source[input + 3u]) / 255.0f;
        const std::array<double, 3> expected = alpha <= 1.0e-6f
            ? std::array<double, 3>{0.0, 0.0, 0.0}
            : std::array<double, 3>{decodeSrgb(source[input + 2u]),
                                    decodeSrgb(source[input + 1u]),
                                    decodeSrgb(source[input])};
        SCOPED_TRACE(pixel);
        EXPECT_NEAR(actual[output], expected[0], 1.0e-6);
        EXPECT_NEAR(actual[output + 1u], expected[1], 1.0e-6);
        EXPECT_NEAR(actual[output + 2u], expected[2], 1.0e-6);
        EXPECT_FLOAT_EQ(actual[output + 3u], alpha);
    }
}

TEST(SurfacePixelConversionTest, EquivalentSrgbByteAndFloatInputsMatchAcrossTargets)
{
    constexpr std::array<std::uint8_t, 12> rgbaBytes = {
        0u, 1u, 127u, 255u,
        128u, 64u, 32u, 173u,
        255u, 240u, 10u, 0u,
    };
    std::array<float, rgbaBytes.size()> rgbaFloats{};
    for (std::size_t index = 0; index < rgbaBytes.size(); ++index) {
        rgbaFloats[index] = static_cast<float>(rgbaBytes[index]) / 255.0f;
    }

    constexpr SurfacePixelTarget targets[] = {
        SurfacePixelTarget::Rgba8SrgbStraight,
        SurfacePixelTarget::Rgba16LinearStraight,
        SurfacePixelTarget::Rgba32LinearStraight,
    };
    for (const SurfaceChannelOrder order : {
             SurfaceChannelOrder::RGBA, SurfaceChannelOrder::BGRA}) {
        auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
        descriptor.channelOrder = order;
        std::array<std::uint8_t, rgbaBytes.size()> orderedBytes{};
        std::array<float, rgbaFloats.size()> orderedFloats{};
        for (std::size_t pixel = 0; pixel < rgbaBytes.size() / 4u; ++pixel) {
            const std::size_t offset = pixel * 4u;
            const std::size_t redIndex = order == SurfaceChannelOrder::RGBA ? 0u : 2u;
            const std::size_t blueIndex = order == SurfaceChannelOrder::RGBA ? 2u : 0u;
            orderedBytes[offset + redIndex] = rgbaBytes[offset];
            orderedBytes[offset + 1u] = rgbaBytes[offset + 1u];
            orderedBytes[offset + blueIndex] = rgbaBytes[offset + 2u];
            orderedBytes[offset + 3u] = rgbaBytes[offset + 3u];
            orderedFloats[offset + redIndex] = rgbaFloats[offset];
            orderedFloats[offset + 1u] = rgbaFloats[offset + 1u];
            orderedFloats[offset + blueIndex] = rgbaFloats[offset + 2u];
            orderedFloats[offset + 3u] = rgbaFloats[offset + 3u];
        }

        for (const SurfacePixelTarget target : targets) {
            const auto fromBytes = convertSurfacePixels(
                nullptr, orderedBytes.data(), 3, 1, descriptor, target);
            const auto fromFloats = convertSurfacePixels(
                orderedFloats.data(), nullptr, 3, 1, descriptor, target);
            ASSERT_TRUE(fromBytes.isValid());
            ASSERT_TRUE(fromFloats.isValid());
            ASSERT_EQ(fromBytes.descriptor, fromFloats.descriptor);
            ASSERT_EQ(fromBytes.bytes.size(), fromFloats.bytes.size());
            SCOPED_TRACE(::testing::Message()
                << "order=" << static_cast<int>(order)
                << " target=" << static_cast<int>(target));
            for (std::size_t index = 0; index < fromBytes.bytes.size(); ++index) {
                EXPECT_EQ(fromBytes.bytes[index], fromFloats.bytes[index])
                    << "byte offset " << index;
            }
        }
    }
}

TEST(SurfacePixelConversionTest, PremultipliedSrgbBytesUnpremultiplyBeforeTransferDecode)
{
    constexpr std::uint8_t straightCodes[] = {200u, 100u, 50u};
    constexpr int pixelCount = 256;
    const auto decodeSrgbReference = [](const double encoded) {
        return encoded <= 0.04045
            ? encoded / 12.92
            : std::pow((encoded + 0.055) / 1.055, 2.4);
    };

    for (const SurfaceChannelOrder order : {
             SurfaceChannelOrder::RGBA, SurfaceChannelOrder::BGRA}) {
        std::array<std::uint8_t, pixelCount * 4> source{};
        for (int alpha = 0; alpha < pixelCount; ++alpha) {
            const std::uint8_t red = static_cast<std::uint8_t>(
                (static_cast<int>(straightCodes[0]) * alpha + 127) / 255);
            const std::uint8_t green = static_cast<std::uint8_t>(
                (static_cast<int>(straightCodes[1]) * alpha + 127) / 255);
            const std::uint8_t blue = static_cast<std::uint8_t>(
                (static_cast<int>(straightCodes[2]) * alpha + 127) / 255);
            const std::size_t offset = static_cast<std::size_t>(alpha) * 4u;
            source[offset] = order == SurfaceChannelOrder::RGBA ? red : blue;
            source[offset + 1] = green;
            source[offset + 2] = order == SurfaceChannelOrder::RGBA ? blue : red;
            source[offset + 3] = static_cast<std::uint8_t>(alpha);
        }

        auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Premultiplied();
        descriptor.channelOrder = order;
        const auto converted = convertSurfacePixels(
            nullptr, source.data(), pixelCount, 1, descriptor,
            SurfacePixelTarget::Rgba32LinearStraight);
        ASSERT_TRUE(converted.isValid());
        ASSERT_EQ(converted.bytes.size(), pixelCount * 4u * sizeof(float));
        std::array<float, pixelCount * 4> pixels{};
        std::memcpy(pixels.data(), converted.bytes.data(), converted.bytes.size());

        for (int alpha = 0; alpha < pixelCount; ++alpha) {
            const std::size_t offset = static_cast<std::size_t>(alpha) * 4u;
            const auto expectedLinear = [alpha, &decodeSrgbReference](const std::uint8_t code) {
                if (alpha == 0) return 0.0;
                const double unpremultiplied = static_cast<double>(code) / alpha;
                return decodeSrgbReference(unpremultiplied);
            };
            const std::uint8_t redCode = order == SurfaceChannelOrder::RGBA
                ? source[offset] : source[offset + 2];
            const std::uint8_t blueCode = order == SurfaceChannelOrder::RGBA
                ? source[offset + 2] : source[offset];

            SCOPED_TRACE(::testing::Message()
                << "order=" << static_cast<int>(order) << " alphaCode=" << alpha);
            EXPECT_NEAR(pixels[offset], expectedLinear(redCode), 1.0e-6f);
            EXPECT_NEAR(pixels[offset + 1], expectedLinear(source[offset + 1]), 1.0e-6f);
            EXPECT_NEAR(pixels[offset + 2], expectedLinear(blueCode), 1.0e-6f);
            EXPECT_FLOAT_EQ(pixels[offset + 3], alpha / 255.0f);
        }
    }
}

TEST(SurfacePixelConversionTest, EverySrgbRgbaByteCodeRoundTripsExactly)
{
    std::array<std::uint8_t, 512 * 4> source{};
    for (std::size_t code = 0; code < 256; ++code) {
        source[code * 4] = static_cast<std::uint8_t>(code);
        source[code * 4 + 1] = static_cast<std::uint8_t>(255u - code);
        source[code * 4 + 2] = static_cast<std::uint8_t>(code);
        source[code * 4 + 3] = 255u;

        const std::size_t alphaPixel = 256u + code;
        source[alphaPixel * 4 + 3] = static_cast<std::uint8_t>(code);
    }

    const auto converted = convertSurfacePixels(
        nullptr, source.data(), 512, 1,
        SurfaceColorDescriptor::encodedSrgbRgba8Straight(),
        SurfacePixelTarget::Rgba8SrgbStraight);

    ASSERT_TRUE(converted.isValid());
    ASSERT_EQ(converted.bytes.size(), source.size());
    for (std::size_t index = 0; index < source.size(); ++index) {
        EXPECT_EQ(converted.bytes[index], source[index]) << "byte index " << index;
    }
}

TEST(SurfacePixelConversionTest, EverySrgbAlphaByteCodePreservesColoredStraightPixels)
{
    std::array<std::uint8_t, 256 * 4> source{};
    for (std::size_t alpha = 0; alpha < 256; ++alpha) {
        source[alpha * 4] = static_cast<std::uint8_t>(alpha);
        source[alpha * 4 + 1] = static_cast<std::uint8_t>(255u - alpha);
        source[alpha * 4 + 2] = static_cast<std::uint8_t>((alpha * 73u) & 0xffu);
        source[alpha * 4 + 3] = static_cast<std::uint8_t>(alpha);
    }

    const auto converted = convertSurfacePixels(
        nullptr, source.data(), 256, 1,
        SurfaceColorDescriptor::encodedSrgbRgba8Straight(),
        SurfacePixelTarget::Rgba8SrgbStraight);

    ASSERT_TRUE(converted.isValid());
    ASSERT_EQ(converted.bytes.size(), source.size());
    for (std::size_t alpha = 0; alpha < 256; ++alpha) {
        const std::size_t offset = alpha * 4;
        if (alpha == 0) {
            EXPECT_EQ(converted.bytes[offset], 0u);
            EXPECT_EQ(converted.bytes[offset + 1], 0u);
            EXPECT_EQ(converted.bytes[offset + 2], 0u);
        } else {
            EXPECT_EQ(converted.bytes[offset], source[offset]) << "alpha code " << alpha;
            EXPECT_EQ(converted.bytes[offset + 1], source[offset + 1]) << "alpha code " << alpha;
            EXPECT_EQ(converted.bytes[offset + 2], source[offset + 2]) << "alpha code " << alpha;
        }
        EXPECT_EQ(converted.bytes[offset + 3], source[offset + 3])
            << "alpha code " << alpha;
    }
}

TEST(SurfacePixelConversionTest, PremultipliedAlphaThresholdZerosRgbButKeepsAlpha)
{
    constexpr std::array<float, 8> source = {
        1.25e-7f, 6.25e-8f, 3.125e-8f, 5.0e-7f,
        5.0e-7f, 2.5e-7f, 1.25e-7f, 2.0e-6f};
    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 2, 1,
        SurfaceColorDescriptor::canonicalLinearPremultiplied(),
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 8> pixels{};
    std::memcpy(pixels.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_FLOAT_EQ(pixels[0], 0.0f);
    EXPECT_FLOAT_EQ(pixels[1], 0.0f);
    EXPECT_FLOAT_EQ(pixels[2], 0.0f);
    EXPECT_FLOAT_EQ(pixels[3], 5.0e-7f);
    EXPECT_NEAR(pixels[4], 0.25f, 1.0e-6f);
    EXPECT_NEAR(pixels[5], 0.125f, 1.0e-6f);
    EXPECT_NEAR(pixels[6], 0.0625f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[7], 2.0e-6f);
}

TEST(SurfacePixelConversionTest, PremultipliedAlphaThresholdUsesStrictGreaterThan)
{
    constexpr float threshold = 1.0e-6f;
    const std::array<float, 12> source = {
        0.5f * std::nextafter(threshold, 0.0f),
        0.25f * std::nextafter(threshold, 0.0f),
        0.125f * std::nextafter(threshold, 0.0f),
        std::nextafter(threshold, 0.0f),
        0.5f * threshold, 0.25f * threshold, 0.125f * threshold, threshold,
        0.5f * std::nextafter(threshold, 1.0f),
        0.25f * std::nextafter(threshold, 1.0f),
        0.125f * std::nextafter(threshold, 1.0f),
        std::nextafter(threshold, 1.0f),
    };
    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 3, 1,
        SurfaceColorDescriptor::canonicalLinearPremultiplied(),
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 12> pixels{};
    std::memcpy(pixels.data(), converted.bytes.data(), converted.bytes.size());
    for (int pixel = 0; pixel < 2; ++pixel) {
        EXPECT_FLOAT_EQ(pixels[pixel * 4], 0.0f);
        EXPECT_FLOAT_EQ(pixels[pixel * 4 + 1], 0.0f);
        EXPECT_FLOAT_EQ(pixels[pixel * 4 + 2], 0.0f);
        EXPECT_FLOAT_EQ(pixels[pixel * 4 + 3], source[pixel * 4 + 3]);
    }
    EXPECT_NEAR(pixels[8], 0.5f, 1e-6f);
    EXPECT_NEAR(pixels[9], 0.25f, 1e-6f);
    EXPECT_NEAR(pixels[10], 0.125f, 1e-6f);
    EXPECT_FLOAT_EQ(pixels[11], source[11]);
}

TEST(SurfacePixelConversionTest, FloatTargetPreservesHdrAndNegativeLinearRgb)
{
    constexpr std::array<float, 4> source = {-0.25f, 0.5f, 2.0f, 1.0f};
    const auto converted = convertSurfacePixels(
        source.data(), nullptr, 1, 1,
        SurfaceColorDescriptor::linearStraightRgba32Float(),
        SurfacePixelTarget::Rgba32LinearStraight);

    ASSERT_TRUE(converted.isValid());
    std::array<float, 4> pixel{};
    std::memcpy(pixel.data(), converted.bytes.data(), converted.bytes.size());
    EXPECT_FLOAT_EQ(pixel[0], -0.25f);
    EXPECT_FLOAT_EQ(pixel[1], 0.5f);
    EXPECT_FLOAT_EQ(pixel[2], 2.0f);
    EXPECT_FLOAT_EQ(pixel[3], 1.0f);
}
