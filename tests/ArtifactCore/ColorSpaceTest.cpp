#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>

import Color.ColorSpace;

using namespace ArtifactCore;

namespace {

constexpr float kMatrixEpsilon = 2.0e-4f;
constexpr float kTransferEpsilon = 2.0e-5f;

std::array<float, 3> applyRgbMatrix(const std::array<float, 16>& matrix,
                                    const std::array<float, 3>& rgb)
{
    return {
        matrix[0] * rgb[0] + matrix[1] * rgb[1] + matrix[2] * rgb[2],
        matrix[4] * rgb[0] + matrix[5] * rgb[1] + matrix[6] * rgb[2],
        matrix[8] * rgb[0] + matrix[9] * rgb[1] + matrix[10] * rgb[2],
    };
}

void expectRgbNear(const std::array<float, 3>& actual,
                   const std::array<float, 3>& expected,
                   float tolerance)
{
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_NEAR(actual[i], expected[i], tolerance) << "channel " << i;
    }
}

} // namespace

TEST(ColorSpaceTest, SameSpaceMatrixIsIdentity)
{
    constexpr std::array<ColorSpace, 7> spaces = {
        ColorSpace::Linear, ColorSpace::sRGB, ColorSpace::Rec709,
        ColorSpace::Rec2020, ColorSpace::P3, ColorSpace::ACES_AP0,
        ColorSpace::ACES_AP1,
    };

    for (const ColorSpace space : spaces) {
        const auto matrix = ColorSpaceConverter::getConversionMatrix(space, space);
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t column = 0; column < 4; ++column) {
                EXPECT_FLOAT_EQ(matrix[row * 4 + column], row == column ? 1.0f : 0.0f)
                    << "space " << static_cast<int>(space)
                    << ", row " << row << ", column " << column;
            }
        }
    }
}

TEST(ColorSpaceTest, ColorSpaceMetadataMatchesDeclaredPrimariesAndTransferDefaults)
{
    struct MetadataCase {
        ColorSpace space;
        float whitePointX;
        float whitePointY;
        float gammaExponent;
    };
    constexpr MetadataCase cases[] = {
        {ColorSpace::Linear, 0.3127f, 0.3290f, 1.0f},
        {ColorSpace::sRGB, 0.3127f, 0.3290f, 2.2f},
        {ColorSpace::Rec709, 0.3127f, 0.3290f, 2.2f},
        {ColorSpace::Rec2020, 0.3127f, 0.3290f, 2.4f},
        {ColorSpace::P3, 0.3140f, 0.3377f, 2.2f},
        {ColorSpace::ACES_AP0, 0.32168f, 0.33767f, 1.0f},
        {ColorSpace::ACES_AP1, 0.32168f, 0.33767f, 1.0f},
    };

    for (const MetadataCase& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase.space));
        EXPECT_NEAR(ColorSpaceConverter::getWhitePointX(testCase.space),
                    testCase.whitePointX, 1e-6f);
        EXPECT_NEAR(ColorSpaceConverter::getWhitePointY(testCase.space),
                    testCase.whitePointY, 1e-6f);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::getGammaExponent(testCase.space),
                        testCase.gammaExponent);
    }
}

TEST(ColorSpaceTest, UnknownColorSpaceMetadataFallsBackToSrgbDefaults)
{
    const auto unknown = static_cast<ColorSpace>(-1);

    EXPECT_FLOAT_EQ(ColorSpaceConverter::getWhitePointX(unknown), 0.3127f);
    EXPECT_FLOAT_EQ(ColorSpaceConverter::getWhitePointY(unknown), 0.3290f);
    EXPECT_FLOAT_EQ(ColorSpaceConverter::getGammaExponent(unknown), 2.2f);
}

TEST(ColorSpaceTest, Rec709AndRec2020MatrixRoundTripPreservesRgb)
{
    const std::array<float, 3> source = {0.72f, 0.18f, 0.04f};
    const auto toRec2020 = ColorSpaceConverter::getConversionMatrix(
        ColorSpace::Rec709, ColorSpace::Rec2020);
    const auto toRec709 = ColorSpaceConverter::getConversionMatrix(
        ColorSpace::Rec2020, ColorSpace::Rec709);

    expectRgbNear(applyRgbMatrix(toRec709, applyRgbMatrix(toRec2020, source)),
                  source, kMatrixEpsilon);
}

