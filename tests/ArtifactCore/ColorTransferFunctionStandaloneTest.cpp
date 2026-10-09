#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

import Color.TransferFunction;

using namespace ArtifactCore;

namespace {

double rec709OetfReference(const double linear)
{
    return linear < 0.018
        ? 4.5 * linear
        : 1.099 * std::pow(linear, 0.45) - 0.099;
}

double rec709EotfReference(const double encoded)
{
    return encoded < 0.081
        ? encoded / 4.5
        : std::pow((encoded + 0.099) / 1.099, 1.0 / 0.45);
}

double rec2020OetfReference(const double linear)
{
    constexpr double alpha = 1.09929682680944;
    constexpr double beta = 0.018053968510807;
    return linear < beta
        ? 4.5 * linear
        : alpha * std::pow(linear, 0.45) - (alpha - 1.0);
}

double rec2020EotfReference(const double encoded)
{
    constexpr double alpha = 1.09929682680944;
    constexpr double encodedBeta = 0.081242858298635;
    return encoded < encodedBeta
        ? encoded / 4.5
        : std::pow((encoded + alpha - 1.0) / alpha, 1.0 / 0.45);
}

double pqOetfReference(const double linear)
{
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    if (linear <= 0.0) return 0.0;
    const double p = std::pow(linear, m1);
    return std::pow((c1 + c2 * p) / (1.0 + c3 * p), m2);
}

double pqEotfReference(const double encoded)
{
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    if (encoded <= 0.0) return 0.0;
    const double p = std::pow(encoded, 1.0 / m2);
    const double numerator = std::max(p - c1, 0.0);
    const double denominator = c2 - c3 * p;
    return denominator <= 0.0
        ? 0.0
        : std::pow(numerator / denominator, 1.0 / m1);
}

double hlgOetfReference(const double linear)
{
    constexpr double a = 0.17883277;
    constexpr double b = 0.28466892;
    constexpr double c = 0.55991073;
    const double value = std::max(linear, 0.0);
    return value <= 1.0 / 12.0
        ? std::sqrt(3.0 * value)
        : a * std::log(12.0 * value - b) + c;
}

double hlgEotfReference(const double encoded)
{
    constexpr double a = 0.17883277;
    constexpr double b = 0.28466892;
    constexpr double c = 0.55991073;
    const double value = std::max(encoded, 0.0);
    return value <= 0.5
        ? (value * value) / 3.0
        : (std::exp((value - c) / a) + b) / 12.0;
}

double slog3OetfReference(const double linear)
{
    constexpr double breakpoint = 0.01125;
    constexpr double codeAtBreakpoint = 171.2102946929;
    return linear >= breakpoint
        ? (420.0 + std::log10((linear + 0.01) / 0.19) * 261.5) / 1023.0
        : (linear * (codeAtBreakpoint - 95.0) / breakpoint + 95.0) / 1023.0;
}

double slog3EotfReference(const double encoded)
{
    constexpr double codeAtBreakpoint = 171.2102946929;
    const double code = encoded * 1023.0;
    return code >= codeAtBreakpoint
        ? std::pow(10.0, (code - 420.0) / 261.5) * 0.19 - 0.01
        : (code - 95.0) * 0.01125 / (codeAtBreakpoint - 95.0);
}

double canonLog3OetfReference(const double linear)
{
    constexpr double low = 0.04076162;
    constexpr double high = 0.105357102;
    constexpr double toe = 0.069886632;
    constexpr double slope = 0.42889912;
    constexpr double scale = 14.98325;
    const double lowLinear = -(std::pow(10.0, (toe - low) / slope) - 1.0) / scale;
    const double highLinear = (high - 0.073059361) / 2.3069815;
    if (linear < lowLinear) {
        return -(slope * std::log10(-linear * scale + 1.0) - toe);
    }
    if (linear <= highLinear) return 2.3069815 * linear + 0.073059361;
    return slope * std::log10(linear * scale + 1.0) + toe;
}

double canonLog3EotfReference(const double encoded)
{
    constexpr double low = 0.04076162;
    constexpr double high = 0.105357102;
    constexpr double toe = 0.069886632;
    constexpr double slope = 0.42889912;
    constexpr double scale = 14.98325;
    if (encoded < low) {
        return -(std::pow(10.0, (toe - encoded) / slope) - 1.0) / scale;
    }
    if (encoded <= high) return (encoded - 0.073059361) / 2.3069815;
    return (std::pow(10.0, (encoded - toe) / slope) - 1.0) / scale;
}

double cineonOetfReference(const double linear)
{
    constexpr double blackOffset = 0.0107977516232771;
    const double value = std::max(linear, 0.0);
    return (685.0 + 300.0 * std::log10(
        value * (1.0 - blackOffset) + blackOffset)) / 1023.0;
}

double cineonEotfReference(const double encoded)
{
    constexpr double blackOffset = 0.0107977516232771;
    return (std::pow(10.0, (1023.0 * encoded - 685.0) / 300.0) - blackOffset) /
           (1.0 - blackOffset);
}

double canonLog2OetfReference(const double linear)
{
    constexpr double toe = 0.035388128;
    constexpr double slope = 0.281863093;
    constexpr double scale = 87.09937546;
    const double toeLinear = -(std::pow(10.0, toe / slope) - 1.0) / scale;
    return linear < toeLinear
        ? -(slope * std::log10(-linear * scale + 1.0) - toe)
        : slope * std::log10(linear * scale + 1.0) + toe;
}

double canonLog2EotfReference(const double encoded)
{
    constexpr double toe = 0.035388128;
    constexpr double slope = 0.281863093;
    constexpr double scale = 87.09937546;
    return encoded < toe
        ? -(std::pow(10.0, (toe - encoded) / slope) - 1.0) / scale
        : (std::pow(10.0, (encoded - toe) / slope) - 1.0) / scale;
}

double daVinciIntermediateOetfReference(const double linear)
{
    return linear <= 0.0
        ? 0.0
        : (std::log2(linear) + 12.473931188) / 12.900429241;
}

double daVinciIntermediateEotfReference(const double encoded)
{
    return std::pow(2.0, encoded * 12.900429241 - 12.473931188);
}

double acesCctOetfReference(const double linear)
{
    constexpr double breakpoint = 0.0078125;
    return linear <= breakpoint
        ? 10.5402377416545 * linear + 0.0729055341958355
        : (std::log2(linear) + 9.72) / 17.52;
}

double acesCctEotfReference(const double encoded)
{
    constexpr double encodedBreakpoint = 0.155251141552511;
    return encoded <= encodedBreakpoint
        ? (encoded - 0.0729055341958355) / 10.5402377416545
        : std::pow(2.0, encoded * 17.52 - 9.72);
}

double acesCcOetfReference(const double linear)
{
    return linear <= 0.0
        ? -0.3584474886
        : (9.72 - std::log2(linear * 0.5 + 0.000030517578125)) / 17.52;
}

double acesCcEotfReference(const double encoded)
{
    return (std::pow(2.0, 9.72 - encoded * 17.52) - 0.000030517578125) * 2.0;
}

} // namespace

TEST(ColorTransferFunctionStandaloneTest,
     Rec709OetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::Rec709),
                    rec709OetfReference(normalized), 4.0e-7);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::Rec709),
                    rec709EotfReference(normalized), 4.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec709PublishedBreakpointsExposeCurrentOetfEotfMismatch)
{
    constexpr double linearBreakpoint = 0.018;
    constexpr double encodedBreakpoint = 0.081;
    const double encodedAtOetfBreakpoint = ColorTransferFunction::encode(
        static_cast<float>(linearBreakpoint), TransferFunction::Rec709);
    const double decodedAtEotfBreakpoint = ColorTransferFunction::decode(
        static_cast<float>(encodedBreakpoint), TransferFunction::Rec709);

    EXPECT_NEAR(encodedAtOetfBreakpoint, rec709OetfReference(linearBreakpoint), 2.0e-7);
    EXPECT_NEAR(decodedAtEotfBreakpoint, rec709EotfReference(encodedBreakpoint), 2.0e-7);
    EXPECT_GT(encodedAtOetfBreakpoint - encodedBreakpoint, 2.4e-4);
    EXPECT_GT(linearBreakpoint - decodedAtEotfBreakpoint, 5.0e-5);
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec709AdjacentFloatBreakpointsCharacterizeOetfRiseAndEotfDrop)
{
    constexpr float linearBreakpoint = 0.018f;
    constexpr float encodedBreakpoint = 0.081f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f),
    };
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f),
    };
    const auto oetfFloatBranchReference = [=](const float linear) {
        return linear < linearBreakpoint
            ? 4.5 * static_cast<double>(linear)
            : 1.099 * std::pow(static_cast<double>(linear), 0.45) - 0.099;
    };
    const auto eotfFloatBranchReference = [=](const float encodedValue) {
        return encodedValue < encodedBreakpoint
            ? static_cast<double>(encodedValue) / 4.5
            : std::pow((static_cast<double>(encodedValue) + 0.099) / 1.099, 1.0 / 0.45);
    };

    std::array<float, 3> encoded{};
    for (std::size_t i = 0; i < linearSamples.size(); ++i) {
        encoded[i] = ColorTransferFunction::encode(linearSamples[i], TransferFunction::Rec709);
        EXPECT_NEAR(encoded[i], oetfFloatBranchReference(linearSamples[i]), 2.0e-7);
    }
    const float oetfRise = encoded[1] - encoded[0];
    EXPECT_GT(oetfRise, 2.4e-4f);
    EXPECT_LT(oetfRise, 2.6e-4f);
    EXPECT_GE(encoded[2], encoded[1]);

    std::array<float, 3> decoded{};
    for (std::size_t i = 0; i < encodedSamples.size(); ++i) {
        decoded[i] = ColorTransferFunction::decode(encodedSamples[i], TransferFunction::Rec709);
        EXPECT_NEAR(decoded[i], eotfFloatBranchReference(encodedSamples[i]), 2.0e-7);
    }
    const float eotfDrop = decoded[0] - decoded[1];
    EXPECT_GT(eotfDrop, 5.0e-5f);
    EXPECT_LT(eotfDrop, 6.0e-5f);
    EXPECT_GE(decoded[2], decoded[1]);
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec709ClampsNegativeInputsAndMatchesHdrExtrapolation)
{
    constexpr std::array<double, 8> linearSamples = {
        0.0, 1.0e-5, 0.001, 0.01, 0.1, 1.0, 4.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float actual = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::Rec709);
        SCOPED_TRACE(::testing::Message() << "Rec.709 linear=" << linear);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, rec709OetfReference(linear),
                    std::max(4.0e-7, std::abs(rec709OetfReference(linear)) * 2.0e-7));
    }

    constexpr std::array<double, 7> encodedSamples = {
        0.0, 0.01, 0.05, 0.2, 0.5, 1.0, 1.25,
    };
    for (const double encoded : encodedSamples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::Rec709);
        SCOPED_TRACE(::testing::Message() << "Rec.709 code=" << encoded);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, rec709EotfReference(encoded),
                    std::max(4.0e-7, std::abs(rec709EotfReference(encoded)) * 2.0e-7));
    }

    for (const float negative : {-100.0f, -1.0f, -0.01f, -1.0e-7f}) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(negative, TransferFunction::Rec709), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(negative, TransferFunction::Rec709), 0.0f);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SrgbOetfAndEotfBranchesMatchAtAdjacentFloatBreakpointsAndExposeOetfDrop)
{
    constexpr float linearBreakpoint = 0.0031308f;
    constexpr float encodedBreakpoint = 0.04045f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f),
    };
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f),
    };
    const auto encodeReference = [](const double linear) {
        return linear <= 0.0031308
            ? 12.92 * linear
            : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
    };
    const auto decodeReference = [](const double encoded) {
        return encoded <= 0.04045
            ? encoded / 12.92
            : std::pow((encoded + 0.055) / 1.055, 2.4);
    };

    for (const float sample : linearSamples) {
        EXPECT_NEAR(ColorTransferFunction::encode(sample, TransferFunction::sRGB),
                    encodeReference(sample), 4.0e-8);
    }
    for (const float sample : encodedSamples) {
        EXPECT_NEAR(ColorTransferFunction::decode(sample, TransferFunction::sRGB),
                    decodeReference(sample), 2.0e-9);
    }
    EXPECT_LE(ColorTransferFunction::encode(linearSamples[0], TransferFunction::sRGB),
              ColorTransferFunction::encode(linearSamples[1], TransferFunction::sRGB));
    const float oetfBoundaryDrop =
        ColorTransferFunction::encode(linearSamples[1], TransferFunction::sRGB) -
        ColorTransferFunction::encode(linearSamples[2], TransferFunction::sRGB);
    EXPECT_GT(oetfBoundaryDrop, 2.0e-8f);
    EXPECT_LT(oetfBoundaryDrop, 5.0e-8f);
    EXPECT_LE(ColorTransferFunction::decode(encodedSamples[0], TransferFunction::sRGB),
              ColorTransferFunction::decode(encodedSamples[1], TransferFunction::sRGB));
    EXPECT_LE(ColorTransferFunction::decode(encodedSamples[1], TransferFunction::sRGB),
              ColorTransferFunction::decode(encodedSamples[2], TransferFunction::sRGB));
}

TEST(ColorTransferFunctionStandaloneTest,
     SrgbClampsNegativeValuesAndRoundTripsPositiveHdrRange)
{
    constexpr std::array<double, 11> linearSamples = {
        0.0, 1.0e-6, 1.0e-4, 0.001, 0.01, 0.1,
        0.18, 0.5, 1.0, 4.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::sRGB);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::sRGB);
        SCOPED_TRACE(::testing::Message() << "sRGB scene-linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, linear * 4.0e-6));
    }

    constexpr std::array<float, 4> negativeSamples = {-100.0f, -1.0f, -0.01f, -1.0e-7f};
    for (const float negative : negativeSamples) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(negative, TransferFunction::sRGB), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(negative, TransferFunction::sRGB), 0.0f);
    }

    const float overRange = ColorTransferFunction::decode(1.25f, TransferFunction::sRGB);
    EXPECT_TRUE(std::isfinite(overRange));
    EXPECT_GT(overRange, 1.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec2020OetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::Rec2020_10),
                    rec2020OetfReference(normalized), 4.0e-7);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::Rec2020_10),
                    rec2020EotfReference(normalized), 4.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec2020PiecewiseBranchesMatchAtAndAroundPublishedBreakpoints)
{
    constexpr double linearBreakpoint = 0.018053968510807;
    constexpr double encodedBreakpoint = 0.081242858298635;
    const std::array<double, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0)};
    const std::array<double, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0)};

    for (const double sample : linearSamples) {
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(sample), TransferFunction::Rec2020_10),
                    rec2020OetfReference(sample), 2.0e-7);
    }
    for (const double sample : encodedSamples) {
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(sample), TransferFunction::Rec2020_10),
                    rec2020EotfReference(sample), 2.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec2020AdjacentFloatBreakpointsMatchFloatBranchSelection)
{
    constexpr float linearBreakpoint = 0.018053968510807f;
    constexpr float encodedBreakpoint = 0.081242858298635f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f),
    };
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f),
    };
    const auto oetfReferenceAtFloatBreakpoint = [=](const float value) {
        constexpr double alpha = 1.09929682680944;
        const double input = value;
        return value < linearBreakpoint
            ? 4.5 * input
            : alpha * std::pow(input, 0.45) - (alpha - 1.0);
    };
    const auto eotfReferenceAtFloatBreakpoint = [=](const float value) {
        constexpr double alpha = 1.09929682680944;
        const double input = value;
        return value < encodedBreakpoint
            ? input / 4.5
            : std::pow((input + alpha - 1.0) / alpha, 1.0 / 0.45);
    };

    float previousEncoded = -1.0f;
    for (const float linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::Rec2020_10);
        EXPECT_NEAR(encoded, oetfReferenceAtFloatBreakpoint(linear), 3.0e-7);
        EXPECT_GE(encoded, previousEncoded);
        previousEncoded = encoded;
    }

    std::array<float, 3> decoded{};
    for (std::size_t index = 0; index < encodedSamples.size(); ++index) {
        decoded[index] = ColorTransferFunction::decode(
            encodedSamples[index], TransferFunction::Rec2020_10);
        EXPECT_NEAR(decoded[index],
                    eotfReferenceAtFloatBreakpoint(encodedSamples[index]), 3.0e-7);
    }
    EXPECT_GT(decoded[0], decoded[1]);
    EXPECT_GT(decoded[0] - decoded[1], 1.0e-8f);
    EXPECT_LE(decoded[1], decoded[2]);
}

