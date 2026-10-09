#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>

import Color.GamutConversion;

using namespace ArtifactCore;

namespace {

constexpr std::array<Gamut, 11> kGamuts = {
    Gamut::sRGB,          Gamut::Rec709,       Gamut::Rec2020,
    Gamut::DCI_P3,        Gamut::DisplayP3,   Gamut::ACES_AP0,
    Gamut::ACES_AP1,      Gamut::AdobeRGB,    Gamut::XYZ_D65,
    Gamut::XYZ_D60,       Gamut::DaVinciWideGamut};
constexpr std::array<Gamut, 6> kD65RoundTripGamuts = {
    Gamut::sRGB, Gamut::Rec709, Gamut::Rec2020,
    Gamut::DCI_P3, Gamut::DisplayP3, Gamut::AdobeRGB};
// XYZ_D60 has separate white-point routing behavior, and the current
// DaVinci Wide Gamut forward/inverse references are not mutual inverses.
constexpr std::array<Gamut, 9> kComposableGamutReferences = {
    Gamut::sRGB, Gamut::Rec709, Gamut::Rec2020,
    Gamut::DCI_P3, Gamut::DisplayP3, Gamut::ACES_AP0,
    Gamut::ACES_AP1, Gamut::AdobeRGB, Gamut::XYZ_D65};

} // namespace

TEST(ColorGamutConversionTest, Rec709RedMapsToDocumentedD65XyzReference)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    ColorGamutConversion::convert(1.0f, 0.0f, 0.0f, Gamut::Rec709,
                                  Gamut::XYZ_D65, x, y, z);

    EXPECT_NEAR(x, 0.4123908f, 1.0e-7f);
    EXPECT_NEAR(y, 0.2126390f, 1.0e-7f);
    EXPECT_NEAR(z, 0.0193308f, 1.0e-7f);
}

TEST(ColorGamutConversionTest, Rec709GreenBlueAndWhiteMapToD65XyzReferences)
{
    constexpr std::array<std::array<float, 6>, 3> cases = {{
        {{0.0f, 1.0f, 0.0f, 0.3575843f, 0.7151687f, 0.1191950f}},
        {{0.0f, 0.0f, 1.0f, 0.1804808f, 0.0721923f, 0.9505321f}},
        {{1.0f, 1.0f, 1.0f, 0.9504559f, 1.0f, 1.0890579f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::Rec709,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-7f);
        EXPECT_NEAR(y, testCase[4], 2.0e-7f);
        EXPECT_NEAR(z, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, Rec2020PrimariesAndWhiteMapToD65XyzReferences)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.6369580f, 0.2627002f, 0.0f}},
        {{0.0f, 1.0f, 0.0f, 0.1446169f, 0.6779981f, 0.0280727f}},
        {{0.0f, 0.0f, 1.0f, 0.1688810f, 0.0593017f, 1.0609851f}},
        {{1.0f, 1.0f, 1.0f, 0.9504559f, 1.0f, 1.0890578f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::Rec2020,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-7f);
        EXPECT_NEAR(y, testCase[4], 2.0e-7f);
        EXPECT_NEAR(z, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, D65WhiteMapsToRec2020Neutral)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    ColorGamutConversion::convert(0.9504559f, 1.0f, 1.0890578f,
                                  Gamut::XYZ_D65, Gamut::Rec2020,
                                  red, green, blue);

    EXPECT_NEAR(red, 1.0f, 2.0e-6f);
    EXPECT_NEAR(green, 1.0f, 2.0e-6f);
    EXPECT_NEAR(blue, 1.0f, 2.0e-6f);
}

TEST(ColorGamutConversionTest, AcesAp1PrimariesAdaptFromD60ToD65Xyz)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.6522375f, 0.2676718f, -0.0053817f}},
        {{0.0f, 1.0f, 0.0f, 0.1282361f, 0.6743390f, 0.0013691f}},
        {{0.0f, 0.0f, 1.0f, 0.1699823f, 0.0579877f, 1.0930699f}},
        {{1.0f, 1.0f, 1.0f, 0.9504559f, 0.9999985f, 1.0890572f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::ACES_AP1,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-6f);
        EXPECT_NEAR(y, testCase[4], 2.0e-6f);
        EXPECT_NEAR(z, testCase[5], 2.0e-6f);
    }
}

TEST(ColorGamutConversionTest, AcesAp1PrimariesConvertToAcesAp0InD60)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.6954522f, 0.0447946f, -0.0055259f}},
        {{0.0f, 1.0f, 0.0f, 0.1406787f, 0.8596711f, 0.0040252f}},
        {{0.0f, 0.0f, 1.0f, 0.1638691f, 0.0955343f, 1.0015007f}},
        {{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},
    }};

    for (const auto& testCase : cases) {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::ACES_AP1,
            Gamut::ACES_AP0, red, green, blue);
        EXPECT_NEAR(red, testCase[3], 2.0e-6f);
        EXPECT_NEAR(green, testCase[4], 2.0e-6f);
        EXPECT_NEAR(blue, testCase[5], 2.0e-6f);
    }
}