TEST(ColorSpaceTest, EverySupportedSpacePairRoundTripsLinearRgbSamples)
{
    constexpr std::array<ColorSpace, 7> spaces = {
        ColorSpace::Linear, ColorSpace::sRGB, ColorSpace::Rec709,
        ColorSpace::Rec2020, ColorSpace::P3, ColorSpace::ACES_AP0,
        ColorSpace::ACES_AP1,
    };
    constexpr std::array<std::array<float, 3>, 7> samples = {{
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.72f, 0.18f, 0.04f},
        {1.5f, 0.25f, -0.1f},
    }};

    for (const ColorSpace from : spaces) {
        for (const ColorSpace to : spaces) {
            const auto forward = ColorSpaceConverter::getConversionMatrix(from, to);
            const auto reverse = ColorSpaceConverter::getConversionMatrix(to, from);
            SCOPED_TRACE(::testing::Message()
                << "from=" << static_cast<int>(from)
                << " to=" << static_cast<int>(to));

            for (std::size_t element = 0; element < forward.size(); ++element) {
                ASSERT_TRUE(std::isfinite(forward[element]));
                ASSERT_TRUE(std::isfinite(reverse[element]));
            }
            for (const auto& matrix : {forward, reverse}) {
                EXPECT_FLOAT_EQ(matrix[3], 0.0f);
                EXPECT_FLOAT_EQ(matrix[7], 0.0f);
                EXPECT_FLOAT_EQ(matrix[11], 0.0f);
                EXPECT_FLOAT_EQ(matrix[12], 0.0f);
                EXPECT_FLOAT_EQ(matrix[13], 0.0f);
                EXPECT_FLOAT_EQ(matrix[14], 0.0f);
                EXPECT_FLOAT_EQ(matrix[15], 1.0f);
            }
            EXPECT_FLOAT_EQ(forward[15], 1.0f);
            EXPECT_FLOAT_EQ(reverse[15], 1.0f);

            constexpr std::array<float, 4> rgba = {0.37f, 0.61f, 0.14f, 0.42f};
            const auto transformedAlpha = [](const std::array<float, 16>& matrix,
                                             const std::array<float, 4>& color) {
                return matrix[12] * color[0] + matrix[13] * color[1] +
                       matrix[14] * color[2] + matrix[15] * color[3];
            };
            EXPECT_FLOAT_EQ(transformedAlpha(forward, rgba), rgba[3]);
            EXPECT_FLOAT_EQ(transformedAlpha(reverse, rgba), rgba[3]);

            for (const auto& source : samples) {
                expectRgbNear(applyRgbMatrix(reverse,
                                             applyRgbMatrix(forward, source)),
                              source, 1.0e-3f);
            }
        }
    }
}

TEST(ColorSpaceTest, Rec709RedUsesExpectedRec2020Coordinates)
{
    const auto matrix = ColorSpaceConverter::getConversionMatrix(
        ColorSpace::Rec709, ColorSpace::Rec2020);
    const auto converted = applyRgbMatrix(matrix, {1.0f, 0.0f, 0.0f});

    // Reference values for linear Rec.709 red expressed in linear Rec.2020.
    expectRgbNear(converted, {0.6274f, 0.0691f, 0.0164f}, 1.5e-3f);
}

TEST(ColorSpaceTest, SrgbTransferHasExpectedReferencePointsAndRoundTrips)
{
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(0.0031308f, GammaFunction::sRGB),
                0.0404499f, 2.0e-6f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(0.04045f, GammaFunction::sRGB),
                0.0031308f, 2.0e-7f);

    EXPECT_NEAR(ColorSpaceConverter::applyGamma(0.0031307f, GammaFunction::sRGB),
                12.92f * 0.0031307f, 1.0e-7f);
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(0.0031309f, GammaFunction::sRGB),
                1.055f * std::pow(0.0031309f, 1.0f / 2.4f) - 0.055f, 1.0e-7f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(0.04044f, GammaFunction::sRGB),
                0.04044f / 12.92f, 1.0e-7f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(0.04046f, GammaFunction::sRGB),
                std::pow((0.04046f + 0.055f) / 1.055f, 2.4f), 1.0e-7f);

    constexpr std::array<float, 5> samples = {0.0f, 0.003f, 0.18f, 0.5f, 1.0f};
    for (const float sample : samples) {
        EXPECT_NEAR(ColorSpaceConverter::removeGamma(
                        ColorSpaceConverter::applyGamma(sample, GammaFunction::sRGB),
                        GammaFunction::sRGB),
                    sample, kTransferEpsilon);
    }
}

