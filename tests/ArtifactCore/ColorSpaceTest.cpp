#include <gtest/gtest.h>

#include <array>
#include <cmath>
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

TEST(ColorSpaceTest, PqAndHlgClampOutOfRangeValues)
{
    for (const GammaFunction transfer : {GammaFunction::PQ, GammaFunction::HLG}) {
        EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(-0.5f, transfer), 0.0f);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(1.5f, transfer), 1.0f);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(-0.5f, transfer), 0.0f);
        EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(1.5f, transfer), 1.0f);
    }
}

TEST(ColorSpaceTest, NonFiniteTransferInputsAreSanitized)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    EXPECT_FLOAT_EQ(ColorSpaceConverter::applyGamma(nan, GammaFunction::sRGB), 0.0f);
    EXPECT_FLOAT_EQ(ColorSpaceConverter::removeGamma(infinity, GammaFunction::Gamma22), 0.0f);
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