TEST(ColorTransferFunctionStandaloneTest,
     Rec2020ClampsNegativeValuesAndRoundTripsHdrSceneLinearRange)
{
    constexpr std::array<double, 12> linearSamples = {
        0.0, 1.0e-7, 1.0e-5, 0.001, 0.01, 0.017,
        0.02, 0.18, 0.5, 1.0, 4.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::Rec2020_10);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::Rec2020_10);
        SCOPED_TRACE(::testing::Message() << "Rec.2020 linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(encoded, rec2020OetfReference(linear), 4.0e-7);
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, linear * 3.0e-6));
    }

    for (const float negative : {-100.0f, -1.0f, -0.01f, -1.0e-7f}) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(negative, TransferFunction::Rec2020_10), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(negative, TransferFunction::Rec2020_10), 0.0f);
    }
    const float overRange = ColorTransferFunction::decode(1.25f, TransferFunction::Rec2020_10);
    EXPECT_TRUE(std::isfinite(overRange));
    EXPECT_GT(overRange, 1.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     PqOetfAndEotfMatchSt2084ReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "PQ 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::Rec2084_PQ),
                    pqOetfReference(normalized), 1.1e-5);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::Rec2084_PQ),
                    pqEotfReference(normalized), 4.0e-5);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     PqEncodeAndDecodeRemainFiniteAndMonotonicAcrossTenBitDomain)
{
    float previousEncoded = ColorTransferFunction::encode(0.0f, TransferFunction::Rec2084_PQ);
    float previousDecoded = ColorTransferFunction::decode(0.0f, TransferFunction::Rec2084_PQ);
    EXPECT_FLOAT_EQ(previousEncoded, 0.0f);
    EXPECT_FLOAT_EQ(previousDecoded, 0.0f);

    for (int code = 1; code <= 1023; ++code) {
        const float normalized = static_cast<float>(code) / 1023.0f;
        const float encoded = ColorTransferFunction::encode(normalized, TransferFunction::Rec2084_PQ);
        const float decoded = ColorTransferFunction::decode(normalized, TransferFunction::Rec2084_PQ);
        SCOPED_TRACE(::testing::Message() << "PQ 10-bit code=" << code);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_GE(encoded, previousEncoded);
        EXPECT_GE(decoded, previousDecoded);
        previousEncoded = encoded;
        previousDecoded = decoded;
    }

    EXPECT_FLOAT_EQ(previousEncoded, 1.0f);
    EXPECT_FLOAT_EQ(previousDecoded, 1.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     PqDecodeReturnsZeroAtAndAboveTheDenominatorSingularity)
{
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    constexpr double m2 = 2523.0 / 32.0;
    const double singularCode = std::pow(c2 / c3, m2);
    ASSERT_GT(singularCode, 1.0);

    EXPECT_NEAR(ColorTransferFunction::decode(1.0f, TransferFunction::Rec2084_PQ), 1.0f, 1.0e-6f);
    const std::array<double, 4> samples = {
        std::nextafter(singularCode, 0.0),
        singularCode,
        std::nextafter(singularCode, std::numeric_limits<double>::infinity()),
        singularCode + 0.25,
    };
    for (const double sample : samples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(sample), TransferFunction::Rec2084_PQ);
        SCOPED_TRACE(::testing::Message() << "PQ code=" << sample);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_FLOAT_EQ(actual, 0.0f);
    }

    EXPECT_GT(pqEotfReference(1.0), 0.99);
}

TEST(ColorTransferFunctionStandaloneTest,
     PqDecodeFloatDenominatorGuardHasAnAdjacentInputTransition)
{
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    constexpr double m2 = 2523.0 / 32.0;
    float denominatorGuardBoundary = std::pow(
        static_cast<float>(c2 / c3), static_cast<float>(m2));
    ASSERT_GT(denominatorGuardBoundary, 1.0f);

    float zeroAtGuard = denominatorGuardBoundary;
    float firstPositiveDenominatorCode = zeroAtGuard;
    float decodedBeforeGuard = 0.0f;
    bool foundPositiveOutput = false;
    for (int step = 0; step < 4096; ++step) {
        const float candidate = std::nextafter(zeroAtGuard, 0.0f);
        const float decoded = ColorTransferFunction::decode(
            candidate, TransferFunction::Rec2084_PQ);
        if (decoded > 0.0f) {
            firstPositiveDenominatorCode = candidate;
            decodedBeforeGuard = decoded;
            foundPositiveOutput = true;
            break;
        }
        zeroAtGuard = candidate;
    }
    ASSERT_TRUE(foundPositiveOutput);
    EXPECT_FLOAT_EQ(ColorTransferFunction::decode(
                        zeroAtGuard, TransferFunction::Rec2084_PQ), 0.0f);
    EXPECT_TRUE(std::isfinite(decodedBeforeGuard));
    EXPECT_GT(decodedBeforeGuard, 1.0e20f);

    const double independentBeforeGuard = pqEotfReference(firstPositiveDenominatorCode);
    EXPECT_TRUE(std::isfinite(independentBeforeGuard));
    EXPECT_GT(independentBeforeGuard / decodedBeforeGuard, 100.0);
}