TEST(ColorGamutConversionTest, AcesAp0PrimariesAdaptFromD60ToD65Xyz)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.9382798f, 0.3373684f, 0.0011741f}},
        {{0.0f, 1.0f, 0.0f, -0.0044515f, 0.7295205f, -0.0037107f}},
        {{0.0f, 0.0f, 1.0f, 0.0166275f, -0.0668904f, 1.0915939f}},
        {{1.0f, 1.0f, 1.0f, 0.9504559f, 0.9999985f, 1.0890572f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::ACES_AP0,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-6f);
        EXPECT_NEAR(y, testCase[4], 2.0e-6f);
        EXPECT_NEAR(z, testCase[5], 2.0e-6f);
    }
}

TEST(ColorGamutConversionTest, AcesAp0PrimariesConvertToAcesAp1InD60)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 1.4514393f, -0.0765538f, 0.0083162f}},
        {{0.0f, 1.0f, 0.0f, -0.2365108f, 1.1762297f, -0.0060324f}},
        {{0.0f, 0.0f, 1.0f, -0.2149286f, -0.0996759f, 0.9977163f}},
        {{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},
    }};

    for (const auto& testCase : cases) {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::ACES_AP0,
            Gamut::ACES_AP1, red, green, blue);
        EXPECT_NEAR(red, testCase[3], 2.0e-6f);
        EXPECT_NEAR(green, testCase[4], 2.0e-6f);
        EXPECT_NEAR(blue, testCase[5], 2.0e-6f);
    }
}

TEST(ColorGamutConversionTest, DisplayP3PrimariesAndWhiteMapToD65XyzReferences)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.4865709f, 0.2289746f, 0.0f}},
        {{0.0f, 1.0f, 0.0f, 0.2656677f, 0.6917385f, 0.0451134f}},
        {{0.0f, 0.0f, 1.0f, 0.1982173f, 0.0792869f, 1.0439444f}},
        {{1.0f, 1.0f, 1.0f, 0.9504559f, 1.0f, 1.0890578f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::DisplayP3,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-7f);
        EXPECT_NEAR(y, testCase[4], 2.0e-7f);
        EXPECT_NEAR(z, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, D65WhiteMapsToDisplayP3Neutral)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    ColorGamutConversion::convert(0.9504559f, 1.0f, 1.0890578f,
                                  Gamut::XYZ_D65, Gamut::DisplayP3,
                                  red, green, blue);

    EXPECT_NEAR(red, 1.0f, 2.0e-6f);
    EXPECT_NEAR(green, 1.0f, 2.0e-6f);
    EXPECT_NEAR(blue, 1.0f, 2.0e-6f);
}

TEST(ColorGamutConversionTest, DciP3PrimariesMapToPresetXyzReferences)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.4451698f, 0.2094917f, 0.0f}},
        {{0.0f, 1.0f, 0.0f, 0.2771344f, 0.7215953f, 0.0470606f}},
        {{0.0f, 0.0f, 1.0f, 0.1722827f, 0.0689131f, 0.9073554f}},
        {{1.0f, 1.0f, 1.0f, 0.8945869f, 1.0000001f, 0.9544160f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::DCI_P3,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-7f);
        EXPECT_NEAR(y, testCase[4], 2.0e-7f);
        EXPECT_NEAR(z, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, DciP3PresetWhiteMapsBackToNeutral)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    ColorGamutConversion::convert(0.8945869f, 1.0000001f, 0.9544160f,
                                  Gamut::XYZ_D65, Gamut::DCI_P3,
                                  red, green, blue);

    EXPECT_NEAR(red, 1.0f, 2.0e-6f);
    EXPECT_NEAR(green, 1.0f, 2.0e-6f);
    EXPECT_NEAR(blue, 1.0f, 2.0e-6f);
}

TEST(ColorGamutConversionTest, AdobeRgbPrimariesMapToPresetXyzReferences)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.6097559f, 0.3111242f, 0.0194816f}},
        {{0.0f, 1.0f, 0.0f, 0.2052401f, 0.6256560f, 0.0608902f}},
        {{0.0f, 0.0f, 1.0f, 0.1492240f, 0.0632197f, 0.7448387f}},
        {{1.0f, 1.0f, 1.0f, 0.9642200f, 0.9999999f, 0.8252105f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::AdobeRGB,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-7f);
        EXPECT_NEAR(y, testCase[4], 2.0e-7f);
        EXPECT_NEAR(z, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, AdobeRgbPresetWhiteMapsBackToNeutral)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    ColorGamutConversion::convert(0.9642200f, 0.9999999f, 0.8252105f,
                                  Gamut::XYZ_D65, Gamut::AdobeRGB,
                                  red, green, blue);

    EXPECT_NEAR(red, 1.0f, 2.0e-6f);
    EXPECT_NEAR(green, 1.0f, 2.0e-6f);
    EXPECT_NEAR(blue, 1.0f, 2.0e-6f);
}