TEST(ColorSpaceTest, SrgbTransferPreservesSignedLinearValues)
{
    constexpr std::array<float, 3> samples = {-1.0f, -0.25f, -0.003f};
    for (const float sample : samples) {
        const float encoded = ColorSpaceConverter::applyGamma(sample, GammaFunction::sRGB);
        EXPECT_LT(encoded, 0.0f);
        EXPECT_NEAR(encoded, sample * 12.92f, 1e-6f);
        EXPECT_NEAR(ColorSpaceConverter::removeGamma(encoded, GammaFunction::sRGB),
                    sample, kTransferEpsilon);
    }
}

TEST(ColorSpaceTest, SrgbTransferMatchesSignedReferenceAcrossLinearHdrGrid)
{
    constexpr float linearBreakpoint = 0.0031308f;
    constexpr float encodedBreakpoint = 0.04045f;
    constexpr std::array<float, 15> samples = {
        -4.0f, -1.0f, -0.25f, -0.01f, -linearBreakpoint,
        -1.0e-6f, 0.0f, 1.0e-6f, linearBreakpoint,
        0.01f, 0.18f, 1.0f, 2.0f, 4.0f, 16.0f,
    };
    const auto encodeReference = [](const float linear) {
        if (linear < 0.0f || linear <= linearBreakpoint) {
            return 12.92f * linear;
        }
        return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    };
    const auto decodeReference = [](const float encoded) {
        if (encoded < 0.0f || encoded <= encodedBreakpoint) {
            return encoded / 12.92f;
        }
        return std::pow((encoded + 0.055f) / 1.055f, 2.4f);
    };

    for (const float linear : samples) {
        const float encoded = ColorSpaceConverter::applyGamma(linear, GammaFunction::sRGB);
        SCOPED_TRACE(::testing::Message() << "linear=" << linear);
        EXPECT_NEAR(encoded, encodeReference(linear), 3e-6f);
        EXPECT_NEAR(ColorSpaceConverter::removeGamma(encoded, GammaFunction::sRGB),
                    linear, std::max(2e-6f, std::abs(linear) * 2e-6f));
    }

    const float belowLinear = std::nextafter(linearBreakpoint, 0.0f);
    const float aboveLinear = std::nextafter(linearBreakpoint, 1.0f);
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(belowLinear, GammaFunction::sRGB),
                encodeReference(belowLinear), 1e-7f);
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(aboveLinear, GammaFunction::sRGB),
                encodeReference(aboveLinear), 1e-7f);

    const float belowEncoded = std::nextafter(encodedBreakpoint, 0.0f);
    const float aboveEncoded = std::nextafter(encodedBreakpoint, 1.0f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(belowEncoded, GammaFunction::sRGB),
                decodeReference(belowEncoded), 1e-8f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(aboveEncoded, GammaFunction::sRGB),
                decodeReference(aboveEncoded), 1e-8f);
}

TEST(ColorSpaceTest, SrgbDecodeMatchesIndependentReferenceForEveryByteCode)
{
    const auto decodeReference = [](const double encoded) {
        return encoded <= 0.04045
            ? encoded / 12.92
            : std::pow((encoded + 0.055) / 1.055, 2.4);
    };

    for (int code = 0; code <= 255; ++code) {
        const double encoded = static_cast<double>(code) / 255.0;
        const float actual = ColorSpaceConverter::removeGamma(
            static_cast<float>(encoded), GammaFunction::sRGB);
        SCOPED_TRACE(::testing::Message() << "sRGB byte code=" << code);
        EXPECT_NEAR(actual, decodeReference(encoded), 2.0e-7);
    }
}