TEST(ColorTransferFunctionStandaloneTest,
     PqExtremeFiniteInputsRemainFiniteOrHitDenominatorGuard)
{
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;
    const float maximum = std::numeric_limits<float>::max();
    const float encodedMaximum = ColorTransferFunction::encode(
        maximum, TransferFunction::Rec2084_PQ);
    const double asymptoticOetf = std::pow(c2 / c3, m2);

    EXPECT_TRUE(std::isfinite(encodedMaximum));
    EXPECT_GT(encodedMaximum, 1.0f);
    EXPECT_NEAR(encodedMaximum, asymptoticOetf,
                std::abs(asymptoticOetf) * 2.0e-6);
    EXPECT_FLOAT_EQ(ColorTransferFunction::decode(maximum, TransferFunction::Rec2084_PQ), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgOetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "HLG 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::HLG),
                    hlgOetfReference(normalized), 3.0e-7);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::HLG),
                    hlgEotfReference(normalized), 3.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgPiecewiseBranchesMatchAtAndAroundPublishedBreakpoints)
{
    constexpr double linearBreakpoint = 1.0 / 12.0;
    constexpr double encodedBreakpoint = 0.5;
    const std::array<double, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0)};
    const std::array<double, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0)};

    for (const double sample : linearSamples) {
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(sample), TransferFunction::HLG),
                    hlgOetfReference(sample), 2.0e-7);
    }
    for (const double sample : encodedSamples) {
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(sample), TransferFunction::HLG),
                    hlgEotfReference(sample), 2.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgAdjacentFloatBreakpointSamplesRemainMonotonic)
{
    constexpr float linearBreakpoint = 1.0f / 12.0f;
    constexpr float encodedBreakpoint = 0.5f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f)};
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f)};
    const auto oetfReferenceAtFloatBreakpoint = [=](const float value) {
        constexpr double a = 0.17883277;
        constexpr double b = 0.28466892;
        constexpr double c = 0.55991073;
        return value <= linearBreakpoint
            ? std::sqrt(3.0 * static_cast<double>(value))
            : a * std::log(12.0 * static_cast<double>(value) - b) + c;
    };
    const auto eotfReferenceAtFloatBreakpoint = [=](const float value) {
        constexpr double a = 0.17883277;
        constexpr double b = 0.28466892;
        constexpr double c = 0.55991073;
        return value <= encodedBreakpoint
            ? (static_cast<double>(value) * value) / 3.0
            : (std::exp((static_cast<double>(value) - c) / a) + b) / 12.0;
    };

    float previousEncoded = -1.0f;
    for (const float linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::HLG);
        EXPECT_NEAR(encoded, oetfReferenceAtFloatBreakpoint(linear), 2.0e-7);
        EXPECT_GE(encoded, previousEncoded);
        previousEncoded = encoded;
    }

    float previousLinear = -1.0f;
    for (const float encoded : encodedSamples) {
        const float linear = ColorTransferFunction::decode(
            encoded, TransferFunction::HLG);
        EXPECT_NEAR(linear, eotfReferenceAtFloatBreakpoint(encoded), 2.0e-7);
        EXPECT_GE(linear, previousLinear);
        previousLinear = linear;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgRoundTripsAcrossToeAndHdrLinearLightRange)
{
    constexpr std::array<float, 15> linearSamples = {
        0.0f,
        1.0e-8f, 1.0e-6f, 1.0e-4f, 0.001f, 0.01f,
        1.0f / 12.0f - 1.0e-6f, 1.0f / 12.0f, 1.0f / 12.0f + 1.0e-6f,
        0.18f, 0.5f, 1.0f, 2.0f, 4.0f, 16.0f,
    };
    for (const float linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(linear, TransferFunction::HLG);
        const float decoded = ColorTransferFunction::decode(encoded, TransferFunction::HLG);
        SCOPED_TRACE(::testing::Message() << "HLG scene-linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7f, linear * 3.0e-6f));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgOutOfRangeCodesClampNegativeAndExtendAboveOne)
{
    constexpr std::array<double, 6> encodedSamples = {-1.0, -0.25, 0.0, 0.5, 1.0, 1.25};
    for (const double encoded : encodedSamples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::HLG);
        const double expected = hlgEotfReference(encoded);
        SCOPED_TRACE(::testing::Message() << "HLG code=" << encoded);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, std::max(2.0e-7, expected * 3.0e-6));
    }

    EXPECT_FLOAT_EQ(ColorTransferFunction::decode(-1.0f, TransferFunction::HLG), 0.0f);
    EXPECT_GT(ColorTransferFunction::decode(1.25f, TransferFunction::HLG), 1.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-1.0f, TransferFunction::HLG), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgIntermediateMultiplyAndDecodeExponentialOverflowAtLargeFiniteInputs)
{
    const float maximum = std::numeric_limits<float>::max();
    const float largeFiniteLinear = maximum / 16.0f;
    const float encodedLargeFinite = ColorTransferFunction::encode(
        largeFiniteLinear, TransferFunction::HLG);
    EXPECT_TRUE(std::isfinite(encodedLargeFinite));
    EXPECT_NEAR(encodedLargeFinite,
                0.17883277 * std::log(12.0 * static_cast<double>(largeFiniteLinear) - 0.28466892) +
                    0.55991073,
                2.0e-6);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(maximum, TransferFunction::HLG)));

    constexpr float finiteCode = 16.0f;
    constexpr float overflowingCode = 17.0f;
    EXPECT_TRUE(std::isfinite(ColorTransferFunction::decode(
        finiteCode, TransferFunction::HLG)));
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(
        overflowingCode, TransferFunction::HLG)));
    EXPECT_GT(ColorTransferFunction::decode(overflowingCode, TransferFunction::HLG), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3OetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "S-Log3 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::SonySLog3),
                    slog3OetfReference(normalized), 2.0e-6);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::SonySLog3),
                    slog3EotfReference(normalized), 1.5e-5);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3TenBitCodesRoundTripToBlackFloorOrOriginalCode)
{
    constexpr int blackCode = 95;
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::SonySLog3);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::SonySLog3);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        SCOPED_TRACE(::testing::Message()
            << "S-Log3 code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, std::max(code, blackCode));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3SixteenBitCodesRoundTripToBlackFloorOrOriginalCode)
{
    constexpr int maximumCode = 65535;
    const int blackCode = static_cast<int>(std::lround(95.0 * maximumCode / 1023.0));
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::SonySLog3);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::SonySLog3);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        EXPECT_EQ(roundTripCode, std::max(code, blackCode)) << "16-bit code=" << code;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3OetfBranchMatchesAroundLinearBreakpoint)
{
    constexpr double breakpoint = 0.01125;
    for (const double sample : {
             std::nextafter(breakpoint, 0.0), breakpoint,
             std::nextafter(breakpoint, 1.0)}) {
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(sample), TransferFunction::SonySLog3),
                    slog3OetfReference(sample), 2.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3AdjacentFloatBreakpointsMatchTheirSelectedBranches)
{
    constexpr float linearBreakpoint = 0.01125f;
    constexpr float encodedBreakpoint = 171.2102946929f / 1023.0f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f)};
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f)};
    const auto oetfReferenceAtFloatBreakpoint = [=](const float value) {
        constexpr double codeAtBreakpoint = 171.2102946929;
        return value >= linearBreakpoint
            ? (420.0 + std::log10((static_cast<double>(value) + 0.01) / 0.19) * 261.5) / 1023.0
            : (static_cast<double>(value) * (codeAtBreakpoint - 95.0) /
               static_cast<double>(linearBreakpoint) + 95.0) / 1023.0;
    };
    const auto eotfReferenceAtFloatBreakpoint = [=](const float value) {
        constexpr double codeAtBreakpoint = 171.2102946929;
        const double code = static_cast<double>(value) * 1023.0;
        return value >= encodedBreakpoint
            ? std::pow(10.0, (code - 420.0) / 261.5) * 0.19 - 0.01
            : (code - 95.0) * 0.01125 / (codeAtBreakpoint - 95.0);
    };

    float previousEncoded = -1.0f;
    for (const float linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::SonySLog3);
        EXPECT_NEAR(encoded, oetfReferenceAtFloatBreakpoint(linear), 2.0e-7);
        EXPECT_GE(encoded, previousEncoded);
        previousEncoded = encoded;
    }

    float previousLinear = -1.0f;
    for (const float encoded : encodedSamples) {
        const float linear = ColorTransferFunction::decode(
            encoded, TransferFunction::SonySLog3);
        EXPECT_NEAR(linear, eotfReferenceAtFloatBreakpoint(encoded), 2.0e-7);
        EXPECT_GE(linear, previousLinear);
        previousLinear = linear;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3DecodeMatchesReferenceOutsideNormalizedCodeRange)
{
    constexpr std::array<double, 7> encodedSamples = {
        -1.0, -0.1, 0.0, 95.0 / 1023.0,
        0.5, 1.0, 1.25,
    };
    for (const double encoded : encodedSamples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::SonySLog3);
        const double expected = slog3EotfReference(encoded);
        SCOPED_TRACE(::testing::Message() << "S-Log3 code=" << encoded);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, std::max(2.0e-7, std::abs(expected) * 3.0e-6));
    }

    EXPECT_LT(ColorTransferFunction::decode(-0.1f, TransferFunction::SonySLog3), 0.0f);
    EXPECT_GT(ColorTransferFunction::decode(1.25f, TransferFunction::SonySLog3), 1.0f);
    EXPECT_NEAR(ColorTransferFunction::encode(-1.0f, TransferFunction::SonySLog3),
                95.0 / 1023.0, 2.0e-7);
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3SubnormalLinearValuesCollapseToBlackCode)
{
    constexpr float blackCode = 95.0f / 1023.0f;
    const std::array<float, 4> tinyPositiveValues = {
        0.0f,
        std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),
        std::nextafter(0.0f, 1.0f),
    };
    for (const float linear : tinyPositiveValues) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(linear, TransferFunction::SonySLog3),
                        blackCode);
    }

    const std::array<float, 3> codes = {
        std::nextafter(blackCode, 0.0f),
        blackCode,
        std::nextafter(blackCode, 1.0f),
    };
    for (const float code : codes) {
        EXPECT_NEAR(ColorTransferFunction::decode(code, TransferFunction::SonySLog3),
                    slog3EotfReference(code), 2.0e-7);
    }
    EXPECT_LT(ColorTransferFunction::decode(codes[0], TransferFunction::SonySLog3), 0.0f);
    EXPECT_NEAR(ColorTransferFunction::decode(blackCode, TransferFunction::SonySLog3),
                0.0f, 1.0e-8f);
    EXPECT_GT(ColorTransferFunction::decode(codes[2], TransferFunction::SonySLog3), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3ExtremeFiniteInputsExposeClampAndOverflowBranches)
{
    const float maximum = std::numeric_limits<float>::max();
    const float largeFinite = maximum / 64.0f;
    const float encodedLarge = ColorTransferFunction::encode(
        largeFinite, TransferFunction::SonySLog3);
    const double encodedLargeReference =
        (420.0 + std::log10((static_cast<double>(largeFinite) + 0.01) / 0.19) * 261.5) /
        1023.0;
    EXPECT_TRUE(std::isfinite(encodedLarge));
    EXPECT_NEAR(encodedLarge, encodedLargeReference, 2.0e-6);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(maximum, TransferFunction::SonySLog3)));
    EXPECT_GT(ColorTransferFunction::encode(maximum, TransferFunction::SonySLog3), 0.0f);
    EXPECT_NEAR(ColorTransferFunction::encode(-maximum, TransferFunction::SonySLog3),
                95.0 / 1023.0, 2.0e-7);

    const float decodedPositiveMaximum = ColorTransferFunction::decode(
        maximum, TransferFunction::SonySLog3);
    EXPECT_TRUE(std::isinf(decodedPositiveMaximum));
    EXPECT_GT(decodedPositiveMaximum, 0.0f);
    const float decodedNegativeMaximum = ColorTransferFunction::decode(
        -maximum, TransferFunction::SonySLog3);
    EXPECT_TRUE(std::isinf(decodedNegativeMaximum));
    EXPECT_LT(decodedNegativeMaximum, 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     SLog3RoundTripsAcrossToeReferenceGrayAndHdrRange)
{
    constexpr double breakpoint = 0.01125;
    const std::array<double, 12> linearSamples = {
        0.0,
        1.0e-6,
        0.001,
        std::nextafter(breakpoint, 0.0),
        breakpoint,
        std::nextafter(breakpoint, 1.0),
        0.018,
        0.18,
        0.5,
        1.0,
        4.0,
        16.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::SonySLog3);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::SonySLog3);
        SCOPED_TRACE(::testing::Message() << "S-Log3 scene-linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, linear * 2.0e-5));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3OetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "Canon Log 3 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::CanonLog3),
                    canonLog3OetfReference(normalized), 1.5e-5);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::CanonLog3),
                    canonLog3EotfReference(normalized), 1.5e-5);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3TenBitCodeRoundTripsMatchReferenceAcrossThreeBranches)
{
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::CanonLog3);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::CanonLog3);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        const int referenceCode = static_cast<int>(std::lround(
            canonLog3OetfReference(canonLog3EotfReference(encodedInput)) * 1023.0));
        SCOPED_TRACE(::testing::Message()
            << "Canon Log 3 code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, referenceCode);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3SixteenBitCodeRoundTripsMatchReferenceAcrossThreeBranches)
{
    constexpr int maximumCode = 65535;
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::CanonLog3);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::CanonLog3);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        const int referenceCode = static_cast<int>(std::lround(
            canonLog3OetfReference(canonLog3EotfReference(encodedInput)) * maximumCode));
        EXPECT_EQ(roundTripCode, referenceCode) << "16-bit code=" << code;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3LowToeBreakpointDiscontinuityIsCharacterized)
{
    constexpr float lowEncoded = 0.04076162f;
    constexpr float toe = 0.069886632f;
    constexpr float slope = 0.42889912f;
    constexpr float scale = 14.98325f;
    const float lowLinear = -(std::pow(10.0f, (toe - lowEncoded) / slope) - 1.0f) / scale;
    const float belowLinear = std::nextafter(lowLinear, -std::numeric_limits<float>::infinity());
    const float encodedBelowLinear = ColorTransferFunction::encode(
        belowLinear, TransferFunction::CanonLog3);
    const float encodedAtLinear = ColorTransferFunction::encode(
        lowLinear, TransferFunction::CanonLog3);

    EXPECT_NEAR(encodedBelowLinear, static_cast<float>(lowEncoded), 2.0e-6f);
    EXPECT_NEAR(encodedAtLinear,
                2.3069815f * lowLinear + 0.073059361f, 2.0e-6f);
    EXPECT_GT(encodedAtLinear - encodedBelowLinear, 0.006f);

    const float encodedJustBelow = std::nextafter(lowEncoded, 0.0f);
    const float decodedBelow = ColorTransferFunction::decode(
        encodedJustBelow, TransferFunction::CanonLog3);
    const float decodedAt = ColorTransferFunction::decode(
        lowEncoded, TransferFunction::CanonLog3);
    EXPECT_NEAR(decodedBelow,
                -(std::pow(10.0f, (toe - encodedJustBelow) / slope) - 1.0f) / scale,
                2.0e-6f);
    EXPECT_NEAR(decodedAt, (lowEncoded - 0.073059361f) / 2.3069815f, 2.0e-6f);
    EXPECT_GT(std::abs(decodedAt - decodedBelow), 0.002f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3AllFloatBreakpointsMatchTheirSelectedBranches)
{
    constexpr float low = 0.04076162f;
    constexpr float high = 0.105357102f;
    constexpr float toe = 0.069886632f;
    constexpr float slope = 0.42889912f;
    constexpr float scale = 14.98325f;
    const float lowLinear = -(std::pow(10.0f, (toe - low) / slope) - 1.0f) / scale;
    const float highLinear = (high - 0.073059361f) / 2.3069815f;
    const auto around = [](const float breakpoint) {
        return std::array<float, 3>{
            std::nextafter(breakpoint, -std::numeric_limits<float>::infinity()),
            breakpoint,
            std::nextafter(breakpoint, std::numeric_limits<float>::infinity()),
        };
    };
    const auto oetfReference = [=](const float value) {
        const double input = value;
        if (value < lowLinear)
            return -(static_cast<double>(slope) * std::log10(-input * scale + 1.0) - toe);
        if (value <= highLinear)
            return 2.3069815 * input + 0.073059361;
        return static_cast<double>(slope) * std::log10(input * scale + 1.0) + toe;
    };
    const auto eotfReference = [=](const float value) {
        const double input = value;
        if (value < low)
            return -(std::pow(10.0, (static_cast<double>(toe) - input) / slope) - 1.0) / scale;
        if (value <= high)
            return (input - 0.073059361) / 2.3069815;
        return (std::pow(10.0, (input - toe) / slope) - 1.0) / scale;
    };

    const std::array<float, 6> linearBreakpoints = {
        lowLinear, highLinear,
        std::nextafter(lowLinear, -std::numeric_limits<float>::infinity()),
        std::nextafter(lowLinear, std::numeric_limits<float>::infinity()),
        std::nextafter(highLinear, -std::numeric_limits<float>::infinity()),
        std::nextafter(highLinear, std::numeric_limits<float>::infinity()),
    };
    for (const float linear : linearBreakpoints) {
        EXPECT_NEAR(ColorTransferFunction::encode(linear, TransferFunction::CanonLog3),
                    oetfReference(linear), 2.0e-7);
    }
    for (const float code : {low, high}) {
        for (const float encoded : around(code)) {
            EXPECT_NEAR(ColorTransferFunction::decode(encoded, TransferFunction::CanonLog3),
                        eotfReference(encoded), 2.0e-7);
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3FloatToeAndLinearConnectionsCharacterizeTheirJumps)
{
    constexpr float low = 0.04076162f;
    constexpr float high = 0.105357102f;
    constexpr float toe = 0.069886632f;
    constexpr float slope = 0.42889912f;
    constexpr float scale = 14.98325f;
    const float lowLinear = -(std::pow(10.0f, (toe - low) / slope) - 1.0f) / scale;
    const float highLinear = (high - 0.073059361f) / 2.3069815f;

    const float encodedBelowLowLinear = ColorTransferFunction::encode(
        std::nextafter(lowLinear, -std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    const float encodedAtLowLinear = ColorTransferFunction::encode(
        lowLinear, TransferFunction::CanonLog3);
    EXPECT_GT(encodedAtLowLinear - encodedBelowLowLinear, 0.006f);

    const float encodedBelowHighLinear = ColorTransferFunction::encode(
        std::nextafter(highLinear, -std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    const float encodedAboveHighLinear = ColorTransferFunction::encode(
        std::nextafter(highLinear, std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    EXPECT_LT(std::abs(encodedAboveHighLinear - encodedBelowHighLinear), 1.0e-6f);

    const float decodedBelowLowCode = ColorTransferFunction::decode(
        std::nextafter(low, 0.0f), TransferFunction::CanonLog3);
    const float decodedAtLowCode = ColorTransferFunction::decode(
        low, TransferFunction::CanonLog3);
    EXPECT_GT(std::abs(decodedAtLowCode - decodedBelowLowCode), 0.002f);

    const float decodedBelowHighCode = ColorTransferFunction::decode(
        std::nextafter(high, 0.0f), TransferFunction::CanonLog3);
    const float decodedAboveHighCode = ColorTransferFunction::decode(
        std::nextafter(high, 1.0f), TransferFunction::CanonLog3);
    EXPECT_LT(std::abs(decodedAboveHighCode - decodedBelowHighCode), 1.0e-6f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3RoundTripsWithinContinuousToeAndLogRegions)
{
    constexpr double lowCode = 0.04076162;
    constexpr double highCode = 0.105357102;
    constexpr double toe = 0.069886632;
    constexpr double slope = 0.42889912;
    constexpr double scale = 14.98325;
    const double lowLinear = -(std::pow(10.0, (toe - lowCode) / slope) - 1.0) / scale;
    const double highLinear = (highCode - 0.073059361) / 2.3069815;
    const std::array<double, 12> linearSamples = {
        -0.1,
        lowLinear - 1.0e-4,
        lowLinear - 1.0e-6,
        -0.001,
        0.0,
        0.001,
        highLinear - 1.0e-5,
        highLinear,
        highLinear + 1.0e-5,
        0.18,
        1.0,
        16.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::CanonLog3);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::CanonLog3);
        SCOPED_TRACE(::testing::Message() << "Canon Log 3 linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, std::abs(linear) * 3.0e-5));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3MatchesReferencesOutsideNormalizedCodeAndLinearRanges)
{
    constexpr std::array<double, 7> encodedSamples = {
        -1.0, -0.1, 0.0, 0.04076162, 0.5, 1.0, 1.25,
    };
    for (const double encoded : encodedSamples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::CanonLog3);
        const double expected = canonLog3EotfReference(encoded);
        SCOPED_TRACE(::testing::Message() << "Canon Log 3 code=" << encoded);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, std::max(2.0e-7, std::abs(expected) * 3.0e-6));
    }

    constexpr std::array<double, 6> linearSamples = {
        -1.0, -0.1, -0.01, 0.0, 1.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float actual = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::CanonLog3);
        const double expected = canonLog3OetfReference(linear);
        SCOPED_TRACE(::testing::Message() << "Canon Log 3 linear=" << linear);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, std::max(2.0e-7, std::abs(expected) * 3.0e-6));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog3ExtremeFiniteInputsExposeSignedFloatOverflow)
{
    const float maximum = std::numeric_limits<float>::max();
    const float largeFinite = maximum / 32.0f;
    const float encodedLarge = ColorTransferFunction::encode(
        largeFinite, TransferFunction::CanonLog3);
    EXPECT_TRUE(std::isfinite(encodedLarge));
    EXPECT_NEAR(encodedLarge,
                canonLog3OetfReference(static_cast<double>(largeFinite)), 2.0e-6);

    const float encodedPositiveMaximum = ColorTransferFunction::encode(
        maximum, TransferFunction::CanonLog3);
    const float encodedNegativeMaximum = ColorTransferFunction::encode(
        -maximum, TransferFunction::CanonLog3);
    EXPECT_TRUE(std::isinf(encodedPositiveMaximum));
    EXPECT_GT(encodedPositiveMaximum, 0.0f);
    EXPECT_TRUE(std::isinf(encodedNegativeMaximum));
    EXPECT_LT(encodedNegativeMaximum, 0.0f);

    const float decodedPositiveMaximum = ColorTransferFunction::decode(
        maximum, TransferFunction::CanonLog3);
    const float decodedNegativeMaximum = ColorTransferFunction::decode(
        -maximum, TransferFunction::CanonLog3);
    EXPECT_TRUE(std::isinf(decodedPositiveMaximum));
    EXPECT_GT(decodedPositiveMaximum, 0.0f);
    EXPECT_TRUE(std::isinf(decodedNegativeMaximum));
    EXPECT_LT(decodedNegativeMaximum, 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     LogCurvesCharacterizeNanAndSignedInfinityInputs)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;
    constexpr std::array<TransferFunction, 3> curves = {
        TransferFunction::SonySLog3,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };

    for (const TransferFunction curve : curves) {
        SCOPED_TRACE(static_cast<int>(curve));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::encode(nan, curve)));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::decode(nan, curve)));
        EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(positiveInfinity, curve)));
        EXPECT_GT(ColorTransferFunction::encode(positiveInfinity, curve), 0.0f);
        EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(positiveInfinity, curve)));
        EXPECT_GT(ColorTransferFunction::decode(positiveInfinity, curve), 0.0f);
        EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(negativeInfinity, curve)));
        EXPECT_LT(ColorTransferFunction::decode(negativeInfinity, curve), 0.0f);
    }

    EXPECT_NEAR(ColorTransferFunction::encode(negativeInfinity, TransferFunction::SonySLog3),
                95.0 / 1023.0, 2.0e-7);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        negativeInfinity, TransferFunction::CanonLog2)));
    EXPECT_LT(ColorTransferFunction::encode(negativeInfinity, TransferFunction::CanonLog2), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        negativeInfinity, TransferFunction::CanonLog3)));
    EXPECT_LT(ColorTransferFunction::encode(negativeInfinity, TransferFunction::CanonLog3), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonAndAcesLogCurvesCharacterizeNanAndSignedInfinity)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;

    for (const TransferFunction curve : {
             TransferFunction::Cineon,
             TransferFunction::ACEScc,
             TransferFunction::ACEScct,
         }) {
        SCOPED_TRACE(static_cast<int>(curve));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::encode(nan, curve)));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::decode(nan, curve)));
    }

    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::Cineon)));
    EXPECT_GT(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::Cineon), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::Cineon)));
    EXPECT_GT(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::Cineon), 0.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(
        negativeInfinity, TransferFunction::Cineon),
        ColorTransferFunction::encode(0.0f, TransferFunction::Cineon));
    EXPECT_LT(ColorTransferFunction::decode(
        negativeInfinity, TransferFunction::Cineon), 0.0f);

    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::ACEScc)));
    EXPECT_LT(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::ACEScc), 0.0f);
    EXPECT_TRUE(std::isfinite(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::ACEScc)));
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(
                        negativeInfinity, TransferFunction::ACEScc),
                    ColorTransferFunction::encode(0.0f, TransferFunction::ACEScc));
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(
        negativeInfinity, TransferFunction::ACEScc)));
    EXPECT_GT(ColorTransferFunction::decode(
        negativeInfinity, TransferFunction::ACEScc), 0.0f);

    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::ACEScct)));
    EXPECT_GT(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::ACEScct), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::ACEScct)));
    EXPECT_GT(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::ACEScct), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        negativeInfinity, TransferFunction::ACEScct)));
    EXPECT_LT(ColorTransferFunction::encode(
        negativeInfinity, TransferFunction::ACEScct), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(
        negativeInfinity, TransferFunction::ACEScct)));
    EXPECT_LT(ColorTransferFunction::decode(
        negativeInfinity, TransferFunction::ACEScct), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonOetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "Cineon 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::Cineon),
                    cineonOetfReference(normalized), 2.0e-7);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::Cineon),
                    cineonEotfReference(normalized), 3.0e-6);
    }
    EXPECT_NEAR(ColorTransferFunction::encode(0.0f, TransferFunction::Cineon),
                95.0 / 1023.0, 2.0e-7);
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonQuantizedTenBitCodesRoundTripToTheirRepresentableBlackFloor)
{
    constexpr int blackCode = 95;
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::Cineon);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::Cineon);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        const int expectedCode = std::max(code, blackCode);
        SCOPED_TRACE(::testing::Message()
            << "Cineon input code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, expectedCode);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonSixteenBitCodesRoundTripToTheirRepresentableBlackFloor)
{
    constexpr int maximumCode = 65535;
    const int blackCode = static_cast<int>(std::lround(95.0 * maximumCode / 1023.0));
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(encodedInput, TransferFunction::Cineon);
        const float encodedOutput = ColorTransferFunction::encode(linear, TransferFunction::Cineon);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        EXPECT_EQ(roundTripCode, std::max(code, blackCode)) << "16-bit code=" << code;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonNegativeAndHdrRangesMatchIndependentTransferEquations)
{
    constexpr std::array<double, 10> linearSamples = {
        -1.0, -0.25, -0.01, 0.0, 1.0e-5,
        0.01, 0.18, 1.0, 10.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float actual = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::Cineon);
        const double expected = cineonOetfReference(linear);
        SCOPED_TRACE(::testing::Message() << "Cineon linear=" << linear);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, 2.0e-7);
    }

    constexpr std::array<double, 8> encodedSamples = {
        -0.1, 0.0, 95.0 / 1023.0, 0.2,
        0.5, 0.75, 1.0, 1.25,
    };
    for (const double encoded : encodedSamples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::Cineon);
        const double expected = cineonEotfReference(encoded);
        SCOPED_TRACE(::testing::Message() << "Cineon code=" << encoded);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, std::max(3.0e-6, std::abs(expected) * 2.0e-6));
    }

    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-1.0f, TransferFunction::Cineon),
                    static_cast<float>(95.0 / 1023.0));
    EXPECT_LT(ColorTransferFunction::decode(0.0f, TransferFunction::Cineon), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonBlackCodeAdjacentFloatsMatchReferenceAndRemainOrdered)
{
    constexpr float blackCode = 95.0f / 1023.0f;
    const std::array<float, 3> codes = {
        std::nextafter(blackCode, 0.0f),
        blackCode,
        std::nextafter(blackCode, 1.0f),
    };
    std::array<float, 3> decoded{};
    for (std::size_t i = 0; i < codes.size(); ++i) {
        decoded[i] = ColorTransferFunction::decode(codes[i], TransferFunction::Cineon);
        SCOPED_TRACE(::testing::Message() << "Cineon code=" << codes[i]);
        EXPECT_NEAR(decoded[i], cineonEotfReference(codes[i]), 2.0e-7);
    }

    EXPECT_FLOAT_EQ(decoded[0], decoded[1]);
    EXPECT_FLOAT_EQ(decoded[1], decoded[2]);
    EXPECT_NEAR(decoded[1], -9.41488554e-10f, 2.0e-12f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonBlackOffsetMatchesReferenceAtSignedZeroAndSubnormalInputs)
{
    const float subnormal = std::numeric_limits<float>::denorm_min();
    const std::array<float, 4> samples = {-subnormal, -0.0f, 0.0f, subnormal};
    for (const float sample : samples) {
        const float encoded = ColorTransferFunction::encode(sample, TransferFunction::Cineon);
        const float decoded = ColorTransferFunction::decode(sample, TransferFunction::Cineon);
        SCOPED_TRACE(::testing::Message() << "Cineon sample=" << sample);
        EXPECT_NEAR(encoded, cineonOetfReference(sample), 2.0e-7);
        EXPECT_NEAR(decoded, cineonEotfReference(sample),
                    std::max(2.0e-7, std::abs(cineonEotfReference(sample)) * 2.0e-6));
    }

    const float blackCode = ColorTransferFunction::encode(0.0f, TransferFunction::Cineon);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-subnormal, TransferFunction::Cineon),
                    blackCode);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-0.0f, TransferFunction::Cineon),
                    blackCode);
    EXPECT_NE(ColorTransferFunction::decode(0.0f, TransferFunction::Cineon), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonRoundTripsZeroAndPositiveSceneLinearValues)
{
    constexpr std::array<float, 10> linearSamples = {
        0.0f, 1.0e-8f, 1.0e-5f, 1.0e-4f, 0.001f,
        0.01f, 0.18f, 1.0f, 16.0f, 100.0f,
    };
    for (const float linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(linear, TransferFunction::Cineon);
        const float decoded = ColorTransferFunction::decode(encoded, TransferFunction::Cineon);
        const double expectedEncoded = cineonOetfReference(linear);
        const double expectedDecoded = cineonEotfReference(expectedEncoded);
        SCOPED_TRACE(::testing::Message() << "Cineon scene-linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(encoded, expectedEncoded, 2.0e-7);
        EXPECT_NEAR(decoded, expectedDecoded,
                    std::max(2.0e-7, std::abs(expectedDecoded) * 3.0e-6));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, linear * 3.0e-6));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CineonExtremeFiniteInputsExposeEncodeAndDecodeFloatLimits)
{
    constexpr double blackOffset = 0.0107977516232771;
    const float maximum = std::numeric_limits<float>::max();
    const float minimum = -maximum;

    const float encodedMaximum = ColorTransferFunction::encode(maximum, TransferFunction::Cineon);
    const double encodedMaximumReference =
        (685.0 + 300.0 * std::log10(static_cast<double>(maximum) * (1.0 - blackOffset) +
                                   blackOffset)) / 1023.0;
    EXPECT_TRUE(std::isfinite(encodedMaximum));
    EXPECT_NEAR(encodedMaximum, encodedMaximumReference, 2.0e-6);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(minimum, TransferFunction::Cineon),
                    ColorTransferFunction::encode(0.0f, TransferFunction::Cineon));

    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(maximum, TransferFunction::Cineon)));
    const float decodedMinimum = ColorTransferFunction::decode(minimum, TransferFunction::Cineon);
    EXPECT_TRUE(std::isfinite(decodedMinimum));
    EXPECT_NEAR(decodedMinimum, -blackOffset / (1.0 - blackOffset), 2.0e-7);
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2OetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "Canon Log 2 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::CanonLog2),
                    canonLog2OetfReference(normalized), 2.0e-6);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::CanonLog2),
                    canonLog2EotfReference(normalized), 2.0e-5);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2TenBitCodeRoundTripsMatchReferenceAcrossToeTransition)
{
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::CanonLog2);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::CanonLog2);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        const int referenceCode = static_cast<int>(std::lround(
            canonLog2OetfReference(canonLog2EotfReference(encodedInput)) * 1023.0));
        SCOPED_TRACE(::testing::Message()
            << "Canon Log 2 code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, referenceCode);
        if (code < 29)
            EXPECT_NE(roundTripCode, code);
        else
            EXPECT_EQ(roundTripCode, code);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2SixteenBitCodeRoundTripsMatchReferenceAcrossToeTransition)
{
    constexpr int maximumCode = 65535;
    int nonIdentityRoundTripCount = 0;
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::CanonLog2);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::CanonLog2);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        const int referenceCode = static_cast<int>(std::lround(
            canonLog2OetfReference(canonLog2EotfReference(encodedInput)) * maximumCode));
        EXPECT_EQ(roundTripCode, referenceCode) << "16-bit code=" << code;
        nonIdentityRoundTripCount += roundTripCode != code;
    }
    EXPECT_GT(nonIdentityRoundTripCount, 0);
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2NegativeLinearToeOetfDiscontinuityIsCharacterized)
{
    constexpr double toe = 0.035388128;
    constexpr double slope = 0.281863093;
    constexpr double scale = 87.09937546;
    constexpr float toeFloat = 0.035388128f;
    constexpr float slopeFloat = 0.281863093f;
    constexpr float scaleFloat = 87.09937546f;
    const float linearToe = -(std::pow(10.0f, toeFloat / slopeFloat) - 1.0f) / scaleFloat;
    const std::array<double, 3> encodedSamples = {
        std::nextafter(toe, 0.0), toe, std::nextafter(toe, 1.0)};

    const float belowLinear = std::nextafter(
        linearToe, -std::numeric_limits<float>::infinity());
    const float atLinearToe = linearToe;
    const float encodedBelow = ColorTransferFunction::encode(
        belowLinear, TransferFunction::CanonLog2);
    const float encodedAt = ColorTransferFunction::encode(
        atLinearToe, TransferFunction::CanonLog2);
    EXPECT_NEAR(encodedBelow, canonLog2OetfReference(belowLinear), 2.0e-7);
    EXPECT_NEAR(encodedAt,
                slope * std::log10(atLinearToe * scale + 1.0) + toe, 2.0e-7);
    EXPECT_GT(std::abs(encodedAt - encodedBelow), 0.014f);

    for (const double sample : encodedSamples) {
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(sample), TransferFunction::CanonLog2),
                    canonLog2EotfReference(sample), 2.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2FloatToeNeighborsMatchTheirSelectedBranches)
{
    constexpr float toe = 0.035388128f;
    constexpr float slope = 0.281863093f;
    constexpr float scale = 87.09937546f;
    const float linearBreakpoint = -(std::pow(10.0f, toe / slope) - 1.0f) / scale;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, -std::numeric_limits<float>::infinity()),
        linearBreakpoint,
        std::nextafter(linearBreakpoint, std::numeric_limits<float>::infinity()),
    };
    const std::array<float, 3> encodedSamples = {
        std::nextafter(toe, 0.0f), toe, std::nextafter(toe, 1.0f),
    };
    const auto oetfReferenceAtFloatBreakpoint = [=](const float value) {
        const double input = static_cast<double>(value);
        return value < linearBreakpoint
            ? -(static_cast<double>(slope) * std::log10(-input * scale + 1.0) - toe)
            : static_cast<double>(slope) * std::log10(input * scale + 1.0) + toe;
    };
    const auto eotfReferenceAtFloatBreakpoint = [=](const float value) {
        const double input = static_cast<double>(value);
        return value < toe
            ? -(std::pow(10.0, (static_cast<double>(toe) - input) / slope) - 1.0) / scale
            : (std::pow(10.0, (input - static_cast<double>(toe)) / slope) - 1.0) / scale;
    };

    std::array<float, 3> encodedResults{};
    for (std::size_t index = 0; index < linearSamples.size(); ++index) {
        encodedResults[index] = ColorTransferFunction::encode(
            linearSamples[index], TransferFunction::CanonLog2);
        EXPECT_NEAR(encodedResults[index],
                    oetfReferenceAtFloatBreakpoint(linearSamples[index]), 2.0e-7);
    }
    EXPECT_GT(std::abs(encodedResults[1] - encodedResults[0]), 0.014f);
    EXPECT_NEAR(encodedResults[2], encodedResults[1], 2.0e-7f);

    float previousLinear = -std::numeric_limits<float>::infinity();
    for (const float encoded : encodedSamples) {
        const float linear = ColorTransferFunction::decode(
            encoded, TransferFunction::CanonLog2);
        EXPECT_NEAR(linear, eotfReferenceAtFloatBreakpoint(encoded), 2.0e-7);
        EXPECT_GE(linear, previousLinear);
        previousLinear = linear;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2RoundTripsNegativeToeAndPositiveHdrRegions)
{
    constexpr double toeCode = 0.035388128;
    constexpr double slope = 0.281863093;
    constexpr double scale = 87.09937546;
    const double negativeToeLinear = -(std::pow(10.0, toeCode / slope) - 1.0) / scale;
    const std::array<double, 15> linearSamples = {
        -0.1,
        -0.01,
        negativeToeLinear - 1.0e-4,
        0.0,
        1.0e-6,
        0.001,
        0.01,
        0.05,
        0.18,
        0.5,
        1.0,
        4.0,
        16.0,
        100.0,
        1000.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::CanonLog2);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::CanonLog2);
        SCOPED_TRACE(::testing::Message() << "Canon Log 2 scene-linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, std::abs(linear) * 3.0e-5));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2NegativeValuesBetweenToeAndZeroMatchPiecewiseReferences)
{
    constexpr double toe = 0.035388128;
    constexpr double slope = 0.281863093;
    constexpr double scale = 87.09937546;
    const double negativeToeLinear = -(std::pow(10.0, toe / slope) - 1.0) / scale;
    for (const double linear : {negativeToeLinear + 1.0e-4, -0.01, -0.005, -0.001, -1.0e-6, 0.0}) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::CanonLog2);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::CanonLog2);
        SCOPED_TRACE(::testing::Message() << "Canon Log 2 linear=" << linear);
        EXPECT_NEAR(encoded, canonLog2OetfReference(linear), 2.0e-7);
        EXPECT_NEAR(decoded,
                    canonLog2EotfReference(canonLog2OetfReference(linear)),
                    std::max(2.0e-7, std::abs(linear) * 3.0e-5));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CanonLog2ExtremeFiniteInputsExposeSignedFloatOverflow)
{
    const float maximum = std::numeric_limits<float>::max();
    const float largeFinite = maximum / 256.0f;
    const float encodedLarge = ColorTransferFunction::encode(
        largeFinite, TransferFunction::CanonLog2);
    EXPECT_TRUE(std::isfinite(encodedLarge));
    EXPECT_NEAR(encodedLarge,
                canonLog2OetfReference(static_cast<double>(largeFinite)), 2.0e-6);

    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(maximum, TransferFunction::CanonLog2)));
    EXPECT_GT(ColorTransferFunction::encode(maximum, TransferFunction::CanonLog2), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(-maximum, TransferFunction::CanonLog2)));
    EXPECT_LT(ColorTransferFunction::encode(-maximum, TransferFunction::CanonLog2), 0.0f);

    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(maximum, TransferFunction::CanonLog2)));
    EXPECT_GT(ColorTransferFunction::decode(maximum, TransferFunction::CanonLog2), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(-maximum, TransferFunction::CanonLog2)));
    EXPECT_LT(ColorTransferFunction::decode(-maximum, TransferFunction::CanonLog2), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateOetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "DaVinci Intermediate 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::DaVinciIntermediate),
                    daVinciIntermediateOetfReference(normalized), 1.0e-6);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::DaVinciIntermediate),
                    daVinciIntermediateEotfReference(normalized), 2.0e-5);
    }
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(0.0f, TransferFunction::DaVinciIntermediate),
                    0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateTenBitCodesRoundTripThroughSceneLinear)
{
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::DaVinciIntermediate);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::DaVinciIntermediate);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        const int referenceCode = static_cast<int>(std::lround(
            daVinciIntermediateOetfReference(
                daVinciIntermediateEotfReference(encodedInput)) * 1023.0));
        SCOPED_TRACE(::testing::Message()
            << "DaVinci Intermediate code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, referenceCode);
        EXPECT_EQ(roundTripCode, code);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateSixteenBitCodesRoundTripThroughSceneLinear)
{
    constexpr int maximumCode = 65535;
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::DaVinciIntermediate);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::DaVinciIntermediate);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        EXPECT_EQ(roundTripCode, code) << "16-bit code=" << code;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateRoundTripsPositiveSceneValuesAcrossHdrRange)
{
    constexpr std::array<double, 13> linearSamples = {
        1.0e-6, 1.0e-5, 1.0e-4, 0.001, 0.01, 0.05, 0.18,
        0.5, 1.0, 4.0, 16.0, 100.0, 1000.0,
    };
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(
                        0.0f, TransferFunction::DaVinciIntermediate),
                    0.0f);
    EXPECT_NE(ColorTransferFunction::decode(
                  0.0f, TransferFunction::DaVinciIntermediate),
              0.0f);

    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::DaVinciIntermediate);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::DaVinciIntermediate);
        SCOPED_TRACE(::testing::Message() << "DaVinci Intermediate linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, linear * 3.0e-5));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateDecodeMatchesExponentialReferenceOutsideNominalRange)
{
    constexpr std::array<double, 9> encodedSamples = {
        -2.0, -1.0, -0.5, -0.1, 0.0, 0.1, 0.5, 1.0, 1.5,
    };
    for (const double encoded : encodedSamples) {
        const float actual = ColorTransferFunction::decode(
            static_cast<float>(encoded), TransferFunction::DaVinciIntermediate);
        const double expected = daVinciIntermediateEotfReference(encoded);
        SCOPED_TRACE(::testing::Message() << "DaVinci Intermediate code=" << encoded);
        EXPECT_TRUE(std::isfinite(actual));
        EXPECT_NEAR(actual, expected, std::max(1.0e-7, expected * 3.0e-6));
    }

    EXPECT_GT(ColorTransferFunction::decode(-2.0f, TransferFunction::DaVinciIntermediate), 0.0f);
    EXPECT_GT(ColorTransferFunction::decode(1.5f, TransferFunction::DaVinciIntermediate), 1.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateZeroAndAdjacentFloatInputsExposeToeDiscontinuity)
{
    const float negativeZero = -0.0f;
    const float negativeSubnormal = -std::numeric_limits<float>::denorm_min();
    const float positiveSubnormal = std::numeric_limits<float>::denorm_min();
    const std::array<float, 5> linearSamples = {
        -std::numeric_limits<float>::min(), negativeSubnormal,
        negativeZero, positiveSubnormal, std::numeric_limits<float>::min(),
    };

    for (const float linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::DaVinciIntermediate);
        EXPECT_NEAR(encoded, daVinciIntermediateOetfReference(linear), 2.0e-6);
        if (linear <= 0.0f)
            EXPECT_FLOAT_EQ(encoded, 0.0f);
        else
            EXPECT_LT(encoded, 0.0f);
    }

    const float encodedAtZero = ColorTransferFunction::encode(
        0.0f, TransferFunction::DaVinciIntermediate);
    const float encodedAtSmallestPositive = ColorTransferFunction::encode(
        positiveSubnormal, TransferFunction::DaVinciIntermediate);
    EXPECT_GT(std::abs(encodedAtSmallestPositive - encodedAtZero), 10.0f);

    const std::array<float, 5> encodedSamples = {
        -positiveSubnormal, negativeZero, 0.0f, positiveSubnormal,
        std::numeric_limits<float>::min(),
    };
    float previousLinear = -1.0f;
    for (const float encoded : encodedSamples) {
        const float linear = ColorTransferFunction::decode(
            encoded, TransferFunction::DaVinciIntermediate);
        EXPECT_NEAR(linear, daVinciIntermediateEotfReference(encoded),
                    std::max(2.0e-7, std::abs(static_cast<double>(linear)) * 3.0e-6));
        EXPECT_GE(linear, previousLinear);
        previousLinear = linear;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     DaVinciIntermediateCharacterizesNanAndSignedInfinityInputs)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;

    EXPECT_TRUE(std::isnan(ColorTransferFunction::encode(
        nan, TransferFunction::DaVinciIntermediate)));
    EXPECT_TRUE(std::isnan(ColorTransferFunction::decode(
        nan, TransferFunction::DaVinciIntermediate)));
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::DaVinciIntermediate)));
    EXPECT_GT(ColorTransferFunction::encode(
        positiveInfinity, TransferFunction::DaVinciIntermediate), 0.0f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::DaVinciIntermediate)));
    EXPECT_GT(ColorTransferFunction::decode(
        positiveInfinity, TransferFunction::DaVinciIntermediate), 0.0f);

    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(
        negativeInfinity, TransferFunction::DaVinciIntermediate), 0.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::decode(
        negativeInfinity, TransferFunction::DaVinciIntermediate), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctOetfAndEotfMatchIndependentReferenceAcrossTenBitCodes)
{
    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "ACEScct 10-bit code=" << code);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(normalized), TransferFunction::ACEScct),
                    acesCctOetfReference(normalized), 1.0e-6);
        const double decodedReference = acesCctEotfReference(normalized);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::ACEScct),
                    decodedReference,
                    std::max(2.0e-6, std::abs(decodedReference) * 1.0e-6));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctTenBitCodesRoundTripThroughSignedLinearToe)
{
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::ACEScct);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScct);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        const int referenceCode = static_cast<int>(std::lround(
            acesCctOetfReference(acesCctEotfReference(encodedInput)) * 1023.0));
        SCOPED_TRACE(::testing::Message()
            << "ACEScct code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, referenceCode);
        EXPECT_EQ(roundTripCode, code);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctSixteenBitCodesRoundTripThroughSignedLinearToe)
{
    constexpr int maximumCode = 65535;
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::ACEScct);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScct);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        EXPECT_EQ(roundTripCode, code) << "16-bit code=" << code;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctToeBranchesMatchAtAndAroundBothBreakpoints)
{
    constexpr float linearBreakpoint = 0.0078125f;
    constexpr float encodedBreakpoint = 0.155251141552511f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f)};
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f)};

    for (const float sample : linearSamples) {
        EXPECT_NEAR(ColorTransferFunction::encode(sample, TransferFunction::ACEScct),
                    acesCctOetfReference(sample), 2.0e-7);
    }
    for (const float sample : encodedSamples) {
        EXPECT_NEAR(ColorTransferFunction::decode(sample, TransferFunction::ACEScct),
                    acesCctEotfReference(sample), 2.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctAdjacentFloatToeValuesExposeDecodeDrop)
{
    constexpr float linearBreakpoint = 0.0078125f;
    constexpr float encodedBreakpoint = 0.155251141552511f;
    const std::array<float, 3> linearSamples = {
        std::nextafter(linearBreakpoint, 0.0f), linearBreakpoint,
        std::nextafter(linearBreakpoint, 1.0f),
    };
    const std::array<float, 3> encodedSamples = {
        std::nextafter(encodedBreakpoint, 0.0f), encodedBreakpoint,
        std::nextafter(encodedBreakpoint, 1.0f),
    };
    std::array<float, 3> encoded{};
    for (std::size_t i = 0; i < linearSamples.size(); ++i) {
        encoded[i] = ColorTransferFunction::encode(linearSamples[i], TransferFunction::ACEScct);
        EXPECT_NEAR(encoded[i], acesCctOetfReference(linearSamples[i]), 2.0e-7);
    }
    EXPECT_LE(encoded[0], encoded[1]);
    EXPECT_LE(encoded[1], encoded[2]);

    std::array<float, 3> decoded{};
    for (std::size_t i = 0; i < encodedSamples.size(); ++i) {
        decoded[i] = ColorTransferFunction::decode(encodedSamples[i], TransferFunction::ACEScct);
        EXPECT_NEAR(decoded[i], acesCctEotfReference(encodedSamples[i]), 2.0e-7);
    }
    EXPECT_LE(decoded[0], decoded[1]);
    const float decodeDrop = decoded[1] - decoded[2];
    EXPECT_GT(decodeDrop, 0.0f);
    EXPECT_LT(decodeDrop, 2.0e-9f);
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctRoundTripsSignedToeAndPositiveHdrValues)
{
    constexpr std::array<double, 14> linearSamples = {
        -1.0, -0.25, -0.01, -1.0e-4,
        0.0, 1.0e-6, 0.001, 0.0078125,
        0.01, 0.18, 1.0, 4.0, 16.0, 100.0,
    };
    for (const double linear : linearSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::ACEScct);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::ACEScct);
        SCOPED_TRACE(::testing::Message() << "ACEScct linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, std::abs(linear) * 3.0e-6));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SimplePowerGammasMatchIndependentReferenceAcrossTenBitCodes)
{
    constexpr std::array<std::pair<TransferFunction, double>, 3> curves = {{
        {TransferFunction::Gamma22, 2.2},
        {TransferFunction::Gamma24, 2.4},
        {TransferFunction::Gamma26, 2.6},
    }};

    for (const auto& [transfer, gamma] : curves) {
        for (int code = 0; code <= 1023; ++code) {
            const double normalized = static_cast<double>(code) / 1023.0;
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer) << " 10-bit code=" << code);
            EXPECT_NEAR(ColorTransferFunction::encode(
                            static_cast<float>(normalized), transfer),
                        std::pow(normalized, 1.0 / gamma), 2.0e-7);
            EXPECT_NEAR(ColorTransferFunction::decode(
                            static_cast<float>(normalized), transfer),
                        std::pow(normalized, gamma), 2.0e-7);
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SimplePowerGammasClampNegativeInputsAndExtendAboveOne)
{
    constexpr std::array<std::pair<TransferFunction, double>, 3> curves = {{
        {TransferFunction::Gamma22, 2.2},
        {TransferFunction::Gamma24, 2.4},
        {TransferFunction::Gamma26, 2.6},
    }};
    constexpr std::array<double, 6> positiveSamples = {0.0, 0.01, 0.18, 1.0, 4.0, 100.0};
    for (const auto& [transfer, gamma] : curves) {
        for (const double linear : positiveSamples) {
            const float encoded = ColorTransferFunction::encode(
                static_cast<float>(linear), transfer);
            const float decoded = ColorTransferFunction::decode(
                static_cast<float>(linear), transfer);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer) << " value=" << linear);
            EXPECT_TRUE(std::isfinite(encoded));
            EXPECT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(encoded, std::pow(linear, 1.0 / gamma),
                        std::max(2.0e-7, std::pow(linear, 1.0 / gamma) * 1.0e-6));
            EXPECT_NEAR(decoded, std::pow(linear, gamma),
                        std::max(2.0e-7, std::pow(linear, gamma) * 1.0e-6));
        }
        for (const float negative : {-100.0f, -1.0f, -0.001f}) {
            EXPECT_FLOAT_EQ(ColorTransferFunction::encode(negative, transfer), 0.0f);
            EXPECT_FLOAT_EQ(ColorTransferFunction::decode(negative, transfer), 0.0f);
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SimplePowerGammasMatchReferenceAtSubnormalAndMinimumNormalInputs)
{
    constexpr std::array<std::pair<TransferFunction, double>, 3> curves = {{
        {TransferFunction::Gamma22, 2.2},
        {TransferFunction::Gamma24, 2.4},
        {TransferFunction::Gamma26, 2.6},
    }};
    const std::array<float, 4> samples = {
        0.0f,
        std::numeric_limits<float>::denorm_min(),
        std::nextafter(0.0f, 1.0f),
        std::numeric_limits<float>::min(),
    };

    for (const auto& [transfer, gamma] : curves) {
        for (const float sample : samples) {
            const float encoded = ColorTransferFunction::encode(sample, transfer);
            const float decoded = ColorTransferFunction::decode(sample, transfer);
            const float encodedReference = static_cast<float>(
                std::pow(static_cast<double>(sample), 1.0 / gamma));
            const float decodedReference = static_cast<float>(
                std::pow(static_cast<double>(sample), gamma));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer) << " input=" << sample);
            EXPECT_NEAR(encoded, encodedReference,
                        std::max(static_cast<double>(std::numeric_limits<float>::denorm_min()),
                                 std::abs(static_cast<double>(encodedReference)) * 3.0e-6));
            EXPECT_FLOAT_EQ(decoded, decodedReference);
        }

        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(
                            -std::numeric_limits<float>::denorm_min(), transfer), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(
                            -std::numeric_limits<float>::denorm_min(), transfer), 0.0f);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SimplePowerGammasEncodeMaxFiniteFloatButDecodeOverflows)
{
    constexpr std::array<std::pair<TransferFunction, double>, 3> curves = {{
        {TransferFunction::Gamma22, 2.2},
        {TransferFunction::Gamma24, 2.4},
        {TransferFunction::Gamma26, 2.6},
    }};
    const float maximum = std::numeric_limits<float>::max();
    for (const auto& [transfer, gamma] : curves) {
        const float encoded = ColorTransferFunction::encode(maximum, transfer);
        const float decoded = ColorTransferFunction::decode(maximum, transfer);
        SCOPED_TRACE(::testing::Message() << "transfer=" << static_cast<int>(transfer));
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_NEAR(encoded,
                    std::pow(static_cast<double>(maximum), 1.0 / gamma),
                    std::pow(static_cast<double>(maximum), 1.0 / gamma) * 2.0e-6);
        EXPECT_TRUE(std::isinf(decoded));
        EXPECT_GT(decoded, 0.0f);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     ClampedDisplayCurvesCharacterizeNanAndSignedInfinityInputs)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;
    constexpr std::array<TransferFunction, 6> curves = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
    };
    for (const TransferFunction curve : curves) {
        SCOPED_TRACE(static_cast<int>(curve));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::encode(nan, curve)));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::decode(nan, curve)));
        EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(positiveInfinity, curve)));
        EXPECT_GT(ColorTransferFunction::encode(positiveInfinity, curve), 0.0f);
        EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(positiveInfinity, curve)));
        EXPECT_GT(ColorTransferFunction::decode(positiveInfinity, curve), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(negativeInfinity, curve), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(negativeInfinity, curve), 0.0f);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     ContinuousDisplayAndSceneCurvesRoundTripAcrossLinearLightGrid)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
    };
    // Keep the samples away from known piecewise discontinuities and the
    // ACEScc / DaVinci zero sentinels; these are positive scene-linear values.
    constexpr float linearSamples[] = {
        1.0e-4f, 0.001f, 0.01f, 0.05f, 0.18f, 0.5f, 0.75f, 1.0f,
    };

    for (const TransferFunction curve : curves) {
        for (const float linear : linearSamples) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " linear=" << linear << " encoded=" << encoded);
            EXPECT_TRUE(std::isfinite(encoded));
            EXPECT_TRUE(std::isfinite(decoded));
            const float relativeTolerance =
                curve == TransferFunction::Rec2084_PQ ? 4.0e-5f : 2.0e-5f;
            EXPECT_NEAR(decoded, linear,
                        std::max(2.0e-7f, linear * relativeTolerance));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     ContinuousCurvesRoundTripAcrossDenseLogarithmicHdrGrid)
{
    constexpr std::array<TransferFunction, 8> curves = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::DaVinciIntermediate,
    };
    constexpr int intervalCount = 256;
    constexpr double minimumExponent = -6.0;
    constexpr double maximumExponent = 3.0;

    for (const TransferFunction curve : curves) {
        for (int index = 0; index <= intervalCount; ++index) {
            const double fraction = static_cast<double>(index) / intervalCount;
            const float linear = static_cast<float>(std::pow(
                10.0, minimumExponent + (maximumExponent - minimumExponent) * fraction));
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve) << " grid index=" << index
                << " linear=" << linear << " encoded=" << encoded);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            const float relativeTolerance = curve == TransferFunction::Rec2084_PQ
                ? 3.0e-4f
                : 3.0e-5f;
            EXPECT_NEAR(decoded, linear,
                        std::max(2.0e-7f, linear * relativeTolerance));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     CrossTransferConversionsMatchIndependentReferenceForEveryCurvePair)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 13> curves = {{
        {TransferFunction::Rec709, rec709OetfReference, rec709EotfReference},
        {TransferFunction::Rec2020_10, rec2020OetfReference, rec2020EotfReference},
        {TransferFunction::Rec2084_PQ, pqOetfReference, pqEotfReference},
        {TransferFunction::HLG, hlgOetfReference, hlgEotfReference},
        {TransferFunction::SonySLog3, slog3OetfReference, slog3EotfReference},
        {TransferFunction::CanonLog2, canonLog2OetfReference, canonLog2EotfReference},
        {TransferFunction::CanonLog3, canonLog3OetfReference, canonLog3EotfReference},
        {TransferFunction::Cineon, cineonOetfReference, cineonEotfReference},
        {TransferFunction::ACEScc, acesCcOetfReference, acesCcEotfReference},
        {TransferFunction::ACEScct, acesCctOetfReference, acesCctEotfReference},
        {TransferFunction::DaVinciIntermediate,
         daVinciIntermediateOetfReference, daVinciIntermediateEotfReference},
        {TransferFunction::sRGB,
         [](double x) { return x <= 0.0031308 ? 12.92 * x
                                               : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055; },
         [](double x) { return x <= 0.04045 ? x / 12.92
                                             : std::pow((x + 0.055) / 1.055, 2.4); }},
        {TransferFunction::Gamma24,
         [](double x) { return std::pow(x, 1.0 / 2.4); },
         [](double x) { return std::pow(x, 2.4); }},
    }};
    constexpr std::array<float, 9> sceneSamples = {
        1.0e-5f, 1.0e-4f, 0.001f, 0.01f, 0.05f,
        0.18f, 0.5f, 1.0f, 4.0f,
    };

    for (const CurveReference& source : curves) {
        for (const CurveReference& destination : curves) {
            if (source.transfer == destination.transfer) continue;
            for (const float sceneLinear : sceneSamples) {
                const float sourceCode = ColorTransferFunction::encode(
                    sceneLinear, source.transfer);
                const float recoveredScene = ColorTransferFunction::decode(
                    sourceCode, source.transfer);
                const float convertedCode = ColorTransferFunction::encode(
                    recoveredScene, destination.transfer);
                const double reference = destination.oetf(source.eotf(
                    source.oetf(static_cast<double>(sceneLinear))));
                SCOPED_TRACE(::testing::Message()
                    << "source=" << static_cast<int>(source.transfer)
                    << " destination=" << static_cast<int>(destination.transfer)
                    << " sceneLinear=" << sceneLinear);
                ASSERT_TRUE(std::isfinite(sourceCode));
                ASSERT_TRUE(std::isfinite(recoveredScene));
                ASSERT_TRUE(std::isfinite(convertedCode));
                EXPECT_NEAR(convertedCode, reference,
                            std::max(3.0e-5, std::abs(reference) * 8.0e-5));
            }
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     LinearTransferIsIdentityForSignedHdrAndNonFiniteInputs)
{
    const std::array<float, 7> finiteSamples = {
        -16.0f, -1.0f, -0.0f, 0.18f, 1.0f, 4.0f, 65504.0f,
    };
    for (const float sample : finiteSamples) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(sample, TransferFunction::Linear), sample);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(sample, TransferFunction::Linear), sample);
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(std::isnan(ColorTransferFunction::encode(nan, TransferFunction::Linear)));
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(infinity, TransferFunction::Linear)));
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCcMatchesIndependentReferenceAcrossTenBitCodesIncludingBlackSentinel)
{
    EXPECT_NEAR(ColorTransferFunction::encode(0.0f, TransferFunction::ACEScc),
                acesCcOetfReference(0.0), 1.0e-8);

    for (int code = 0; code <= 1023; ++code) {
        const double normalized = static_cast<double>(code) / 1023.0;
        SCOPED_TRACE(::testing::Message() << "ACEScc 10-bit code=" << code);
        const double decodedReference = acesCcEotfReference(normalized);
        EXPECT_NEAR(ColorTransferFunction::decode(
                        static_cast<float>(normalized), TransferFunction::ACEScc),
                    decodedReference,
                    std::max(1.0e-4, std::abs(decodedReference) * 1.0e-6));
        if (code > 0) {
            EXPECT_NEAR(ColorTransferFunction::encode(
                            static_cast<float>(normalized), TransferFunction::ACEScc),
                        acesCcOetfReference(normalized), 1.0e-6);
        }
    }

    EXPECT_NEAR(ColorTransferFunction::decode(
                    static_cast<float>(acesCcOetfReference(0.0)), TransferFunction::ACEScc),
                131072.0, 0.1);
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCcTenBitCodesRoundTripThroughLinearLight)
{
    for (int code = 0; code <= 1023; ++code) {
        const float encodedInput = static_cast<float>(code) / 1023.0f;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::ACEScc);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScc);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * 1023.0f));
        SCOPED_TRACE(::testing::Message()
            << "ACEScc code=" << code << " linear=" << linear
            << " re-encoded=" << encodedOutput);
        EXPECT_TRUE(std::isfinite(linear));
        EXPECT_TRUE(std::isfinite(encodedOutput));
        EXPECT_EQ(roundTripCode, code);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCcSixteenBitCodesRoundTripThroughLinearLight)
{
    constexpr int maximumCode = 65535;
    for (int code = 0; code <= maximumCode; ++code) {
        const float encodedInput = static_cast<float>(code) / maximumCode;
        const float linear = ColorTransferFunction::decode(
            encodedInput, TransferFunction::ACEScc);
        const float encodedOutput = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScc);
        const int roundTripCode = static_cast<int>(std::lround(encodedOutput * maximumCode));
        EXPECT_TRUE(std::isfinite(linear)) << "16-bit code=" << code;
        EXPECT_TRUE(std::isfinite(encodedOutput)) << "16-bit code=" << code;
        EXPECT_EQ(roundTripCode, code) << "16-bit code=" << code;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCcClampsNegativeSceneInputsToBlackSentinelAndRoundTripsHdr)
{
    const float blackCode = ColorTransferFunction::encode(0.0f, TransferFunction::ACEScc);
    for (const float negative : {-100.0f, -1.0f, -0.01f, -1.0e-7f}) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(negative, TransferFunction::ACEScc),
                        blackCode);
    }
    EXPECT_NEAR(ColorTransferFunction::decode(blackCode, TransferFunction::ACEScc),
                131072.0f, 0.25f);

    constexpr std::array<double, 9> positiveSamples = {
        1.0e-6, 1.0e-4, 0.001, 0.01, 0.18, 1.0, 4.0, 100.0, 1000.0,
    };
    for (const double linear : positiveSamples) {
        const float encoded = ColorTransferFunction::encode(
            static_cast<float>(linear), TransferFunction::ACEScc);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::ACEScc);
        SCOPED_TRACE(::testing::Message() << "ACEScc linear=" << linear);
        EXPECT_TRUE(std::isfinite(encoded));
        EXPECT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, std::max(2.0e-7, linear * 5.0e-5));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCcBlackSentinelAdjacentCodesMatchReferenceAndRemainOrdered)
{
    constexpr float blackCode = -0.3584474886f;
    const std::array<float, 3> codes = {
        std::nextafter(blackCode, -std::numeric_limits<float>::infinity()),
        blackCode,
        std::nextafter(blackCode, std::numeric_limits<float>::infinity()),
    };
    std::array<float, 3> decoded{};
    for (std::size_t i = 0; i < codes.size(); ++i) {
        decoded[i] = ColorTransferFunction::decode(codes[i], TransferFunction::ACEScc);
        SCOPED_TRACE(::testing::Message() << "ACEScc code=" << codes[i]);
        EXPECT_TRUE(std::isfinite(decoded[i]));
        EXPECT_NEAR(decoded[i], acesCcEotfReference(codes[i]),
                    std::max(0.1, std::abs(acesCcEotfReference(codes[i])) * 2.0e-6));
    }
    EXPECT_FLOAT_EQ(decoded[0], decoded[1]);
    EXPECT_FLOAT_EQ(decoded[1], decoded[2]);
    EXPECT_NEAR(decoded[1], 131072.0f, 0.25f);
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCcExtremeFiniteInputsExposeSentinelAndPowerLimits)
{
    const float maximum = std::numeric_limits<float>::max();
    const float blackCode = ColorTransferFunction::encode(0.0f, TransferFunction::ACEScc);
    const float encodedMaximum = ColorTransferFunction::encode(maximum, TransferFunction::ACEScc);
    EXPECT_TRUE(std::isfinite(encodedMaximum));
    EXPECT_LT(encodedMaximum, 0.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-maximum, TransferFunction::ACEScc), blackCode);

    const float decodedPositiveMaximum = ColorTransferFunction::decode(
        maximum, TransferFunction::ACEScc);
    EXPECT_TRUE(std::isfinite(decodedPositiveMaximum));
    EXPECT_NEAR(decodedPositiveMaximum, -0.00006103515625f, 1.0e-8f);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(-maximum, TransferFunction::ACEScc)));
    EXPECT_GT(ColorTransferFunction::decode(-maximum, TransferFunction::ACEScc), 0.0f);
}