TEST(ColorGamutConversionTest, DaVinciWideGamutPrimariesMapToPresetXyzReferences)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 0.7007449f, 0.2741010f, -0.0989175f}},
        {{0.0f, 1.0f, 0.0f, 0.1487773f, 0.8736398f, 0.1576015f}},
        {{0.0f, 0.0f, 1.0f, 0.1010455f, -0.1477407f, 0.9016143f}},
        {{1.0f, 1.0f, 1.0f, 0.9505677f, 1.0000001f, 0.9602983f}},
    }};

    for (const auto& testCase : cases) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::DaVinciWideGamut,
            Gamut::XYZ_D65, x, y, z);
        EXPECT_NEAR(x, testCase[3], 2.0e-7f);
        EXPECT_NEAR(y, testCase[4], 2.0e-7f);
        EXPECT_NEAR(z, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, DaVinciWideGamutInverseMatrixReferences)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 1.5435783f, -0.4826754f, 0.1666435f}},
        {{0.0f, 1.0f, 0.0f, -0.2488530f, 1.2609685f, -0.2304632f}},
        {{0.0f, 0.0f, 1.0f, -0.1820373f, 0.1746234f, 1.1159845f}},
        {{0.9505677f, 1.0000001f, 0.9602983f,
          1.0436125f, 0.9698435f, 0.9996207f}},
    }};

    for (const auto& testCase : cases) {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::XYZ_D65,
            Gamut::DaVinciWideGamut, red, green, blue);
        EXPECT_NEAR(red, testCase[3], 2.0e-7f);
        EXPECT_NEAR(green, testCase[4], 2.0e-7f);
        EXPECT_NEAR(blue, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, DaVinciWideGamutToRec709MapsPrimaryAndSignedHdrSamples)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 1.8990162f, -0.1690985f, -0.1214808f}},
        {{0.0f, 1.0f, 0.0f, -0.9395182f, 1.5012676f, -0.0033456f}},
        {{0.0f, 0.0f, 1.0f, 0.1050649f, -0.3376278f, 0.9887375f}},
        {{-0.25f, 0.5f, 2.0f, -0.7343834f, 0.1176528f, 2.0061724f}},
    }};

    for (const auto& testCase : cases) {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::DaVinciWideGamut,
            Gamut::Rec709, red, green, blue);
        EXPECT_NEAR(red, testCase[3], 2.0e-6f);
        EXPECT_NEAR(green, testCase[4], 2.0e-6f);
        EXPECT_NEAR(blue, testCase[5], 2.0e-6f);
    }
}

TEST(ColorGamutConversionTest, Rec2020InverseMatrixMapsD65XyzAxes)
{
    constexpr std::array<std::array<float, 6>, 4> cases = {{
        {{1.0f, 0.0f, 0.0f, 1.7166512f, -0.6666844f, 0.0176399f}},
        {{0.0f, 1.0f, 0.0f, -0.3556708f, 1.6164812f, -0.0427706f}},
        {{0.0f, 0.0f, 1.0f, -0.2533663f, 0.0157685f, 0.9421031f}},
        {{0.9504559f, 1.0f, 1.0890578f, 1.0f, 1.0f, 1.0f}},
    }};

    for (const auto& testCase : cases) {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        ColorGamutConversion::convert(
            testCase[0], testCase[1], testCase[2], Gamut::XYZ_D65,
            Gamut::Rec2020, red, green, blue);
        EXPECT_NEAR(red, testCase[3], 2.0e-7f);
        EXPECT_NEAR(green, testCase[4], 2.0e-7f);
        EXPECT_NEAR(blue, testCase[5], 2.0e-7f);
    }
}

TEST(ColorGamutConversionTest, MatrixMultiplicationUsesRowMajorComposition)
{
    const Matrix3x3 left = {{{1.0f, 2.0f, 3.0f},
                             {0.0f, 1.0f, 4.0f},
                             {5.0f, 6.0f, 0.0f}}};
    const Matrix3x3 right = {{{-2.0f, 1.0f, 0.0f},
                              {3.0f, 0.0f, 0.0f},
                              {4.0f, 5.0f, 1.0f}}};
    const Matrix3x3 product = multiply(left, right);
    constexpr std::array<std::array<float, 3>, 3> expected = {{
        {{16.0f, 16.0f, 3.0f}},
        {{19.0f, 20.0f, 4.0f}},
        {{8.0f, 5.0f, 0.0f}},
    }};

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_FLOAT_EQ(product[row][column], expected[row][column]);
        }
    }
    const Matrix3x3 identity = ArtifactCore::identity();
    const Matrix3x3 leftIdentity = multiply(identity, left);
    const Matrix3x3 rightIdentity = multiply(left, identity);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_FLOAT_EQ(leftIdentity[row][column], left[row][column]);
            EXPECT_FLOAT_EQ(rightIdentity[row][column], left[row][column]);
        }
    }
}

