#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

import Color.GamutConversion;
import Color.TransferFunction;

using namespace ArtifactCore;

namespace {

constexpr std::array<std::array<double, 3>, 3> kRec709ToXyz = {{
    {{0.4123908, 0.3575843, 0.1804808}},
    {{0.2126390, 0.7151687, 0.0721923}},
    {{0.0193308, 0.1191950, 0.9505321}},
}};
constexpr std::array<std::array<double, 3>, 3> kXyzToRec2020 = {{
    {{1.7166512, -0.3556708, -0.2533663}},
    {{-0.6666844, 1.6164812, 0.0157685}},
    {{0.0176399, -0.0427706, 0.9421031}},
}};
constexpr std::array<std::array<double, 3>, 3> kRec2020ToXyz = {{
    {{0.6369580, 0.1446169, 0.1688810}},
    {{0.2627002, 0.6779981, 0.0593017}},
    {{0.0, 0.0280727, 1.0609851}},
}};
constexpr std::array<std::array<double, 3>, 3> kXyzToRec709 = {{
    {{3.2409699, -1.5373832, -0.4986108}},
    {{-0.9692436, 1.8759675, 0.0415551}},
    {{0.0556301, -0.2039770, 1.0569715}},
}};

std::array<double, 3> applyMatrix(
    const std::array<std::array<double, 3>, 3>& matrix,
    const std::array<double, 3>& rgb)
{
    return {
        matrix[0][0] * rgb[0] + matrix[0][1] * rgb[1] + matrix[0][2] * rgb[2],
        matrix[1][0] * rgb[0] + matrix[1][1] * rgb[1] + matrix[1][2] * rgb[2],
        matrix[2][0] * rgb[0] + matrix[2][1] * rgb[1] + matrix[2][2] * rgb[2],
    };
}

double decodeSrgb(const double encoded)
{
    const double value = std::max(encoded, 0.0);
    return value <= 0.04045
        ? value / 12.92
        : std::pow((value + 0.055) / 1.055, 2.4);
}

double encodeSrgb(const double linear)
{
    const double value = std::max(linear, 0.0);
    return value <= 0.0031308
        ? 12.92 * value
        : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
}

double decodeRec2020(const double encoded)
{
    constexpr double alpha = 1.09929682680944;
    constexpr double breakpoint = 0.081242858298635;
    const double value = std::max(encoded, 0.0);
    return value < breakpoint
        ? value / 4.5
        : std::pow((value + alpha - 1.0) / alpha, 1.0 / 0.45);
}

double encodeRec2020(const double linear)
{
    constexpr double alpha = 1.09929682680944;
    constexpr double breakpoint = 0.018053968510807;
    const double value = std::max(linear, 0.0);
    return value < breakpoint
        ? 4.5 * value
        : alpha * std::pow(value, 0.45) - (alpha - 1.0);
}

double decodePq(const double encoded)
{
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    const double value = std::max(encoded, 0.0);
    if (value == 0.0) {
        return 0.0;
    }
    const double powered = std::pow(value, 1.0 / m2);
    const double numerator = std::max(powered - c1, 0.0);
    const double denominator = c2 - c3 * powered;
    return denominator <= 0.0 ? 0.0 : std::pow(numerator / denominator, 1.0 / m1);
}

double encodePq(const double linear)
{
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    const double value = std::max(linear, 0.0);
    if (value == 0.0) {
        return 0.0;
    }
    const double powered = std::pow(value, m1);
    return std::pow((c1 + c2 * powered) / (1.0 + c3 * powered), m2);
}

std::array<double, 3> convertSrgbToRec2020Reference(
    const std::array<double, 3>& encoded)
{
    const std::array<double, 3> linear = {
        decodeSrgb(encoded[0]), decodeSrgb(encoded[1]), decodeSrgb(encoded[2])};
    return {
        encodeRec2020(applyMatrix(kXyzToRec2020,
                                  applyMatrix(kRec709ToXyz, linear))[0]),
        encodeRec2020(applyMatrix(kXyzToRec2020,
                                  applyMatrix(kRec709ToXyz, linear))[1]),
        encodeRec2020(applyMatrix(kXyzToRec2020,
                                  applyMatrix(kRec709ToXyz, linear))[2]),
    };
}

std::array<double, 3> convertRec2020ToSrgbReference(
    const std::array<double, 3>& encoded)
{
    const std::array<double, 3> linear = {
        decodeRec2020(encoded[0]), decodeRec2020(encoded[1]), decodeRec2020(encoded[2])};
    const auto rec709 = applyMatrix(kXyzToRec709,
                                    applyMatrix(kRec2020ToXyz, linear));
    return {encodeSrgb(rec709[0]), encodeSrgb(rec709[1]), encodeSrgb(rec709[2])};
}

} // namespace