TEST(ColorTransferFunctionStandaloneTest,
     AcesCctExtremeFiniteInputsExposeToeAndExponentialLimits)
{
    const float maximum = std::numeric_limits<float>::max();
    const float encodedPositiveMaximum = ColorTransferFunction::encode(
        maximum, TransferFunction::ACEScct);
    EXPECT_TRUE(std::isfinite(encodedPositiveMaximum));
    EXPECT_NEAR(encodedPositiveMaximum,
                (std::log2(static_cast<double>(maximum)) + 9.72) / 17.52,
                2.0e-6);
    EXPECT_TRUE(std::isinf(ColorTransferFunction::encode(-maximum, TransferFunction::ACEScct)));
    EXPECT_LT(ColorTransferFunction::encode(-maximum, TransferFunction::ACEScct), 0.0f);

    EXPECT_TRUE(std::isinf(ColorTransferFunction::decode(maximum, TransferFunction::ACEScct)));
    EXPECT_GT(ColorTransferFunction::decode(maximum, TransferFunction::ACEScct), 0.0f);
    const float decodedNegativeMaximum = ColorTransferFunction::decode(
        -maximum, TransferFunction::ACEScct);
    EXPECT_TRUE(std::isfinite(decodedNegativeMaximum));
    EXPECT_LT(decodedNegativeMaximum, 0.0f);
    EXPECT_FLOAT_EQ(decodedNegativeMaximum,
                    (-maximum - 0.0729055341958355f) / 10.5402377416545f);
}