TEST(ColorGamutConversionTest, MatrixVectorMultiplicationUsesRowDotProducts)
{
    const Matrix3x3 matrix = {{{1.0f, 2.0f, 3.0f},
                               {0.0f, 1.0f, 4.0f},
                               {5.0f, 6.0f, 0.0f}}};
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    multiply(matrix, 1.0f, 2.0f, 3.0f, red, green, blue);

    EXPECT_FLOAT_EQ(red, 14.0f);
    EXPECT_FLOAT_EQ(green, 14.0f);
    EXPECT_FLOAT_EQ(blue, 17.0f);
}

TEST(ColorGamutConversionTest, GamutConversionPreservesMatrixLinearity)
{
    constexpr std::array<float, 3> first = {-0.2f, 0.75f, 1.25f};
    constexpr std::array<float, 3> second = {0.4f, -0.1f, 2.0f};
    constexpr float scale = -2.25f;
    const auto convert = [](const std::array<float, 3>& rgb) {
        std::array<float, 3> result{};
        ColorGamutConversion::convert(
            rgb[0], rgb[1], rgb[2], Gamut::Rec709, Gamut::Rec2020,
            result[0], result[1], result[2]);
        return result;
    };

    const std::array<float, 3> convertedFirst = convert(first);
    const std::array<float, 3> convertedSecond = convert(second);
    std::array<float, 3> sum{};
    std::array<float, 3> scaled{};
    for (int channel = 0; channel < 3; ++channel) {
        sum[channel] = first[channel] + second[channel];
        scaled[channel] = first[channel] * scale;
    }
    const std::array<float, 3> convertedSum = convert(sum);
    const std::array<float, 3> convertedScaled = convert(scaled);

    for (int channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(convertedSum[channel],
                    convertedFirst[channel] + convertedSecond[channel],
                    1.0e-6f);
        EXPECT_NEAR(convertedScaled[channel], convertedFirst[channel] * scale,
                    1.0e-6f);
    }
}

TEST(ColorGamutConversionTest, SameGamutMatrixIsExactIdentityForAllPresets)
{
    for (const Gamut gamut : kGamuts) {
        SCOPED_TRACE(static_cast<int>(gamut));
        const Matrix3x3 matrix =
            ColorGamutConversion::getConversionMatrix(gamut, gamut);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                EXPECT_FLOAT_EQ(matrix[row][column],
                               row == column ? 1.0f : 0.0f);
            }
        }
    }
}

TEST(ColorGamutConversionTest, SrgbAndRec709ConvertAsEquivalentPrimaries)
{
    const Matrix3x3 srgbToXyz = ColorGamutConversion::getConversionMatrix(
        Gamut::sRGB, Gamut::XYZ_D65);
    const Matrix3x3 rec709ToXyz = ColorGamutConversion::getConversionMatrix(
        Gamut::Rec709, Gamut::XYZ_D65);
    const Matrix3x3 xyzToSrgb = ColorGamutConversion::getConversionMatrix(
        Gamut::XYZ_D65, Gamut::sRGB);
    const Matrix3x3 xyzToRec709 = ColorGamutConversion::getConversionMatrix(
        Gamut::XYZ_D65, Gamut::Rec709);
    constexpr std::array<std::array<float, 3>, 3> samples = {{
        {{0.18f, 0.18f, 0.18f}},
        {{1.0f, 0.0f, 0.0f}},
        {{-0.25f, 0.75f, 2.0f}},
    }};

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_FLOAT_EQ(srgbToXyz[row][column],
                            rec709ToXyz[row][column]);
            EXPECT_FLOAT_EQ(xyzToSrgb[row][column],
                            xyzToRec709[row][column]);
        }
    }

    for (const auto& sample : samples) {
        float toRec709R = 0.0f;
        float toRec709G = 0.0f;
        float toRec709B = 0.0f;
        ColorGamutConversion::convert(
            sample[0], sample[1], sample[2], Gamut::sRGB, Gamut::Rec709,
            toRec709R, toRec709G, toRec709B);
        EXPECT_NEAR(toRec709R, sample[0], 1.0e-5f);
        EXPECT_NEAR(toRec709G, sample[1], 1.0e-5f);
        EXPECT_NEAR(toRec709B, sample[2], 1.0e-5f);

        float toSrgbR = 0.0f;
        float toSrgbG = 0.0f;
        float toSrgbB = 0.0f;
        ColorGamutConversion::convert(
            sample[0], sample[1], sample[2], Gamut::Rec709, Gamut::sRGB,
            toSrgbR, toSrgbG, toSrgbB);
        EXPECT_NEAR(toSrgbR, sample[0], 1.0e-5f);
        EXPECT_NEAR(toSrgbG, sample[1], 1.0e-5f);
        EXPECT_NEAR(toSrgbB, sample[2], 1.0e-5f);
    }
}