TEST(ColorSpaceTest, Gamma22MatchesIndependentReferenceAcrossTenBitCodes)
{
    constexpr double exponent = 2.2;

    for (int code = 0; code <= 1023; ++code) {
        const double encoded = static_cast<double>(code) / 1023.0;
        const float decoded = ColorSpaceConverter::removeGamma(
            static_cast<float>(encoded), GammaFunction::Gamma22);
        SCOPED_TRACE(::testing::Message() << "gamma 2.2 10-bit code=" << code);
        EXPECT_NEAR(decoded, std::pow(encoded, exponent), 3e-7);
    }
}

TEST(ColorSpaceTest, PowerTransfersRoundTripSignedValues)
{
    constexpr std::array<GammaFunction, 3> transfers = {
        GammaFunction::Gamma22, GammaFunction::Gamma24, GammaFunction::Gamma26,
    };
    constexpr std::array<float, 5> samples = {-0.25f, -0.01f, 0.0f, 0.18f, 1.0f};

    for (const GammaFunction transfer : transfers) {
        for (const float sample : samples) {
            const float encoded = ColorSpaceConverter::applyGamma(sample, transfer);
            EXPECT_NEAR(ColorSpaceConverter::removeGamma(encoded, transfer), sample,
                        kTransferEpsilon);
        }
    }
}

TEST(ColorSpaceTest, PqAndHlgStayFiniteAndRoundTripNormalizedValues)
{
    constexpr std::array<GammaFunction, 2> transfers = {
        GammaFunction::PQ, GammaFunction::HLG,
    };
    constexpr std::array<float, 6> samples = {0.0f, 0.001f, 0.01f, 0.18f, 0.5f, 1.0f};

    for (const GammaFunction transfer : transfers) {
        for (const float sample : samples) {
            const float encoded = ColorSpaceConverter::applyGamma(sample, transfer);
            ASSERT_TRUE(std::isfinite(encoded));
            EXPECT_GE(encoded, 0.0f);
            EXPECT_LE(encoded, 1.0f);
            EXPECT_NEAR(ColorSpaceConverter::removeGamma(encoded, transfer), sample,
                        3.0e-5f);
        }
    }
}

TEST(ColorSpaceTest, HlgTransferMatchesBothPiecewiseBranchBoundaries)
{
    constexpr float sceneLinearBoundary = 1.0f / 12.0f;
    constexpr float encodedBoundary = 0.5f;
    constexpr float hlgA = 0.17883277f;
    constexpr float hlgB = 0.28466892f;
    constexpr float hlgC = 0.55991073f;

    EXPECT_NEAR(ColorSpaceConverter::applyGamma(
                    sceneLinearBoundary, GammaFunction::HLG),
                std::sqrt(3.0f * sceneLinearBoundary), 1e-7f);
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(
                    std::nextafter(sceneLinearBoundary, 0.0f), GammaFunction::HLG),
                std::sqrt(3.0f * std::nextafter(sceneLinearBoundary, 0.0f)), 1e-7f);
    const float aboveSceneBoundary = std::nextafter(sceneLinearBoundary, 1.0f);
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(aboveSceneBoundary, GammaFunction::HLG),
                hlgA * std::log(12.0f * aboveSceneBoundary - hlgB) + hlgC, 1e-7f);

    EXPECT_NEAR(ColorSpaceConverter::removeGamma(encodedBoundary, GammaFunction::HLG),
                (encodedBoundary * encodedBoundary) / 3.0f, 1e-7f);
    const float aboveEncodedBoundary = std::nextafter(encodedBoundary, 1.0f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(aboveEncodedBoundary, GammaFunction::HLG),
                (std::exp((aboveEncodedBoundary - hlgC) / hlgA) + hlgB) / 12.0f,
                1e-7f);
}

TEST(ColorSpaceTest, PqTransferMatchesStandardEighteenPercentReference)
{
    // ST 2084 encodes 18% reference luminance to approximately 0.81594.
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(0.18f, GammaFunction::PQ),
                0.81594f, 2e-5f);
}