TEST(ColorTransferFunctionStandaloneTest,
     DeclaredAcesLogCurrentlyFallsBackToLinearIdentity)
{
    constexpr std::array<float, 5> samples = {-4.0f, -0.25f, 0.0f, 0.18f, 4.0f};
    for (const float sample : samples) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(sample, TransferFunction::ACESlog), sample);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(sample, TransferFunction::ACESlog), sample);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     UnknownTransferFunctionValuesFallBackToIdentityForAllFloatClasses)
{
    constexpr auto unknownTransfer = static_cast<TransferFunction>(0x7fffffff);
    constexpr std::array<TransferFunction, 2> fallbackTransfers = {
        TransferFunction::ACESlog,
        unknownTransfer,
    };
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;
    const std::array<float, 7> samples = {
        -16.0f, -0.0f, 0.0f, 0.18f, 4.0f,
        positiveInfinity, negativeInfinity,
    };

    for (const TransferFunction transfer : fallbackTransfers) {
        for (const float sample : samples) {
            EXPECT_FLOAT_EQ(ColorTransferFunction::encode(sample, transfer), sample);
            EXPECT_FLOAT_EQ(ColorTransferFunction::decode(sample, transfer), sample);
        }
        EXPECT_TRUE(std::isnan(ColorTransferFunction::encode(nan, transfer)));
        EXPECT_TRUE(std::isnan(ColorTransferFunction::decode(nan, transfer)));
        EXPECT_TRUE(std::signbit(ColorTransferFunction::encode(-0.0f, transfer)));
        EXPECT_TRUE(std::signbit(ColorTransferFunction::decode(-0.0f, transfer)));
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     EveryImplementedTransferFunctionHasNonIdentityEncodeAndDecodePaths)
{
    constexpr std::array<TransferFunction, 15> implementedTransfers = {
        TransferFunction::Linear,
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::Cineon,
        TransferFunction::SonySLog3,
        TransferFunction::CanonLog2,
    };
    constexpr std::array<float, 4> samples = {0.01f, 0.18f, 0.5f, 4.0f};

    for (const TransferFunction transfer : implementedTransfers) {
        for (const float sample : samples) {
            const float encoded = ColorTransferFunction::encode(sample, transfer);
            const float decoded = ColorTransferFunction::decode(sample, transfer);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer) << " sample=" << sample);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            if (transfer != TransferFunction::Linear) {
                EXPECT_GT(std::abs(encoded - sample), 1.0e-5f);
                EXPECT_GT(std::abs(decoded - sample), 1.0e-5f);
            }
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     StandardDisplayAndVideoCurvesClampNegativeEncodeAndDecodeInputs)
{
    constexpr std::array<TransferFunction, 8> clampedCurves = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
    };

    for (const TransferFunction curve : clampedCurves) {
        SCOPED_TRACE(static_cast<int>(curve));
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(-0.25f, curve), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(-0.25f, curve), 0.0f);
    }

    constexpr double sLog3BlackCode = 95.0 / 1023.0;
    EXPECT_NEAR(ColorTransferFunction::encode(-0.25f, TransferFunction::SonySLog3),
                sLog3BlackCode, 1.0e-7);
    EXPECT_NEAR(ColorTransferFunction::encode(-0.25f, TransferFunction::Cineon),
                sLog3BlackCode, 1.0e-7);
}