TEST(ColorGamutConversionTest, UnknownGamutFallsBackToD65XyzDispatch)
{
    constexpr Gamut unknown = static_cast<Gamut>(255);
    constexpr std::array<std::array<Gamut, 3>, 4> cases = {{
        {{unknown, Gamut::Rec709, Gamut::XYZ_D65}},
        {{Gamut::Rec709, unknown, Gamut::XYZ_D65}},
        {{unknown, Gamut::ACES_AP1, Gamut::XYZ_D65}},
        {{Gamut::ACES_AP1, unknown, Gamut::XYZ_D65}},
    }};

    for (const auto& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase[0]));
        SCOPED_TRACE(static_cast<int>(testCase[1]));
        const Matrix3x3 actual = ColorGamutConversion::getConversionMatrix(
            testCase[0], testCase[1]);
        const Matrix3x3 expected = ColorGamutConversion::getConversionMatrix(
            testCase[0] == unknown ? Gamut::XYZ_D65 : testCase[0],
            testCase[1] == unknown ? Gamut::XYZ_D65 : testCase[1]);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                EXPECT_FLOAT_EQ(actual[row][column], expected[row][column]);
            }
        }
    }

    const Matrix3x3 bothUnknown =
        ColorGamutConversion::getConversionMatrix(unknown, unknown);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_FLOAT_EQ(bothUnknown[row][column],
                           row == column ? 1.0f : 0.0f);
        }
    }
}

TEST(ColorGamutConversionTest, BradfordMatricesMapD60AndD65WhitePoints)
{
    constexpr std::array<float, 3> d60White = {
        0.9526461f, 1.0f, 1.0088252f};
    constexpr std::array<float, 3> d65White = {
        0.9504559f, 1.0f, 1.0890581f};
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    multiply(ColorGamutConversion::d60ToD65(), d60White[0], d60White[1],
             d60White[2], x, y, z);
    EXPECT_NEAR(x, d65White[0], 2.0e-5f);
    EXPECT_NEAR(y, d65White[1], 2.0e-5f);
    EXPECT_NEAR(z, d65White[2], 2.0e-5f);

    multiply(ColorGamutConversion::d65ToD60(), d65White[0], d65White[1],
             d65White[2], x, y, z);
    EXPECT_NEAR(x, d60White[0], 2.0e-5f);
    EXPECT_NEAR(y, d60White[1], 2.0e-5f);
    EXPECT_NEAR(z, d60White[2], 2.0e-5f);

    const Matrix3x3 roundTrip = multiply(
        ColorGamutConversion::d65ToD60(),
        ColorGamutConversion::d60ToD65());
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_NEAR(roundTrip[row][column],
                        row == column ? 1.0f : 0.0f, 2.0e-5f);
        }
    }
}

TEST(ColorGamutConversionTest, BradfordAdaptationRoundTripsSignedXyzAndHdrSamples)
{
    constexpr std::array<std::array<float, 3>, 5> samples = {{
        {{0.0f, 0.0f, 0.0f}},
        {{0.9526461f, 1.0f, 1.0088252f}},
        {{0.125f, 0.5f, 0.875f}},
        {{-0.25f, 0.4f, 1.75f}},
        {{8.0f, -2.0f, 0.125f}},
    }};
    const Matrix3x3 d60ToD65 = ColorGamutConversion::d60ToD65();
    const Matrix3x3 d65ToD60 = ColorGamutConversion::d65ToD60();

    for (const auto& sample : samples) {
        float d65X = 0.0f;
        float d65Y = 0.0f;
        float d65Z = 0.0f;
        multiply(d60ToD65, sample[0], sample[1], sample[2], d65X, d65Y, d65Z);
        float restoredD60X = 0.0f;
        float restoredD60Y = 0.0f;
        float restoredD60Z = 0.0f;
        multiply(d65ToD60, d65X, d65Y, d65Z,
                 restoredD60X, restoredD60Y, restoredD60Z);

        float d60X = 0.0f;
        float d60Y = 0.0f;
        float d60Z = 0.0f;
        multiply(d65ToD60, sample[0], sample[1], sample[2], d60X, d60Y, d60Z);
        float restoredD65X = 0.0f;
        float restoredD65Y = 0.0f;
        float restoredD65Z = 0.0f;
        multiply(d60ToD65, d60X, d60Y, d60Z,
                 restoredD65X, restoredD65Y, restoredD65Z);

        SCOPED_TRACE(::testing::Message()
            << "XYZ=" << sample[0] << ',' << sample[1] << ',' << sample[2]);
        EXPECT_NEAR(restoredD60X, sample[0], 1.0e-4f * std::max(1.0f, std::abs(sample[0])));
        EXPECT_NEAR(restoredD60Y, sample[1], 1.0e-4f * std::max(1.0f, std::abs(sample[1])));
        EXPECT_NEAR(restoredD60Z, sample[2], 1.0e-4f * std::max(1.0f, std::abs(sample[2])));
        EXPECT_NEAR(restoredD65X, sample[0], 1.0e-4f * std::max(1.0f, std::abs(sample[0])));
        EXPECT_NEAR(restoredD65Y, sample[1], 1.0e-4f * std::max(1.0f, std::abs(sample[1])));
        EXPECT_NEAR(restoredD65Z, sample[2], 1.0e-4f * std::max(1.0f, std::abs(sample[2])));
    }
}