TEST(ColorPipelineStandaloneTest,
     SrgbDecodeGamutConvertAndRec2020EncodeMatchIndependentReference)
{
    constexpr std::array<double, 8> samples = {
        -0.1, 0.0, 0.0031308, 0.04045, 0.18, 0.5, 1.0, 1.25,
    };

    for (const double red : samples) {
        for (const double green : samples) {
            for (const double blue : samples) {
                const std::array<double, 3> source = {red, green, blue};
                const auto expected = convertSrgbToRec2020Reference(source);
                const std::array<float, 3> linear = {
                    ColorTransferFunction::decode(static_cast<float>(red), TransferFunction::sRGB),
                    ColorTransferFunction::decode(static_cast<float>(green), TransferFunction::sRGB),
                    ColorTransferFunction::decode(static_cast<float>(blue), TransferFunction::sRGB),
                };
                std::array<float, 3> rec2020Linear{};
                ColorGamutConversion::convert(
                    linear[0], linear[1], linear[2], Gamut::sRGB, Gamut::Rec2020,
                    rec2020Linear[0], rec2020Linear[1], rec2020Linear[2]);
                const std::array<float, 3> actual = {
                    ColorTransferFunction::encode(rec2020Linear[0], TransferFunction::Rec2020_10),
                    ColorTransferFunction::encode(rec2020Linear[1], TransferFunction::Rec2020_10),
                    ColorTransferFunction::encode(rec2020Linear[2], TransferFunction::Rec2020_10),
                };

                SCOPED_TRACE(::testing::Message()
                    << "source sRGB=" << red << ',' << green << ',' << blue);
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    EXPECT_NEAR(actual[channel], expected[channel], 2.0e-5);
                }
            }
        }
    }
}

TEST(ColorPipelineStandaloneTest,
     Rec2020DecodeGamutConvertAndSrgbEncodeMatchIndependentReference)
{
    constexpr std::array<double, 8> samples = {
        -0.1, 0.0, 0.018053968510807, 0.081242858298635,
        0.18, 0.5, 1.0, 1.25,
    };

    for (const double red : samples) {
        for (const double green : samples) {
            for (const double blue : samples) {
                const std::array<double, 3> source = {red, green, blue};
                const auto expected = convertRec2020ToSrgbReference(source);
                const std::array<float, 3> linear = {
                    ColorTransferFunction::decode(static_cast<float>(red), TransferFunction::Rec2020_10),
                    ColorTransferFunction::decode(static_cast<float>(green), TransferFunction::Rec2020_10),
                    ColorTransferFunction::decode(static_cast<float>(blue), TransferFunction::Rec2020_10),
                };
                std::array<float, 3> rec709Linear{};
                ColorGamutConversion::convert(
                    linear[0], linear[1], linear[2], Gamut::Rec2020, Gamut::sRGB,
                    rec709Linear[0], rec709Linear[1], rec709Linear[2]);
                const std::array<float, 3> actual = {
                    ColorTransferFunction::encode(rec709Linear[0], TransferFunction::sRGB),
                    ColorTransferFunction::encode(rec709Linear[1], TransferFunction::sRGB),
                    ColorTransferFunction::encode(rec709Linear[2], TransferFunction::sRGB),
                };

                SCOPED_TRACE(::testing::Message()
                    << "source Rec.2020=" << red << ',' << green << ',' << blue);
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    EXPECT_NEAR(actual[channel], expected[channel], 3.0e-5);
                }
            }
        }
    }
}

TEST(ColorPipelineStandaloneTest,
     Full16BitNeutralSrgbRampRemainsNeutralAndMonotonicInRec2020)
{
    std::array<float, 3> previous = {-1.0f, -1.0f, -1.0f};

    for (std::uint32_t code = 0; code <= 65535; ++code) {
        const double encoded = static_cast<double>(code) / 65535.0;
        const std::array<double, 3> source = {encoded, encoded, encoded};
        const auto expected = convertSrgbToRec2020Reference(source);

        const float linear = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::sRGB);
        std::array<float, 3> convertedLinear{};
        ColorGamutConversion::convert(
            linear, linear, linear, Gamut::sRGB, Gamut::Rec2020,
            convertedLinear[0], convertedLinear[1], convertedLinear[2]);
        const std::array<float, 3> actual = {
            ColorTransferFunction::encode(
                convertedLinear[0], TransferFunction::Rec2020_10),
            ColorTransferFunction::encode(
                convertedLinear[1], TransferFunction::Rec2020_10),
            ColorTransferFunction::encode(
                convertedLinear[2], TransferFunction::Rec2020_10),
        };

        SCOPED_TRACE(::testing::Message() << "sRGB code=" << code);
        for (std::size_t channel = 0; channel < actual.size(); ++channel) {
            EXPECT_NEAR(actual[channel], expected[channel], 2.0e-5);
            EXPECT_GE(actual[channel], previous[channel]);
        }
        EXPECT_NEAR(actual[0], actual[1], 2.0e-6f);
        EXPECT_NEAR(actual[1], actual[2], 2.0e-6f);
        previous = actual;
    }
}

TEST(ColorPipelineStandaloneTest, Rec2020PqEncodeAndDecodeMatchIndependentReference)
{
    constexpr std::array<double, 12> linearSamples = {
        -0.1, 0.0, 1.0e-6, 1.0e-4, 0.001, 0.01,
        0.18, 0.5, 1.0, 4.0, 10.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::Rec2084_PQ);
        const double expectedEncoded = encodePq(linear);
        SCOPED_TRACE(::testing::Message() << "linear Rec.2020=" << linear);
        EXPECT_NEAR(encoded, expectedEncoded, std::max(3.0e-6, expectedEncoded * 1.0e-5));

        const float decoded = ColorTransferFunction::decode(
            static_cast<float>(expectedEncoded), TransferFunction::Rec2084_PQ);
        EXPECT_NEAR(decoded, decodePq(expectedEncoded), std::max(3.0e-6, linear * 1.0e-4));
        if (linear >= 0.0) {
            EXPECT_NEAR(decoded, linear, std::max(3.0e-6, linear * 1.0e-4));
        }
    }

    EXPECT_FLOAT_EQ(ColorTransferFunction::decode(-0.25f, TransferFunction::Rec2084_PQ), 0.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-0.25f, TransferFunction::Rec2084_PQ), 0.0f);
}