TEST(ColorTransferFunctionStandaloneTest,
     ClampedCurvesMapNegativeZeroAndNegativeSubnormalToZero)
{
    constexpr std::array<TransferFunction, 8> clampedCurves = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
    };
    const float negativeSubnormal = -std::numeric_limits<float>::denorm_min();
    const std::array<float, 3> negativeInputs = {-1.0e-7f, negativeSubnormal, -0.0f};

    for (const TransferFunction transfer : clampedCurves) {
        for (const float input : negativeInputs) {
            EXPECT_FLOAT_EQ(ColorTransferFunction::encode(input, transfer), 0.0f);
            EXPECT_FLOAT_EQ(ColorTransferFunction::decode(input, transfer), 0.0f);
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SignedLogCurvesMatchIndependentReferencesForNegativeSceneValues)
{
    constexpr std::array<double, 5> samples = {-1.0, -0.25, -0.05, -0.01, -0.001};
    for (const double linear : samples) {
        SCOPED_TRACE(linear);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(linear), TransferFunction::ACEScct),
                    acesCctOetfReference(linear), 4.0e-7);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(linear), TransferFunction::CanonLog2),
                    canonLog2OetfReference(linear), 2.0e-7);
        EXPECT_NEAR(ColorTransferFunction::encode(
                        static_cast<float>(linear), TransferFunction::CanonLog3),
                    canonLog3OetfReference(linear), 2.0e-7);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SignedLogCurvesMatchReferencesAtSignedZeroAndAdjacentSubnormals)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 5> curves = {{
        {TransferFunction::SonySLog3, slog3OetfReference, slog3EotfReference},
        {TransferFunction::CanonLog2, canonLog2OetfReference, canonLog2EotfReference},
        {TransferFunction::CanonLog3, canonLog3OetfReference, canonLog3EotfReference},
        {TransferFunction::ACEScc, acesCcOetfReference, acesCcEotfReference},
        {TransferFunction::ACEScct, acesCctOetfReference, acesCctEotfReference},
    }};
    const float subnormal = std::numeric_limits<float>::denorm_min();
    const std::array<float, 4> samples = {-subnormal, -0.0f, 0.0f, subnormal};

    for (const CurveReference& curve : curves) {
        for (const float sample : samples) {
            const double encodedReference = curve.oetf(sample);
            const double decodedReference = curve.eotf(sample);
            const float encoded = ColorTransferFunction::encode(sample, curve.transfer);
            const float decoded = ColorTransferFunction::decode(sample, curve.transfer);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer) << " sample=" << sample);
            EXPECT_NEAR(encoded, encodedReference,
                        std::max(2.0e-7, std::abs(encodedReference) * 3.0e-6));
            EXPECT_NEAR(decoded, decodedReference,
                        std::max(2.0e-7, std::abs(decodedReference) * 3.0e-6));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     LogCurvesMatchIndependentReferencesOutsideNormalizedCodeRange)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 3> curves = {{
        {TransferFunction::ACEScc, acesCcOetfReference, acesCcEotfReference},
        {TransferFunction::ACEScct, acesCctOetfReference, acesCctEotfReference},
        {TransferFunction::CanonLog2, canonLog2OetfReference, canonLog2EotfReference},
    }};
    constexpr std::array<double, 7> outOfRangeCodes = {
        -1.0, -0.25, -0.01, 0.0, 1.0, 1.01, 1.25,
    };
    constexpr std::array<double, 7> outOfRangeLinear = {
        -100.0, -1.0, -0.1, 0.0, 1.0, 10.0, 1000.0,
    };

    for (const CurveReference& curve : curves) {
        for (const double code : outOfRangeCodes) {
            const float decoded = ColorTransferFunction::decode(
                static_cast<float>(code), curve.transfer);
            const double expected = curve.eotf(static_cast<double>(static_cast<float>(code)));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer) << " code=" << code);
            EXPECT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, expected,
                        std::max(2.0e-6, std::abs(expected) * 3.0e-6));
        }

        for (const double linear : outOfRangeLinear) {
            const float encoded = ColorTransferFunction::encode(
                static_cast<float>(linear), curve.transfer);
            const double expected = curve.oetf(
                static_cast<double>(static_cast<float>(linear)));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer) << " linear=" << linear);
            EXPECT_TRUE(std::isfinite(encoded));
            EXPECT_NEAR(encoded, expected,
                        std::max(2.0e-6, std::abs(expected) * 3.0e-6));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     LogCurvesMatchReferencesAcrossWideNegativeAndHdrExtrapolation)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::SonySLog3, slog3OetfReference, slog3EotfReference},
        {TransferFunction::CanonLog2, canonLog2OetfReference, canonLog2EotfReference},
        {TransferFunction::CanonLog3, canonLog3OetfReference, canonLog3EotfReference},
        {TransferFunction::Cineon, cineonOetfReference, cineonEotfReference},
        {TransferFunction::ACEScc, acesCcOetfReference, acesCcEotfReference},
        {TransferFunction::ACEScct, acesCctOetfReference, acesCctEotfReference},
    }};
    constexpr std::array<double, 6> encodedSamples = {
        -2.0, -1.25, -1.0, 1.0, 1.25, 2.0,
    };
    constexpr std::array<double, 7> linearSamples = {
        -1.0e6, -1.0e4, -100.0, 100.0, 1.0e4, 1.0e6, 1.0e7,
    };

    for (const CurveReference& curve : curves) {
        for (const double code : encodedSamples) {
            const float input = static_cast<float>(code);
            const float decoded = ColorTransferFunction::decode(input, curve.transfer);
            const double expected = curve.eotf(static_cast<double>(input));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer) << " code=" << code);
            EXPECT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, expected,
                        std::max(2.0e-5, std::abs(expected) * 5.0e-6));
        }

        for (const double linear : linearSamples) {
            const float input = static_cast<float>(linear);
            const float encoded = ColorTransferFunction::encode(input, curve.transfer);
            const double referenceInput = curve.transfer == TransferFunction::SonySLog3
                    && input < 0.0f
                ? 0.0
                : static_cast<double>(input);
            const double expected = curve.oetf(referenceInput);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer) << " linear=" << linear);
            EXPECT_TRUE(std::isfinite(encoded));
            EXPECT_NEAR(encoded, expected,
                        std::max(2.0e-5, std::abs(expected) * 5.0e-6));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     SignedLogOetfsMatchReferenceAndOrderingAcrossDensePositiveSceneGrid)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::SonySLog3, slog3OetfReference},
        {TransferFunction::CanonLog2, canonLog2OetfReference},
        {TransferFunction::CanonLog3, canonLog3OetfReference},
        {TransferFunction::Cineon, cineonOetfReference},
        {TransferFunction::ACEScc, acesCcOetfReference},
        {TransferFunction::DaVinciIntermediate, daVinciIntermediateOetfReference},
    }};
    constexpr int intervalCount = 1000;
    constexpr double minimumExponent = -5.0;
    constexpr double maximumExponent = 4.0;
    const auto ordering = [](const double lhs, const double rhs) {
        return (lhs > rhs) - (lhs < rhs);
    };

    for (const CurveReference& curve : curves) {
        float previousActual = 0.0f;
        double previousReference = 0.0;
        for (int index = 0; index <= intervalCount; ++index) {
            const double fraction = static_cast<double>(index) / intervalCount;
            const double linear = std::pow(10.0,
                minimumExponent + (maximumExponent - minimumExponent) * fraction);
            const float input = static_cast<float>(linear);
            const float actual = ColorTransferFunction::encode(input, curve.transfer);
            const double reference = curve.oetf(static_cast<double>(input));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer)
                << " grid index=" << index << " linear=" << input);
            ASSERT_TRUE(std::isfinite(actual));
            ASSERT_TRUE(std::isfinite(reference));
            EXPECT_NEAR(actual, reference,
                        std::max(2.0e-6, std::abs(reference) * 5.0e-6));

            if (index > 0) {
                const int referenceOrder = ordering(reference, previousReference);
                const int actualOrder = ordering(actual, previousActual);
                if (referenceOrder > 0)
                    EXPECT_GE(actualOrder, 0);
                else if (referenceOrder < 0)
                    EXPECT_LE(actualOrder, 0);
                else
                    EXPECT_EQ(actualOrder, 0);
            }
            previousActual = actual;
            previousReference = reference;
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     LogCurveExtendedCodeSweepsFollowReferenceOrdering)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*eotf)(double);
        double relativeTolerance;
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::SonySLog3, slog3EotfReference},
        {TransferFunction::CanonLog2, canonLog2EotfReference},
        {TransferFunction::CanonLog3, canonLog3EotfReference},
        {TransferFunction::Cineon, cineonEotfReference},
        {TransferFunction::ACEScc, acesCcEotfReference},
        {TransferFunction::ACEScct, acesCctEotfReference},
    }};
    constexpr int intervalCount = 400;
    constexpr double minimumCode = -2.0;
    constexpr double maximumCode = 2.0;
    const auto ordering = [](const double lhs, const double rhs) {
        return (lhs > rhs) - (lhs < rhs);
    };

    for (const CurveReference& curve : curves) {
        float previousActual = 0.0f;
        double previousReference = 0.0;
        for (int index = 0; index <= intervalCount; ++index) {
            const double fraction = static_cast<double>(index) / intervalCount;
            const float code = static_cast<float>(
                minimumCode + (maximumCode - minimumCode) * fraction);
            const float actual = ColorTransferFunction::decode(code, curve.transfer);
            const double reference = curve.eotf(static_cast<double>(code));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer)
                << " code-grid index=" << index << " code=" << code);
            ASSERT_TRUE(std::isfinite(actual));
            ASSERT_TRUE(std::isfinite(reference));
            EXPECT_NEAR(actual, reference,
                        std::max(2.0e-5, std::abs(reference) * 5.0e-6));
            if (index > 0) {
                const int referenceOrder = ordering(reference, previousReference);
                if (referenceOrder > 0)
                    EXPECT_GE(actual, previousActual);
                else if (referenceOrder < 0)
                    EXPECT_LE(actual, previousActual);
                else
                    EXPECT_FLOAT_EQ(actual, previousActual);
            }
            previousActual = actual;
            previousReference = reference;
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     AllNonLinearCurvesFollowReferenceDecodeOrderingAcrossTenBitCodes)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 16> curves = {{
        {TransferFunction::sRGB,
         [](double x) { return x <= 0.04045 ? x / 12.92
                                             : std::pow((x + 0.055) / 1.055, 2.4); }},
        {TransferFunction::Gamma22,
         [](double x) { return std::pow(x, 2.2); }},
        {TransferFunction::Gamma24,
         [](double x) { return std::pow(x, 2.4); }},
        {TransferFunction::Gamma26,
         [](double x) { return std::pow(x, 2.6); }},
        {TransferFunction::Rec709, rec709EotfReference},
        {TransferFunction::Rec2020_10, rec2020EotfReference},
        {TransferFunction::Rec2084_PQ, pqEotfReference},
        {TransferFunction::HLG, hlgEotfReference},
        {TransferFunction::SonySLog3, slog3EotfReference},
        {TransferFunction::CanonLog2, canonLog2EotfReference},
        {TransferFunction::CanonLog3, canonLog3EotfReference},
        {TransferFunction::Cineon, cineonEotfReference},
        {TransferFunction::ACEScc, acesCcEotfReference},
        {TransferFunction::ACEScct, acesCctEotfReference},
        {TransferFunction::DaVinciIntermediate, daVinciIntermediateEotfReference},
        {TransferFunction::Linear, [](double x) { return x; }},
    }};
    const auto ordering = [](const double lhs, const double rhs) {
        return (lhs > rhs) - (lhs < rhs);
    };

    for (const CurveReference& curve : curves) {
        bool foundReferenceRise = false;
        bool foundReferenceDrop = false;
        bool foundFloatPlateauOnRise = false;
        bool foundFloatPlateauOnDrop = false;
        float previousActual = 0.0f;
        double previousReference = 0.0;
        for (int code = 0; code <= 1023; ++code) {
            const float encoded = static_cast<float>(code) / 1023.0f;
            const float actual = ColorTransferFunction::decode(encoded, curve.transfer);
            const double reference = curve.eotf(static_cast<double>(encoded));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer)
                << " code=" << code << " encoded=" << encoded);
            ASSERT_TRUE(std::isfinite(actual));
            ASSERT_TRUE(std::isfinite(reference));

            if (code > 0) {
                const int referenceOrder = ordering(reference, previousReference);
                const int actualOrder = ordering(actual, previousActual);
                if (referenceOrder > 0) {
                    foundReferenceRise = true;
                    EXPECT_GE(actualOrder, 0);
                    foundFloatPlateauOnRise |= actualOrder == 0;
                } else if (referenceOrder < 0) {
                    foundReferenceDrop = true;
                    EXPECT_LE(actualOrder, 0)
                        << "Implementation must preserve reference's local drop.";
                    foundFloatPlateauOnDrop |= actualOrder == 0;
                } else {
                    EXPECT_EQ(actualOrder, 0);
                }
            }
            previousActual = actual;
            previousReference = reference;
        }
        EXPECT_TRUE(foundReferenceRise || foundReferenceDrop)
            << "transfer=" << static_cast<int>(curve.transfer);
        SCOPED_TRACE(::testing::Message()
            << "referenceDrops=" << foundReferenceDrop
            << " floatPlateauOnRise=" << foundFloatPlateauOnRise
            << " floatPlateauOnDrop=" << foundFloatPlateauOnDrop);
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     DisplayTransferCurvesMatchReferencesAcrossFullSixteenBitCodeGrid)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::sRGB,
         [](double x) { return x <= 0.0031308 ? 12.92 * x
                                               : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055; },
         [](double x) { return x <= 0.04045 ? x / 12.92
                                             : std::pow((x + 0.055) / 1.055, 2.4); }},
        {TransferFunction::Gamma22,
         [](double x) { return std::pow(x, 1.0 / 2.2); },
         [](double x) { return std::pow(x, 2.2); }},
        {TransferFunction::Gamma24,
         [](double x) { return std::pow(x, 1.0 / 2.4); },
         [](double x) { return std::pow(x, 2.4); }},
        {TransferFunction::Gamma26,
         [](double x) { return std::pow(x, 1.0 / 2.6); },
         [](double x) { return std::pow(x, 2.6); }},
        {TransferFunction::Rec709, rec709OetfReference, rec709EotfReference},
        {TransferFunction::Rec2020_10, rec2020OetfReference, rec2020EotfReference},
    }};
    constexpr int maximumCode = 65535;

    for (const CurveReference& curve : curves) {
        float previousEncoded = 0.0f;
        float previousDecoded = 0.0f;
        double previousDecodedReference = 0.0;
        for (int code = 0; code <= maximumCode; ++code) {
            const double normalized = static_cast<double>(code) / maximumCode;
            const float input = static_cast<float>(normalized);
            const float encoded = ColorTransferFunction::encode(input, curve.transfer);
            const float decoded = ColorTransferFunction::decode(input, curve.transfer);
            const double decodedReference = curve.eotf(static_cast<double>(input));
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer)
                << " 16-bit code=" << code);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            const double tolerance = curve.transfer == TransferFunction::Rec2020_10
                ? 5.0e-7
                : 3.0e-7;
            EXPECT_NEAR(encoded, curve.oetf(static_cast<double>(input)), tolerance);
            EXPECT_NEAR(decoded, decodedReference, tolerance);
            if (code > 0) {
                EXPECT_GE(encoded, previousEncoded);
                if (decodedReference > previousDecodedReference)
                    EXPECT_GE(decoded, previousDecoded);
                else if (decodedReference < previousDecodedReference)
                    EXPECT_LE(decoded, previousDecoded);
                else
                    EXPECT_FLOAT_EQ(decoded, previousDecoded);
            }
            previousEncoded = encoded;
            previousDecoded = decoded;
            previousDecodedReference = decodedReference;
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     QuantizedDisplayCodeCentersDecodeWithinSixteenBitStepAcrossSceneGrid)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::sRGB,
         [](double x) { return x <= 0.0031308 ? 12.92 * x
                                               : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055; },
         [](double x) { return x <= 0.04045 ? x / 12.92
                                             : std::pow((x + 0.055) / 1.055, 2.4); }},
        {TransferFunction::Gamma22,
         [](double x) { return std::pow(x, 1.0 / 2.2); },
         [](double x) { return std::pow(x, 2.2); }},
        {TransferFunction::Gamma24,
         [](double x) { return std::pow(x, 1.0 / 2.4); },
         [](double x) { return std::pow(x, 2.4); }},
        {TransferFunction::Gamma26,
         [](double x) { return std::pow(x, 1.0 / 2.6); },
         [](double x) { return std::pow(x, 2.6); }},
        {TransferFunction::Rec709, rec709OetfReference, rec709EotfReference},
        {TransferFunction::Rec2020_10, rec2020OetfReference, rec2020EotfReference},
    }};
    constexpr int maximumCode = 65535;
    constexpr int sceneSampleCount = 4097;

    for (const CurveReference& curve : curves) {
        for (int sampleIndex = 0; sampleIndex < sceneSampleCount; ++sampleIndex) {
            const double linear = static_cast<double>(sampleIndex) /
                                  (sceneSampleCount - 1);
            const float encoded = ColorTransferFunction::encode(
                static_cast<float>(linear), curve.transfer);
            const int quantizedCode = static_cast<int>(std::lround(encoded * maximumCode));
            const float quantizedEncoded = static_cast<float>(quantizedCode) / maximumCode;
            const float decoded = ColorTransferFunction::decode(quantizedEncoded, curve.transfer);
            const double referenceEncoded = curve.oetf(linear);
            const int referenceCode = static_cast<int>(std::lround(
                referenceEncoded * maximumCode));
            const double referenceDecoded = curve.eotf(
                static_cast<double>(referenceCode) / maximumCode);
            const double referenceTolerance =
                curve.transfer == TransferFunction::sRGB ||
                curve.transfer == TransferFunction::Rec709 ||
                curve.transfer == TransferFunction::Rec2020_10
                ? 5.0e-5
                : curve.transfer == TransferFunction::Gamma22 ||
                      curve.transfer == TransferFunction::Gamma24 ||
                      curve.transfer == TransferFunction::Gamma26
                    ? 4.0e-5
                    : std::max(2.0e-7, std::abs(referenceDecoded) * 2.0e-6);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer)
                << " linear sample=" << sampleIndex << " quantized code=" << quantizedCode);
            ASSERT_TRUE(std::isfinite(decoded));
            ASSERT_TRUE(std::isfinite(referenceDecoded));
            EXPECT_NEAR(decoded, referenceDecoded, referenceTolerance);
            EXPECT_NEAR(decoded, linear,
                        std::max(2.0e-5, std::abs(linear) * 1.0e-4));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     DisplayTransferCodesRoundTripAgainstReferencesAcrossFullSixteenBitGrid)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::sRGB,
         [](double x) { return x <= 0.0031308 ? 12.92 * x
                                               : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055; },
         [](double x) { return x <= 0.04045 ? x / 12.92
                                             : std::pow((x + 0.055) / 1.055, 2.4); }},
        {TransferFunction::Gamma22,
         [](double x) { return std::pow(x, 1.0 / 2.2); },
         [](double x) { return std::pow(x, 2.2); }},
        {TransferFunction::Gamma24,
         [](double x) { return std::pow(x, 1.0 / 2.4); },
         [](double x) { return std::pow(x, 2.4); }},
        {TransferFunction::Gamma26,
         [](double x) { return std::pow(x, 1.0 / 2.6); },
         [](double x) { return std::pow(x, 2.6); }},
        {TransferFunction::Rec709, rec709OetfReference, rec709EotfReference},
        {TransferFunction::Rec2020_10, rec2020OetfReference, rec2020EotfReference},
    }};
    constexpr int maximumCode = 65535;

    for (const CurveReference& curve : curves) {
        int maximumCodeError = 0;
        int maximumErrorInputCode = 0;
        int maximumReferenceError = 0;
        for (int code = 0; code <= maximumCode; ++code) {
            const float encoded = static_cast<float>(code) / maximumCode;
            const float linear = ColorTransferFunction::decode(encoded, curve.transfer);
            const float reencoded = ColorTransferFunction::encode(linear, curve.transfer);
            if (!std::isfinite(linear) || !std::isfinite(reencoded)) {
                maximumCodeError = maximumCode;
                maximumErrorInputCode = code;
                break;
            }
            const int roundTripCode = static_cast<int>(std::lround(
                static_cast<double>(reencoded) * maximumCode));
            const int referenceRoundTripCode = static_cast<int>(std::lround(
                curve.oetf(curve.eotf(static_cast<double>(encoded))) * maximumCode));
            const int codeError = std::abs(roundTripCode - code);
            if (codeError > maximumCodeError) {
                maximumCodeError = codeError;
                maximumErrorInputCode = code;
            }
            maximumReferenceError = std::max(maximumReferenceError,
                std::abs(roundTripCode - referenceRoundTripCode));
        }
        EXPECT_LE(maximumReferenceError, 1)
            << "transfer=" << static_cast<int>(curve.transfer);
        // The Rec.709 branch thresholds are not exact inverses of one another;
        // preserve the measured code bias while still matching the reference round trip.
        const int sourceCodeErrorLimit = curve.transfer == TransferFunction::Rec709
            ? 16
            : 1;
        EXPECT_LE(maximumCodeError, sourceCodeErrorLimit)
            << "transfer=" << static_cast<int>(curve.transfer)
            << " input code=" << maximumErrorInputCode;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     QuantizedHdrCodeCentersDecodeWithinSixteenBitStepAcrossSceneGrid)
{
    constexpr int maximumCode = 65535;
    constexpr int sceneSampleCount = 4097;
    for (const TransferFunction transfer : {
             TransferFunction::Rec2084_PQ, TransferFunction::HLG}) {
        for (int sampleIndex = 0; sampleIndex < sceneSampleCount; ++sampleIndex) {
            const double linear = static_cast<double>(sampleIndex) /
                                  (sceneSampleCount - 1);
            const float encoded = ColorTransferFunction::encode(
                static_cast<float>(linear), transfer);
            const int quantizedCode = static_cast<int>(std::lround(encoded * maximumCode));
            const float quantizedEncoded = static_cast<float>(quantizedCode) / maximumCode;
            const float decoded = ColorTransferFunction::decode(quantizedEncoded, transfer);
            const double referenceEncoded = transfer == TransferFunction::Rec2084_PQ
                ? pqOetfReference(linear)
                : hlgOetfReference(linear);
            const int referenceCode = static_cast<int>(std::lround(
                referenceEncoded * maximumCode));
            const double quantizedCodeCenter =
                static_cast<double>(quantizedCode) / maximumCode;
            const double referenceDecoded = transfer == TransferFunction::Rec2084_PQ
                ? pqEotfReference(quantizedCodeCenter)
                : hlgEotfReference(quantizedCodeCenter);
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer)
                << " linear sample=" << sampleIndex << " quantized code=" << quantizedCode
                << " reference code=" << referenceCode);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            ASSERT_TRUE(std::isfinite(referenceDecoded));
            EXPECT_LE(std::abs(quantizedCode - referenceCode), 1)
                << "transfer=" << static_cast<int>(transfer)
                << " linear sample=" << sampleIndex;
            EXPECT_NEAR(decoded, referenceDecoded,
                        transfer == TransferFunction::HLG ? 7.0e-5 : 6.0e-5);
            EXPECT_NEAR(decoded, linear,
                        std::max(3.0e-5, std::abs(linear) * 2.0e-4));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     HdrTransferCodesRoundTripAgainstReferencesAcrossFullSixteenBitGrid)
{
    constexpr int maximumCode = 65535;
    for (const TransferFunction transfer : {
             TransferFunction::Rec2084_PQ, TransferFunction::HLG}) {
        int maximumCodeError = 0;
        int maximumErrorInputCode = 0;
        int maximumReferenceError = 0;
        for (int code = 0; code <= maximumCode; ++code) {
            const double normalized = static_cast<double>(code) / maximumCode;
            const float encoded = static_cast<float>(normalized);
            const float linear = ColorTransferFunction::decode(encoded, transfer);
            const float reencoded = ColorTransferFunction::encode(linear, transfer);
            if (!std::isfinite(linear) || !std::isfinite(reencoded)) {
                maximumCodeError = maximumCode;
                maximumErrorInputCode = code;
                break;
            }

            const double decodedReference = transfer == TransferFunction::Rec2084_PQ
                ? pqEotfReference(normalized)
                : hlgEotfReference(normalized);
            const double reencodedReference = transfer == TransferFunction::Rec2084_PQ
                ? pqOetfReference(decodedReference)
                : hlgOetfReference(decodedReference);
            const int referenceRoundTripCode = static_cast<int>(std::lround(
                reencodedReference * maximumCode));
            const int roundTripCode = static_cast<int>(std::lround(
                static_cast<double>(reencoded) * maximumCode));
            const int codeError = std::abs(roundTripCode - code);
            if (codeError > maximumCodeError) {
                maximumCodeError = codeError;
                maximumErrorInputCode = code;
            }
            maximumReferenceError = std::max(maximumReferenceError,
                std::abs(roundTripCode - referenceRoundTripCode));
        }

        EXPECT_LE(maximumReferenceError, 1)
            << "transfer=" << static_cast<int>(transfer);
        const int sourceCodeErrorLimit = transfer == TransferFunction::Rec2084_PQ
            ? 2
            : 1;
        EXPECT_LE(maximumCodeError, sourceCodeErrorLimit)
            << "transfer=" << static_cast<int>(transfer)
            << " input code=" << maximumErrorInputCode;
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     QuantizedLogCodeCentersDecodeWithinSixteenBitStepAcrossSceneGrid)
{
    struct CurveReference {
        TransferFunction transfer;
        double (*oetf)(double);
        double (*eotf)(double);
        double codeCenterTolerance;
    };
    constexpr std::array<CurveReference, 6> curves = {{
        {TransferFunction::SonySLog3, slog3OetfReference, slog3EotfReference, 3.0e-5},
        {TransferFunction::CanonLog2, canonLog2OetfReference, canonLog2EotfReference, 3.0e-5},
        {TransferFunction::CanonLog3, canonLog3OetfReference, canonLog3EotfReference, 3.0e-5},
        {TransferFunction::Cineon, cineonOetfReference, cineonEotfReference, 3.0e-5},
        {TransferFunction::ACEScc, acesCcOetfReference, acesCcEotfReference, 3.0e-5},
        {TransferFunction::DaVinciIntermediate,
         daVinciIntermediateOetfReference, daVinciIntermediateEotfReference, 3.0e-5},
    }};
    constexpr int maximumCode = 65535;
    constexpr int sceneSampleCount = 4097;
    constexpr double minimumExponent = -5.0;
    constexpr double maximumExponent = 4.0;

    for (const CurveReference& curve : curves) {
        for (int sampleIndex = 0; sampleIndex < sceneSampleCount; ++sampleIndex) {
            const double fraction = static_cast<double>(sampleIndex) /
                                    (sceneSampleCount - 1);
            const double linear = std::pow(10.0,
                minimumExponent + (maximumExponent - minimumExponent) * fraction);
            const float encoded = ColorTransferFunction::encode(
                static_cast<float>(linear), curve.transfer);
            const int quantizedCode = static_cast<int>(std::lround(encoded * maximumCode));
            const float quantizedEncoded = static_cast<float>(quantizedCode) / maximumCode;
            const float decoded = ColorTransferFunction::decode(
                quantizedEncoded, curve.transfer);
            const int referenceCode = static_cast<int>(std::lround(
                curve.oetf(linear) * maximumCode));
            const double referenceCodeCenter =
                static_cast<double>(quantizedCode) / maximumCode;
            const double referenceDecoded = curve.eotf(referenceCodeCenter);
            const double codeCenterTolerance = curve.transfer ==
                    TransferFunction::DaVinciIntermediate ||
                    curve.transfer == TransferFunction::ACEScc ||
                    curve.transfer == TransferFunction::CanonLog3 ||
                    curve.transfer == TransferFunction::Cineon ||
                    curve.transfer == TransferFunction::CanonLog2 ||
                    curve.transfer == TransferFunction::SonySLog3
                ? std::max(curve.codeCenterTolerance,
                           std::abs(referenceDecoded) * 2.0e-6)
                : curve.codeCenterTolerance;
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(curve.transfer)
                << " scene grid index=" << sampleIndex
                << " linear=" << linear << " quantized code=" << quantizedCode);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            ASSERT_TRUE(std::isfinite(referenceDecoded));
            EXPECT_LE(std::abs(quantizedCode - referenceCode), 1);
            EXPECT_NEAR(decoded, referenceDecoded, codeCenterTolerance);
            EXPECT_NEAR(decoded, linear,
                        std::max(5.0e-5, std::abs(linear) * 3.0e-4));
        }
    }
}