TEST(ColorGamutConversionTest, XyzD60GamutEnumCurrentlyFallsThroughWithoutBradfordAdaptation)
{
    constexpr std::array<float, 3> d60White = {
        0.9526461f, 1.0f, 1.0088252f};
    constexpr std::array<float, 3> d65White = {
        0.9504559f, 1.0f, 1.0890581f};

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    ColorGamutConversion::convert(
        d60White[0], d60White[1], d60White[2],
        Gamut::XYZ_D60, Gamut::XYZ_D65, x, y, z);
    EXPECT_FLOAT_EQ(x, d60White[0]);
    EXPECT_FLOAT_EQ(y, d60White[1]);
    EXPECT_FLOAT_EQ(z, d60White[2]);

    ColorGamutConversion::convert(
        d65White[0], d65White[1], d65White[2],
        Gamut::XYZ_D65, Gamut::XYZ_D60, x, y, z);
    EXPECT_FLOAT_EQ(x, d65White[0]);
    EXPECT_FLOAT_EQ(y, d65White[1]);
    EXPECT_FLOAT_EQ(z, d65White[2]);
}

TEST(ColorGamutConversionTest, BradfordAdaptationPreservesNeutralAcrossAcesAndRec709)
{
    constexpr std::array<std::array<Gamut, 2>, 4> pairs = {{
        {{Gamut::ACES_AP0, Gamut::Rec709}},
        {{Gamut::ACES_AP1, Gamut::Rec709}},
        {{Gamut::Rec709, Gamut::ACES_AP0}},
        {{Gamut::Rec709, Gamut::ACES_AP1}},
    }};

    for (const auto& pair : pairs) {
        SCOPED_TRACE(static_cast<int>(pair[0]));
        SCOPED_TRACE(static_cast<int>(pair[1]));
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        ColorGamutConversion::convert(1.0f, 1.0f, 1.0f, pair[0], pair[1],
                                      red, green, blue);
        EXPECT_NEAR(red, 1.0f, 2.0e-5f);
        EXPECT_NEAR(green, 1.0f, 2.0e-5f);
        EXPECT_NEAR(blue, 1.0f, 2.0e-5f);
    }
}

TEST(ColorGamutConversionTest, D65MatrixPairsRoundTripSignedAndHdrSamples)
{
    constexpr float values[] = {
        -1.0f, -0.125f, 0.0f, 0.01f, 0.18f, 0.5f,
        1.0f, 2.0f, 4.0f, 8.0f, 16.0f,
    };

    for (const Gamut source : kD65RoundTripGamuts) {
        for (const Gamut target : kD65RoundTripGamuts) {
            SCOPED_TRACE(static_cast<int>(source));
            SCOPED_TRACE(static_cast<int>(target));
            for (const float inputR : values) {
                for (const float inputG : values) {
                    for (const float inputB : values) {
                        float convertedR = 0.0f;
                        float convertedG = 0.0f;
                        float convertedB = 0.0f;
                        ColorGamutConversion::convert(
                            inputR, inputG, inputB, source, target,
                            convertedR, convertedG, convertedB);

                        float restoredR = 0.0f;
                        float restoredG = 0.0f;
                        float restoredB = 0.0f;
                        ColorGamutConversion::convert(
                            convertedR, convertedG, convertedB, target, source,
                            restoredR, restoredG, restoredB);

                        SCOPED_TRACE(::testing::Message()
                            << "input=" << inputR << ',' << inputG << ',' << inputB);
                        EXPECT_NEAR(restoredR, inputR,
                                    2.0e-3f * std::max(1.0f, std::abs(inputR)));
                        EXPECT_NEAR(restoredG, inputG,
                                    2.0e-3f * std::max(1.0f, std::abs(inputG)));
                        EXPECT_NEAR(restoredB, inputB,
                                    2.0e-3f * std::max(1.0f, std::abs(inputB)));
                    }
                }
            }
        }
    }
}

TEST(ColorGamutConversionTest, AcesAndD65GamutPairsRoundTripSignedAndHdrSamples)
{
    constexpr std::array<std::array<Gamut, 2>, 4> pairs = {{
        {{Gamut::ACES_AP0, Gamut::Rec709}},
        {{Gamut::ACES_AP1, Gamut::Rec709}},
        {{Gamut::ACES_AP0, Gamut::XYZ_D65}},
        {{Gamut::ACES_AP1, Gamut::XYZ_D65}},
    }};
    constexpr float values[] = {
        -1.0f, -0.125f, 0.0f, 0.01f, 0.18f, 0.5f,
        1.0f, 2.0f, 4.0f, 8.0f, 16.0f,
    };

    for (const auto& pair : pairs) {
        SCOPED_TRACE(static_cast<int>(pair[0]));
        SCOPED_TRACE(static_cast<int>(pair[1]));
        for (const float inputR : values) {
            for (const float inputG : values) {
                for (const float inputB : values) {
                    float convertedR = 0.0f;
                    float convertedG = 0.0f;
                    float convertedB = 0.0f;
                    ColorGamutConversion::convert(
                        inputR, inputG, inputB, pair[0], pair[1],
                        convertedR, convertedG, convertedB);

                    float restoredR = 0.0f;
                    float restoredG = 0.0f;
                    float restoredB = 0.0f;
                    ColorGamutConversion::convert(
                        convertedR, convertedG, convertedB, pair[1], pair[0],
                        restoredR, restoredG, restoredB);

                    SCOPED_TRACE(::testing::Message()
                        << "input=" << inputR << ',' << inputG << ',' << inputB);
                    EXPECT_NEAR(restoredR, inputR,
                                3.0e-4f * std::max(1.0f, std::abs(inputR)));
                    EXPECT_NEAR(restoredG, inputG,
                                3.0e-4f * std::max(1.0f, std::abs(inputG)));
                    EXPECT_NEAR(restoredB, inputB,
                                3.0e-4f * std::max(1.0f, std::abs(inputB)));
                }
            }
        }
    }
}