TEST(ColorSpaceTest, PqTransferMatchesSt2084ReferenceAcrossNormalizedGrid)
{
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    constexpr std::array<float, 13> samples = {
        0.0f, 1.0e-6f, 1.0e-5f, 1.0e-4f, 0.001f, 0.01f,
        0.05f, 0.18f, 0.5f, 0.75f, 0.9f, 0.99f, 1.0f,
    };
    const auto encodeSt2084 = [](const double luminance) {
        const double p = std::pow(std::clamp(luminance, 0.0, 1.0), m1);
        return std::pow((c1 + c2 * p) / (1.0 + c3 * p), m2);
    };
    const auto decodeSt2084 = [](const double encoded) {
        const double p = std::pow(std::clamp(encoded, 0.0, 1.0), 1.0 / m2);
        const double numerator = std::max(p - c1, 0.0);
        const double denominator = c2 - c3 * p;
        return denominator > 0.0 ? std::pow(numerator / denominator, 1.0 / m1) : 0.0;
    };

    for (const float luminance : samples) {
        const float encoded = ColorSpaceConverter::applyGamma(luminance, GammaFunction::PQ);
        SCOPED_TRACE(luminance);
        EXPECT_NEAR(encoded, encodeSt2084(luminance), 7e-6);
        EXPECT_NEAR(ColorSpaceConverter::removeGamma(encoded, GammaFunction::PQ),
                    decodeSt2084(encoded), 3e-5);
    }
    EXPECT_NEAR(ColorSpaceConverter::applyGamma(1.0f, GammaFunction::PQ), 1.0f, 1e-7f);
    EXPECT_NEAR(ColorSpaceConverter::removeGamma(1.0f, GammaFunction::PQ), 1.0f, 1e-6f);
}

TEST(ColorSpaceTest, ImplementedTransferFunctionsAreMonotonicAcrossNormalizedGrid)
{
    constexpr std::array<GammaFunction, 7> transfers = {
        GammaFunction::Linear,
        GammaFunction::sRGB,
        GammaFunction::Gamma22,
        GammaFunction::Gamma24,
        GammaFunction::Gamma26,
        GammaFunction::PQ,
        GammaFunction::HLG,
    };
    constexpr int subdivisions = 256;
    constexpr float roundTripTolerance = 1.0e-4f;

    for (const GammaFunction transfer : transfers) {
        float previousEncoded = ColorSpaceConverter::applyGamma(0.0f, transfer);
        ASSERT_TRUE(std::isfinite(previousEncoded));
        EXPECT_GE(previousEncoded, 0.0f);
        EXPECT_LE(previousEncoded, 1.0f);
        for (int index = 0; index <= subdivisions; ++index) {
            const float linear = index / static_cast<float>(subdivisions);
            const float encoded = ColorSpaceConverter::applyGamma(linear, transfer);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer)
                << " linear=" << linear);

            ASSERT_TRUE(std::isfinite(encoded));
            EXPECT_GE(encoded, 0.0f);
            EXPECT_LE(encoded, 1.0f);
            EXPECT_GE(encoded, previousEncoded);
            const float decoded = ColorSpaceConverter::removeGamma(encoded, transfer);
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, linear, roundTripTolerance);
            previousEncoded = encoded;
        }
    }
}

TEST(ColorSpaceTest, PqAndHlgClampOutOfRangeInputsToTransferEndpoints)
{
    for (const GammaFunction transfer : {GammaFunction::PQ, GammaFunction::HLG}) {
        EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(-0.5f, transfer),
                        ColorSpaceConverter::applyGamma(0.0f, transfer));
        EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(1.5f, transfer),
                        ColorSpaceConverter::applyGamma(1.0f, transfer));
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(-0.5f, transfer), 0.0f);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(1.5f, transfer), 1.0f);
    }
}

TEST(ColorSpaceTest, NonFiniteTransferInputsAreSanitized)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    constexpr std::array<GammaFunction, 7> transfers = {
        GammaFunction::Linear, GammaFunction::sRGB, GammaFunction::Gamma22,
        GammaFunction::Gamma24, GammaFunction::Gamma26, GammaFunction::PQ,
        GammaFunction::HLG,
    };

    for (const GammaFunction transfer : transfers) {
        SCOPED_TRACE(static_cast<int>(transfer));
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(nan, transfer), 0.0f);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(infinity, transfer), 0.0f);
        if (transfer == GammaFunction::PQ) {
            constexpr float m1 = 2610.0f / 16384.0f;
            constexpr float m2 = 2523.0f / 32.0f;
            constexpr float c1 = 3424.0f / 4096.0f;
            constexpr float c2 = 2413.0f / 128.0f;
            constexpr float c3 = 2392.0f / 128.0f;
            const float expected = std::pow(c1, m2);
            EXPECT_NEAR(ColorSpaceConverter::applyGamma(nan, transfer), expected, 1e-12f);
            EXPECT_NEAR(ColorSpaceConverter::applyGamma(infinity, transfer), expected, 1e-12f);
        } else {
            EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(nan, transfer), 0.0f);
            EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(infinity, transfer), 0.0f);
        }
    }
}