TEST(ColorTransferFunctionStandaloneTest,
     PqAndHlgEotfsMatchReferencesAcrossFullSixteenBitCodeGrid)
{
    constexpr int maximumCode = 65535;
    for (const TransferFunction transfer : {
             TransferFunction::Rec2084_PQ, TransferFunction::HLG}) {
        float previousDecoded = 0.0f;
        double previousDecodedReference = 0.0;
        double maximumReferenceError = 0.0;
        int maximumErrorCode = 0;
        int directionMismatchCount = 0;
        for (int code = 0; code <= maximumCode; ++code) {
            const double normalized = static_cast<double>(code) / maximumCode;
            const float input = static_cast<float>(normalized);
            const float decoded = ColorTransferFunction::decode(input, transfer);
            const double decodedReference = transfer == TransferFunction::Rec2084_PQ
                ? pqEotfReference(static_cast<double>(input))
                : hlgEotfReference(static_cast<double>(input));
            ASSERT_TRUE(std::isfinite(decoded)) << "transfer=" << static_cast<int>(transfer)
                                                << " code=" << code;
            ASSERT_TRUE(std::isfinite(decodedReference)) << "code=" << code;
            const double error = std::abs(static_cast<double>(decoded) - decodedReference);
            if (error > maximumReferenceError) {
                maximumReferenceError = error;
                maximumErrorCode = code;
            }
            if (code > 0) {
                if (decodedReference > previousDecodedReference)
                    directionMismatchCount += decoded < previousDecoded;
                else if (decodedReference < previousDecodedReference)
                    directionMismatchCount += decoded > previousDecoded;
                else
                    directionMismatchCount += decoded != previousDecoded;
            }
            previousDecoded = decoded;
            previousDecodedReference = decodedReference;
        }
        const double maximumErrorLimit = 6.0e-5;
        EXPECT_LE(maximumReferenceError, maximumErrorLimit)
            << "transfer=" << static_cast<int>(transfer) << " code=" << maximumErrorCode;
        EXPECT_EQ(directionMismatchCount, 0)
            << "transfer=" << static_cast<int>(transfer);
    }

}