TEST(ColorGamutConversionTest, ConversionPropagatesNonFiniteChannelValues)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    ColorGamutConversion::convert(
        std::numeric_limits<float>::quiet_NaN(), 0.25f, 0.75f,
        Gamut::Rec709, Gamut::Rec2020, red, green, blue);
    EXPECT_TRUE(std::isnan(red));
    EXPECT_TRUE(std::isnan(green));
    EXPECT_TRUE(std::isnan(blue));

    ColorGamutConversion::convert(
        std::numeric_limits<float>::infinity(), 0.25f, 0.75f,
        Gamut::Rec709, Gamut::Rec2020, red, green, blue);
    EXPECT_TRUE(std::isinf(red));
    EXPECT_TRUE(std::isinf(green));
    EXPECT_TRUE(std::isinf(blue));
}

TEST(ColorGamutConversionTest, EveryGamutPairProducesFiniteSignedAndHdrValues)
{
    constexpr std::array<std::array<float, 3>, 4> samples = {{
        {{0.18f, 0.18f, 0.18f}},
        {{1.0f, 0.0f, 0.0f}},
        {{-0.125f, 0.5f, 1.75f}},
        {{16.0f, 4.0f, 0.25f}},
    }};

    for (const Gamut source : kGamuts) {
        for (const Gamut target : kGamuts) {
            SCOPED_TRACE(static_cast<int>(source));
            SCOPED_TRACE(static_cast<int>(target));
            for (const auto& sample : samples) {
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                ColorGamutConversion::convert(
                    sample[0], sample[1], sample[2], source, target, red,
                    green, blue);
                EXPECT_TRUE(std::isfinite(red));
                EXPECT_TRUE(std::isfinite(green));
                EXPECT_TRUE(std::isfinite(blue));
            }
        }
    }
}

TEST(ColorGamutConversionTest, EveryGamutPairIsHomogeneousAcrossSignedHdrScaleGrid)
{
    constexpr std::array<std::array<float, 3>, 6> samples = {{
        {{0.0f, 0.0f, 0.0f}},
        {{0.125f, 0.5f, 0.875f}},
        {{1.0f, 0.0f, 0.0f}},
        {{-0.125f, 0.5f, 1.75f}},
        {{16.0f, 4.0f, 0.25f}},
        {{-2.0f, -0.5f, 0.75f}},
    }};
    constexpr std::array<float, 5> scales = {-4.0f, -0.5f, 0.0f, 0.25f, 3.0f};

    for (const Gamut source : kGamuts) {
        for (const Gamut target : kGamuts) {
            for (const auto& sample : samples) {
                float baseR = 0.0f;
                float baseG = 0.0f;
                float baseB = 0.0f;
                ColorGamutConversion::convert(
                    sample[0], sample[1], sample[2], source, target,
                    baseR, baseG, baseB);

                for (const float scale : scales) {
                    float scaledR = 0.0f;
                    float scaledG = 0.0f;
                    float scaledB = 0.0f;
                    ColorGamutConversion::convert(
                        sample[0] * scale, sample[1] * scale, sample[2] * scale,
                        source, target, scaledR, scaledG, scaledB);
                    SCOPED_TRACE(::testing::Message()
                        << "source=" << static_cast<int>(source)
                        << " target=" << static_cast<int>(target)
                        << " scale=" << scale);
                    EXPECT_NEAR(scaledR, baseR * scale,
                                2.0e-5f * std::max(1.0f, std::abs(baseR * scale)));
                    EXPECT_NEAR(scaledG, baseG * scale,
                                2.0e-5f * std::max(1.0f, std::abs(baseG * scale)));
                    EXPECT_NEAR(scaledB, baseB * scale,
                                2.0e-5f * std::max(1.0f, std::abs(baseB * scale)));
                }
            }
        }
    }
}