TEST(ColorSpaceTest, UnknownGammaFunctionFallsBackToSanitizedIdentity)
{
    const auto unknown = static_cast<GammaFunction>(-1);
    constexpr std::array<float, 4> values = {-0.25f, 0.0f, 0.18f, 1.5f};

    for (const float value : values) {
        EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(value, unknown), value);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(value, unknown), value);
    }
    EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(
                        std::numeric_limits<float>::quiet_NaN(), unknown), 0.0f);
    EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(
                        std::numeric_limits<float>::infinity(), unknown), 0.0f);
}

TEST(ColorSpaceTest, AcesDisplayTransformIsFiniteBoundedAndMonotonic)
{
    const auto black = ColorSpaceConverter::applyACESDisplayTransform({0.0f, 0.0f, 0.0f});
    const auto middle = ColorSpaceConverter::applyACESDisplayTransform({0.18f, 0.18f, 0.18f});
    const auto white = ColorSpaceConverter::applyACESDisplayTransform({1.0f, 1.0f, 1.0f});
    const auto brighter = ColorSpaceConverter::applyACESDisplayTransform({1.0f, 1.0f, 1.0f}, 1.0f);
    const auto negativeAndNonFinite = ColorSpaceConverter::applyACESDisplayTransform({
        -1.0f, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()});

    for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_FLOAT_EQ(black[channel], 0.0f);
        EXPECT_GE(middle[channel], 0.0f);
        EXPECT_LE(middle[channel], 1.0f);
        EXPECT_GT(middle[channel], black[channel]);
        EXPECT_GE(white[channel], middle[channel]);
        EXPECT_LE(white[channel], 1.0f);
        EXPECT_GE(brighter[channel], white[channel]);
        EXPECT_LE(brighter[channel], 1.0f);
        EXPECT_FLOAT_EQ(negativeAndNonFinite[channel], 0.0f);
    }
    expectRgbNear(middle, {middle[0], middle[0], middle[0]}, 0.0f);
}

TEST(ColorSpaceTest, AcesDisplayTransformRemainsOrderedAcrossDenseHdrAndExposureGrid)
{
    constexpr float exposures[] = {-8.0f, 0.0f, 4.0f, 8.0f, 16.0f};
    constexpr int subdivisions = 2048;
    constexpr float maximumSceneLinear = 64.0f;

    for (const float exposure : exposures) {
        float previous = 0.0f;
        for (int index = 0; index <= subdivisions; ++index) {
            const float linear = maximumSceneLinear * index / subdivisions;
            const auto mapped = ColorSpaceConverter::applyACESDisplayTransform(
                {linear, linear, linear}, exposure);
            SCOPED_TRACE(::testing::Message()
                << "exposure=" << exposure << " index=" << index
                << " linear=" << linear);
            for (const float channel : mapped) {
                ASSERT_TRUE(std::isfinite(channel));
                EXPECT_GE(channel, 0.0f);
                EXPECT_LE(channel, 1.0f);
                EXPECT_FLOAT_EQ(channel, mapped[0]);
                EXPECT_GE(channel, previous);
            }
            previous = mapped[0];
        }
    }
}

TEST(ColorSpaceTest, AcesDisplayTransformMatchesFittedCurveReferencePoints)
{
    const auto middle = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.18f, 0.18f});
    const auto white = ColorSpaceConverter::applyACESDisplayTransform(
        {1.0f, 1.0f, 1.0f});
    const auto hdr = ColorSpaceConverter::applyACESDisplayTransform(
        {4.0f, 4.0f, 4.0f});

    expectRgbNear(middle, {0.10559125f, 0.10559125f, 0.10559125f}, 2e-7f);
    expectRgbNear(white, {0.61911543f, 0.61911543f, 0.61911543f}, 2e-7f);
    expectRgbNear(hdr, {0.90901377f, 0.90901377f, 0.90901377f}, 2e-7f);
}