TEST(ColorTransferFunctionStandaloneTest,
     PqOetfMatchesReferencesAcrossFullSixteenBitLinearGrid)
{
    constexpr int maximumCode = 65535;
    float previousEncoded = ColorTransferFunction::encode(0.0f, TransferFunction::Rec2084_PQ);
    double maximumReferenceError = 0.0;
    int maximumErrorCode = 0;
    int decreaseCount = 0;
    double maximumDecrease = 0.0;
    for (int code = 0; code <= maximumCode; ++code) {
        const float linear = static_cast<float>(code) / maximumCode;
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::Rec2084_PQ);
        ASSERT_TRUE(std::isfinite(encoded)) << "code=" << code;
        const double reference = pqOetfReference(static_cast<double>(linear));
        const double error = std::abs(static_cast<double>(encoded) - reference);
        if (error > maximumReferenceError) {
            maximumReferenceError = error;
            maximumErrorCode = code;
        }
        if (code > 0 && encoded < previousEncoded) {
            ++decreaseCount;
            maximumDecrease = std::max(maximumDecrease,
                static_cast<double>(previousEncoded - encoded));
        }
        previousEncoded = encoded;
    }
    EXPECT_LE(maximumReferenceError, 1.5e-5) << "code=" << maximumErrorCode;
    EXPECT_LE(decreaseCount, 3000);
    EXPECT_LE(maximumDecrease, 2.0e-5);
}

TEST(ColorTransferFunctionStandaloneTest,
     HlgOetfMatchesReferenceAcrossFullSixteenBitLinearGrid)
{
    constexpr int maximumCode = 65535;
    float previousEncoded = ColorTransferFunction::encode(0.0f, TransferFunction::HLG);
    for (int code = 0; code <= maximumCode; ++code) {
        const float linear = static_cast<float>(code) / maximumCode;
        const float encoded = ColorTransferFunction::encode(linear, TransferFunction::HLG);
        SCOPED_TRACE(::testing::Message() << "HLG linear-grid code=" << code);
        ASSERT_TRUE(std::isfinite(encoded));
        EXPECT_NEAR(encoded, hlgOetfReference(static_cast<double>(linear)), 5.0e-5);
        if (code > 0)
            EXPECT_GE(encoded, previousEncoded);
        previousEncoded = encoded;
    }
}