TEST(ColorGamutConversionTest, ComposedConversionMatricesMatchSequentialConversions)
{
    constexpr std::array<std::array<float, 3>, 5> samples = {{
        {{0.18f, 0.18f, 0.18f}},
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 0.75f, 0.25f}},
        {{-0.25f, 0.5f, 2.0f}},
        {{8.0f, -1.0f, 0.125f}},
    }};

    for (const Gamut source : kGamuts) {
        for (const Gamut intermediate : kGamuts) {
            for (const Gamut target : kGamuts) {
                const Matrix3x3 sourceToIntermediate =
                    ColorGamutConversion::getConversionMatrix(source, intermediate);
                const Matrix3x3 intermediateToTarget =
                    ColorGamutConversion::getConversionMatrix(intermediate, target);
                const Matrix3x3 composed = multiply(
                    intermediateToTarget, sourceToIntermediate);

                SCOPED_TRACE(::testing::Message()
                    << "source=" << static_cast<int>(source)
                    << " intermediate=" << static_cast<int>(intermediate)
                    << " target=" << static_cast<int>(target));
                for (const auto& sample : samples) {
                    float matrixR = 0.0f;
                    float matrixG = 0.0f;
                    float matrixB = 0.0f;
                    multiply(composed, sample[0], sample[1], sample[2],
                             matrixR, matrixG, matrixB);

                    float firstR = 0.0f;
                    float firstG = 0.0f;
                    float firstB = 0.0f;
                    ColorGamutConversion::convert(
                        sample[0], sample[1], sample[2], source, intermediate,
                        firstR, firstG, firstB);
                    float sequentialR = 0.0f;
                    float sequentialG = 0.0f;
                    float sequentialB = 0.0f;
                    ColorGamutConversion::convert(
                        firstR, firstG, firstB, intermediate, target,
                        sequentialR, sequentialG, sequentialB);

                    EXPECT_NEAR(matrixR, sequentialR, 2.0e-5f);
                    EXPECT_NEAR(matrixG, sequentialG, 2.0e-5f);
                    EXPECT_NEAR(matrixB, sequentialB, 2.0e-5f);
                }
            }
        }
    }
}

TEST(ColorGamutConversionTest, ConversionIsTransitiveAcrossSupportedGamutTriples)
{
    constexpr std::array<std::array<float, 3>, 8> samples = {{
        {{0.0f, 0.0f, 0.0f}},
        {{0.18f, 0.18f, 0.18f}},
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}},
        {{-0.125f, 0.5f, 1.75f}},
        {{16.0f, 4.0f, 0.25f}},
        {{-2.0f, -0.5f, 0.75f}},
    }};

    for (const Gamut source : kComposableGamutReferences) {
        for (const Gamut intermediate : kComposableGamutReferences) {
            for (const Gamut target : kComposableGamutReferences) {
                SCOPED_TRACE(::testing::Message()
                    << "source=" << static_cast<int>(source)
                    << " intermediate=" << static_cast<int>(intermediate)
                    << " target=" << static_cast<int>(target));
                for (const auto& sample : samples) {
                    float directR = 0.0f;
                    float directG = 0.0f;
                    float directB = 0.0f;
                    ColorGamutConversion::convert(
                        sample[0], sample[1], sample[2], source, target,
                        directR, directG, directB);

                    float intermediateR = 0.0f;
                    float intermediateG = 0.0f;
                    float intermediateB = 0.0f;
                    ColorGamutConversion::convert(
                        sample[0], sample[1], sample[2], source, intermediate,
                        intermediateR, intermediateG, intermediateB);

                    float chainedR = 0.0f;
                    float chainedG = 0.0f;
                    float chainedB = 0.0f;
                    ColorGamutConversion::convert(
                        intermediateR, intermediateG, intermediateB,
                        intermediate, target, chainedR, chainedG, chainedB);

                    EXPECT_NEAR(chainedR, directR,
                                1.0e-4f * std::max(1.0f, std::abs(directR)));
                    EXPECT_NEAR(chainedG, directG,
                                1.0e-4f * std::max(1.0f, std::abs(directG)));
                    EXPECT_NEAR(chainedB, directB,
                                1.0e-4f * std::max(1.0f, std::abs(directB)));
                }
            }
        }
    }
}

TEST(ColorGamutConversionTest, ConversionSupportsAliasingOutputWithInputVariables)
{
    constexpr std::array<float, 3> source = {-0.125f, 0.375f, 1.25f};

    for (const Gamut sourceGamut : kGamuts) {
        for (const Gamut targetGamut : kGamuts) {
            float expectedR = 0.0f;
            float expectedG = 0.0f;
            float expectedB = 0.0f;
            ColorGamutConversion::convert(
                source[0], source[1], source[2], sourceGamut, targetGamut,
                expectedR, expectedG, expectedB);

            float inPlaceR = source[0];
            float inPlaceG = source[1];
            float inPlaceB = source[2];
            ColorGamutConversion::convert(
                inPlaceR, inPlaceG, inPlaceB, sourceGamut, targetGamut,
                inPlaceR, inPlaceG, inPlaceB);

            SCOPED_TRACE(::testing::Message()
                << "source=" << static_cast<int>(sourceGamut)
                << " target=" << static_cast<int>(targetGamut));
            EXPECT_FLOAT_EQ(inPlaceR, expectedR);
            EXPECT_FLOAT_EQ(inPlaceG, expectedG);
            EXPECT_FLOAT_EQ(inPlaceB, expectedB);
        }
    }
}