TEST(ColorSpaceTest, AcesDisplayTransformProcessesRgbChannelsIndependently)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto distinct = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f});
    expectRgbNear(distinct,
                  {0.10559125f, 0.37430832f, 0.61911543f}, 2e-7f);

    const auto badRed = ColorSpaceConverter::applyACESDisplayTransform(
        {nan, 0.5f, 1.0f});
    const auto badGreen = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, -1.0f, 1.0f});
    const auto badBlue = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, infinity});

    expectRgbNear(badRed, {0.0f, distinct[1], distinct[2]}, 2e-7f);
    expectRgbNear(badGreen, {distinct[0], 0.0f, distinct[2]}, 2e-7f);
    expectRgbNear(badBlue, {distinct[0], distinct[1], 0.0f}, 2e-7f);
}

TEST(ColorSpaceTest, AcesDisplayTransformAppliesClampedExposureAndSanitizesExposure)
{
    const auto oneStopUp = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, 1.0f);
    const auto equivalentInputScale = ColorSpaceConverter::applyACESDisplayTransform(
        {0.36f, 1.0f, 2.0f}, 0.0f);
    const auto nonFiniteExposure = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, std::numeric_limits<float>::infinity());
    const auto zeroExposure = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, 0.0f);
    const auto highExposure = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, 100.0f);
    const auto clampedHighExposure = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, 16.0f);

    expectRgbNear(oneStopUp, equivalentInputScale, 2e-7f);
    expectRgbNear(nonFiniteExposure, zeroExposure, 0.0f);
    expectRgbNear(highExposure, clampedHighExposure, 0.0f);
}

TEST(ColorSpaceTest, AcesDisplayTransformClampsLowExposureAndNegativeToeOutput)
{
    const auto belowExposureFloor = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, -100.0f);
    const auto atExposureFloor = ColorSpaceConverter::applyACESDisplayTransform(
        {0.18f, 0.5f, 1.0f}, -16.0f);
    const auto darkInputs = ColorSpaceConverter::applyACESDisplayTransform(
        {0.0f, 1.0e-5f, 1.0e-4f});

    expectRgbNear(belowExposureFloor, atExposureFloor, 0.0f);
    expectRgbNear(darkInputs, {0.0f, 0.0f, 0.0f}, 0.0f);
}

TEST(ColorSpaceTest, AcesDisplayTransformRetainsDistinctSignalsAtExposureFloor)
{
    const auto belowFloor = ColorSpaceConverter::applyACESDisplayTransform(
        {300.0f, 500.0f, 1000.0f}, -100.0f);
    const auto atFloor = ColorSpaceConverter::applyACESDisplayTransform(
        {300.0f, 500.0f, 1000.0f}, -16.0f);

    expectRgbNear(belowFloor, atFloor, 0.0f);
    expectRgbNear(atFloor,
                  {0.00017881f, 0.00064277f, 0.00211229f}, 2e-8f);
    EXPECT_LT(atFloor[0], atFloor[1]);
    EXPECT_LT(atFloor[1], atFloor[2]);
}

TEST(ColorSpaceTest, AcesDisplayTransformPreservesHdrOrderBeforeHighlightSaturation)
{
    const auto hdrAtEightStops = ColorSpaceConverter::applyACESDisplayTransform(
        {0.01f, 0.1f, 1.0f}, 8.0f);
    const auto hdrAtExposureCeiling = ColorSpaceConverter::applyACESDisplayTransform(
        {0.01f, 0.1f, 1.0f}, 16.0f);

    expectRgbNear(hdrAtEightStops,
                  {0.8489785f, 0.9999556f, 1.0f}, 2e-7f);
    EXPECT_LT(hdrAtEightStops[0], hdrAtEightStops[1]);
    EXPECT_LT(hdrAtEightStops[1], hdrAtEightStops[2]);
    for (const float channel : hdrAtEightStops) {
        EXPECT_GE(channel, 0.0f);
        EXPECT_LE(channel, 1.0f);
    }
    expectRgbNear(hdrAtExposureCeiling, {1.0f, 1.0f, 1.0f}, 0.0f);
}
