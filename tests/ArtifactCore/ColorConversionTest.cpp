#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>

import Color.Conversion;

using namespace ArtifactCore;

TEST(ColorConversionTest, RGBToHSVMapsPrimaryAndSecondaryHues)
{
    const auto red = ColorConversion::RGBToHSV(1.0f, 0.0f, 0.0f);
    const auto yellow = ColorConversion::RGBToHSV(1.0f, 1.0f, 0.0f);
    const auto green = ColorConversion::RGBToHSV(0.0f, 1.0f, 0.0f);
    const auto cyan = ColorConversion::RGBToHSV(0.0f, 1.0f, 1.0f);
    const auto blue = ColorConversion::RGBToHSV(0.0f, 0.0f, 1.0f);
    const auto magenta = ColorConversion::RGBToHSV(1.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(red.h, 0.0f);
    EXPECT_FLOAT_EQ(yellow.h, 60.0f);
    EXPECT_FLOAT_EQ(green.h, 120.0f);
    EXPECT_FLOAT_EQ(cyan.h, 180.0f);
    EXPECT_FLOAT_EQ(blue.h, 240.0f);
    EXPECT_FLOAT_EQ(magenta.h, 300.0f);
    for (const auto& color : {red, yellow, green, cyan, blue, magenta}) {
        EXPECT_FLOAT_EQ(color.s, 1.0f);
        EXPECT_FLOAT_EQ(color.v, 1.0f);
    }
}

TEST(ColorConversionTest, AchromaticValuesHaveZeroHueAndSaturation)
{
    for (const float value : {0.0f, 0.18f, 0.5f, 1.0f}) {
        const auto hsv = ColorConversion::RGBToHSV(value, value, value);
        EXPECT_FLOAT_EQ(hsv.h, 0.0f);
        EXPECT_FLOAT_EQ(hsv.s, 0.0f);
        EXPECT_FLOAT_EQ(hsv.v, value);

        const auto hsl = ColorConversion::RGBToHSL(value, value, value);
        EXPECT_FLOAT_EQ(hsl.h, 0.0f);
        EXPECT_FLOAT_EQ(hsl.s, 0.0f);
        EXPECT_FLOAT_EQ(hsl.l, value);
    }
}

TEST(ColorConversionTest, NearAchromaticHsvUsesDeltaThresholdWithoutChangingHslHue)
{
    const auto belowHsvThreshold = ColorConversion::RGBToHSV(0.5f, 0.5f, 0.500004f);
    const auto belowHslThreshold = ColorConversion::RGBToHSL(0.5f, 0.5f, 0.500004f);
    EXPECT_FLOAT_EQ(belowHsvThreshold.h, 0.0f);
    EXPECT_FLOAT_EQ(belowHsvThreshold.s, 0.0f);
    EXPECT_FLOAT_EQ(belowHsvThreshold.v, 0.500004f);
    EXPECT_FLOAT_EQ(belowHslThreshold.h, 240.0f);
    EXPECT_GT(belowHslThreshold.s, 0.0f);

    const auto aboveHsvThreshold = ColorConversion::RGBToHSV(0.5f, 0.5f, 0.50002f);
    EXPECT_NEAR(aboveHsvThreshold.h, 240.0f, 1e-4f);
    EXPECT_NEAR(aboveHsvThreshold.s, (0.50002f - 0.5f) / 0.50002f, 1e-7f);
    const auto restored = ColorConversion::HSVToRGB(aboveHsvThreshold);
    EXPECT_NEAR(restored[0], 0.5f, 1e-6f);
    EXPECT_NEAR(restored[1], 0.5f, 1e-6f);
    EXPECT_NEAR(restored[2], 0.50002f, 1e-6f);
}

TEST(ColorConversionTest, ZeroSaturationInverseConversionsProduceNeutralRGB)
{
    const auto hsv = ColorConversion::HSVToRGB({237.0f, 0.0f, 0.42f});
    const auto hsl = ColorConversion::HSLToRGB({123.0f, 0.0f, 0.68f});

    EXPECT_FLOAT_EQ(hsv[0], 0.42f);
    EXPECT_FLOAT_EQ(hsv[1], 0.42f);
    EXPECT_FLOAT_EQ(hsv[2], 0.42f);
    EXPECT_FLOAT_EQ(hsl[0], 0.68f);
    EXPECT_FLOAT_EQ(hsl[1], 0.68f);
    EXPECT_FLOAT_EQ(hsl[2], 0.68f);
}

TEST(ColorConversionTest, HSVHueWrapsAtFullTurnsAndAcrossZero)
{
    const auto zero = ColorConversion::HSVToRGB({0.0f, 1.0f, 1.0f});
    const auto fullTurn = ColorConversion::HSVToRGB({360.0f, 1.0f, 1.0f});
    const auto nextTurn = ColorConversion::HSVToRGB({420.0f, 1.0f, 1.0f});
    const auto negative = ColorConversion::HSVToRGB({-60.0f, 1.0f, 1.0f});

    EXPECT_EQ(zero, (std::array<float, 3>{1.0f, 0.0f, 0.0f}));
    EXPECT_EQ(fullTurn, zero);
    EXPECT_EQ(nextTurn, (std::array<float, 3>{1.0f, 1.0f, 0.0f}));
    EXPECT_EQ(negative, (std::array<float, 3>{1.0f, 0.0f, 1.0f}));
}

TEST(ColorConversionTest, HSVHueWrapsAcrossManyPositiveAndNegativeTurns)
{
    constexpr float saturation = 0.83f;
    constexpr float value = 0.72f;
    constexpr float canonicalHue = 213.0f;
    const auto expected = ColorConversion::HSVToRGB(
        {canonicalHue, saturation, value});
    constexpr int turnCounts[] = {-64, -17, -2, -1, 1, 2, 17, 64};

    for (const int turns : turnCounts) {
        const float hue = canonicalHue + 360.0f * turns;
        const auto actual = ColorConversion::HSVToRGB({hue, saturation, value});
        SCOPED_TRACE(::testing::Message() << "hue=" << hue);
        for (std::size_t channel = 0; channel < actual.size(); ++channel) {
            EXPECT_NEAR(actual[channel], expected[channel], 2e-6f);
        }
    }
}

TEST(ColorConversionTest, HslHueWrapsAcrossAdjacentTurns)
{
    const auto red = ColorConversion::HSLToRGB({0.0f, 1.0f, 0.5f});
    const auto fullTurn = ColorConversion::HSLToRGB({360.0f, 1.0f, 0.5f});
    const auto nextTurnYellow = ColorConversion::HSLToRGB({420.0f, 1.0f, 0.5f});
    const auto previousTurnMagenta = ColorConversion::HSLToRGB({-60.0f, 1.0f, 0.5f});

    constexpr std::array<float, 3> expectedYellow = {1.0f, 1.0f, 0.0f};
    constexpr std::array<float, 3> expectedMagenta = {1.0f, 0.0f, 1.0f};
    for (std::size_t channel = 0; channel < red.size(); ++channel) {
        EXPECT_NEAR(fullTurn[channel], red[channel], 2e-6f);
        EXPECT_NEAR(nextTurnYellow[channel], expectedYellow[channel], 2e-6f);
        EXPECT_NEAR(previousTurnMagenta[channel], expectedMagenta[channel], 2e-6f);
    }
}

TEST(ColorConversionTest, HSVAndHSLMatchEveryHueSectorBoundary)
{
    constexpr float hues[] = {0.0f, 60.0f, 120.0f, 180.0f,
                              240.0f, 300.0f, 360.0f};
    constexpr float expected[][3] = {
        {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
    };

    for (std::size_t index = 0; index < std::size(hues); ++index) {
        const auto hsv = ColorConversion::HSVToRGB({hues[index], 1.0f, 1.0f});
        const auto hsl = ColorConversion::HSLToRGB({hues[index], 1.0f, 0.5f});
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(hsv[channel], expected[index][channel], 1e-6f);
            EXPECT_NEAR(hsl[channel], expected[index][channel], 1e-6f);
        }
    }
}

TEST(ColorConversionTest, HSVAndHSLRoundTripRepresentativeDisplayRGBValues)
{
    constexpr float colors[][3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
        {0.125f, 0.5f, 0.875f},
        {0.9f, 0.2f, 0.4f},
        {0.03f, 0.72f, 0.31f},
    };

    for (const auto& rgb : colors) {
        const auto hsv = ColorConversion::RGBToHSV(rgb[0], rgb[1], rgb[2]);
        const auto fromHsv = ColorConversion::HSVToRGB(hsv);
        const auto hsl = ColorConversion::RGBToHSL(rgb[0], rgb[1], rgb[2]);
        const auto fromHsl = ColorConversion::HSLToRGB(hsl);

        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(fromHsv[static_cast<std::size_t>(channel)], rgb[channel],
                        1e-5f);
            EXPECT_NEAR(fromHsl[static_cast<std::size_t>(channel)], rgb[channel],
                        1e-5f);
        }
    }
}

TEST(ColorConversionTest, RGBToHSLReportsHueAndLightnessAcrossSaturationBranches)
{
    constexpr float colors[][3] = {
        {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f},
    };
    constexpr float hues[] = {0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f};

    for (std::size_t index = 0; index < std::size(hues); ++index) {
        const auto hsl = ColorConversion::RGBToHSL(
            colors[index][0], colors[index][1], colors[index][2]);
        EXPECT_NEAR(hsl.h, hues[index], 1e-6f);
        EXPECT_FLOAT_EQ(hsl.s, 1.0f);
        EXPECT_FLOAT_EQ(hsl.l, 0.5f);
    }

    const auto darkBranch = ColorConversion::RGBToHSL(0.75f, 0.25f, 0.25f);
    EXPECT_FLOAT_EQ(darkBranch.h, 0.0f);
    EXPECT_FLOAT_EQ(darkBranch.s, 0.5f);
    EXPECT_FLOAT_EQ(darkBranch.l, 0.5f);

    const auto lightBranch = ColorConversion::RGBToHSL(0.75f, 0.5f, 0.5f);
    EXPECT_FLOAT_EQ(lightBranch.h, 0.0f);
    EXPECT_NEAR(lightBranch.s, 1.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(lightBranch.l, 0.625f);
}

TEST(ColorConversionTest, RGBToHSLUsesLowerLightnessSaturationFormula)
{
    const auto dark = ColorConversion::RGBToHSL(0.6f, 0.2f, 0.2f);

    EXPECT_FLOAT_EQ(dark.h, 0.0f);
    EXPECT_FLOAT_EQ(dark.s, 0.5f);
    EXPECT_FLOAT_EQ(dark.l, 0.4f);
}

TEST(ColorConversionTest, DenseUnitRgbGridRoundTripsThroughHsvAndHsl)
{
    constexpr int subdivisions = 10;
    for (int redIndex = 0; redIndex <= subdivisions; ++redIndex) {
        for (int greenIndex = 0; greenIndex <= subdivisions; ++greenIndex) {
            for (int blueIndex = 0; blueIndex <= subdivisions; ++blueIndex) {
                const float red = redIndex / static_cast<float>(subdivisions);
                const float green = greenIndex / static_cast<float>(subdivisions);
                const float blue = blueIndex / static_cast<float>(subdivisions);
                SCOPED_TRACE(::testing::Message()
                    << "rgb=(" << red << ", " << green << ", " << blue << ")");

                const auto fromHsv = ColorConversion::HSVToRGB(
                    ColorConversion::RGBToHSV(red, green, blue));
                const auto fromHsl = ColorConversion::HSLToRGB(
                    ColorConversion::RGBToHSL(red, green, blue));

                EXPECT_NEAR(fromHsv[0], red, 1e-5f);
                EXPECT_NEAR(fromHsv[1], green, 1e-5f);
                EXPECT_NEAR(fromHsv[2], blue, 1e-5f);
                EXPECT_NEAR(fromHsl[0], red, 1e-5f);
                EXPECT_NEAR(fromHsl[1], green, 1e-5f);
                EXPECT_NEAR(fromHsl[2], blue, 1e-5f);
            }
        }
    }
}

TEST(ColorConversionTest, HslAndEquivalentHsvParametersProduceTheSameRgbGrid)
{
    constexpr float saturations[] = {0.0f, 0.05f, 0.25f, 0.5f, 0.75f, 1.0f};
    constexpr float lightnesses[] = {
        0.0f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.99f, 1.0f,
    };

    for (int hue = 0; hue < 360; ++hue) {
        for (const float saturation : saturations) {
            for (const float lightness : lightnesses) {
                const float chroma = (1.0f - std::abs(2.0f * lightness - 1.0f))
                    * saturation;
                const float value = lightness + chroma * 0.5f;
                const float hsvSaturation = value == 0.0f ? 0.0f : chroma / value;
                const auto fromHsl = ColorConversion::HSLToRGB(
                    {static_cast<float>(hue), saturation, lightness});
                const auto fromEquivalentHsv = ColorConversion::HSVToRGB(
                    {static_cast<float>(hue), hsvSaturation, value});

                SCOPED_TRACE(::testing::Message()
                    << "hue=" << hue << " saturation=" << saturation
                    << " lightness=" << lightness);
                for (std::size_t channel = 0; channel < fromHsl.size(); ++channel) {
                    EXPECT_NEAR(fromHsl[channel], fromEquivalentHsv[channel], 2.0e-6f)
                        << "channel " << channel;
                }
            }
        }
    }
}

TEST(ColorConversionTest, SignedAndHdrRgbSamplesRoundTripThroughHsvAndHsl)
{
    constexpr float colors[][3] = {
        {-0.25f, 0.5f, 1.25f},
        {1.25f, -0.25f, 0.5f},
        {0.5f, 1.25f, -0.25f},
        {-0.1f, 0.5f, 1.1f},
    };
    constexpr float expectedHue[] = {210.0f, 330.0f, 90.0f, 210.0f};
    constexpr float expectedHsvSaturation[] = {1.2f, 1.2f, 1.2f, 1.2f / 1.1f};
    constexpr float expectedHsvValue[] = {1.25f, 1.25f, 1.25f, 1.1f};
    constexpr float expectedHslSaturation[] = {1.5f, 1.5f, 1.5f, 1.2f};

    for (std::size_t index = 0; index < std::size(colors); ++index) {
        const auto& rgb = colors[index];
        const auto hsv = ColorConversion::RGBToHSV(rgb[0], rgb[1], rgb[2]);
        const auto hsl = ColorConversion::RGBToHSL(rgb[0], rgb[1], rgb[2]);
        const auto fromHsv = ColorConversion::HSVToRGB(hsv);
        const auto fromHsl = ColorConversion::HSLToRGB(hsl);
        SCOPED_TRACE(::testing::Message()
            << "rgb=(" << rgb[0] << ", " << rgb[1] << ", " << rgb[2] << ")");

        EXPECT_TRUE(std::isfinite(hsv.h));
        EXPECT_TRUE(std::isfinite(hsv.s));
        EXPECT_TRUE(std::isfinite(hsv.v));
        EXPECT_NEAR(hsv.h, expectedHue[index], 1e-5f);
        EXPECT_NEAR(hsv.s, expectedHsvSaturation[index], 1e-6f);
        EXPECT_FLOAT_EQ(hsv.v, expectedHsvValue[index]);
        EXPECT_TRUE(std::isfinite(hsl.h));
        EXPECT_TRUE(std::isfinite(hsl.s));
        EXPECT_TRUE(std::isfinite(hsl.l));
        EXPECT_NEAR(hsl.h, expectedHue[index], 1e-5f);
        EXPECT_FLOAT_EQ(hsl.s, expectedHslSaturation[index]);
        EXPECT_FLOAT_EQ(hsl.l, 0.5f);

        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(fromHsv[channel], rgb[channel], 2e-6f)
                << "HSV channel " << channel;
            EXPECT_NEAR(fromHsl[channel], rgb[channel], 2e-6f)
                << "HSL channel " << channel;
        }
    }
}

TEST(ColorConversionTest, HslPrimaryAndSecondaryHueSectorBoundariesRoundTrip)
{
    constexpr float hues[] = {0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f, 360.0f};
    constexpr float expected[][3] = {
        {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
    };

    for (std::size_t index = 0; index < std::size(hues); ++index) {
        const auto rgb = ColorConversion::HSLToRGB({hues[index], 1.0f, 0.5f});
        for (std::size_t channel = 0; channel < rgb.size(); ++channel) {
            EXPECT_NEAR(rgb[channel], expected[index][channel], 1e-6f)
                << "hue " << hues[index] << ", channel " << channel;
        }
    }
}

TEST(ColorConversionTest, OneThirdTurnRotatesRgbChannelsCyclicallyAcrossHueGrid)
{
    constexpr float saturations[] = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};
    constexpr float hsvValues[] = {0.0f, 0.05f, 0.25f, 0.6f, 1.0f};
    constexpr float hslLightness[] = {0.0f, 0.05f, 0.25f, 0.5f, 0.85f, 1.0f};

    for (int hue = 0; hue < 360; ++hue) {
        for (const float saturation : saturations) {
            for (const float value : hsvValues) {
                const auto original = ColorConversion::HSVToRGB(
                    {static_cast<float>(hue), saturation, value});
                const auto rotated = ColorConversion::HSVToRGB(
                    {static_cast<float>(hue + 120), saturation, value});
                SCOPED_TRACE(::testing::Message()
                    << "HSV hue=" << hue << " saturation=" << saturation
                    << " value=" << value);
                EXPECT_NEAR(rotated[0], original[2], 2e-6f);
                EXPECT_NEAR(rotated[1], original[0], 2e-6f);
                EXPECT_NEAR(rotated[2], original[1], 2e-6f);
            }

            for (const float lightness : hslLightness) {
                const auto original = ColorConversion::HSLToRGB(
                    {static_cast<float>(hue), saturation, lightness});
                const auto rotated = ColorConversion::HSLToRGB(
                    {static_cast<float>(hue + 120), saturation, lightness});
                SCOPED_TRACE(::testing::Message()
                    << "HSL hue=" << hue << " saturation=" << saturation
                    << " lightness=" << lightness);
                EXPECT_NEAR(rotated[0], original[2], 2e-6f);
                EXPECT_NEAR(rotated[1], original[0], 2e-6f);
                EXPECT_NEAR(rotated[2], original[1], 2e-6f);
            }
        }
    }
}

TEST(ColorConversionTest, FixedSeedRgbSamplesRoundTripWithFiniteNormalizedComponents)
{
    std::uint32_t state = 0x00C0FFEEu;
    const auto nextUnit = [&state]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8u) / 16777215.0f;
    };

    constexpr int sampleCount = 4096;
    for (int sample = 0; sample < sampleCount; ++sample) {
        const float red = nextUnit();
        const float green = nextUnit();
        const float blue = nextUnit();
        const std::array<float, 3> source = {red, green, blue};
        const auto hsv = ColorConversion::RGBToHSV(red, green, blue);
        const auto hsl = ColorConversion::RGBToHSL(red, green, blue);
        const auto fromHsv = ColorConversion::HSVToRGB(hsv);
        const auto fromHsl = ColorConversion::HSLToRGB(hsl);
        SCOPED_TRACE(::testing::Message()
            << "sample=" << sample << " rgb=(" << red << ',' << green << ',' << blue << ')');

        EXPECT_TRUE(std::isfinite(hsv.h));
        EXPECT_TRUE(std::isfinite(hsv.s));
        EXPECT_TRUE(std::isfinite(hsv.v));
        EXPECT_GE(hsv.h, 0.0f);
        EXPECT_LT(hsv.h, 360.0f);
        EXPECT_GE(hsv.s, 0.0f);
        EXPECT_LE(hsv.s, 1.0f);
        EXPECT_GE(hsv.v, 0.0f);
        EXPECT_LE(hsv.v, 1.0f);

        EXPECT_TRUE(std::isfinite(hsl.h));
        EXPECT_TRUE(std::isfinite(hsl.s));
        EXPECT_TRUE(std::isfinite(hsl.l));
        EXPECT_GE(hsl.h, 0.0f);
        EXPECT_LT(hsl.h, 360.0f);
        EXPECT_GE(hsl.s, 0.0f);
        EXPECT_LE(hsl.s, 1.0f);
        EXPECT_GE(hsl.l, 0.0f);
        EXPECT_LE(hsl.l, 1.0f);

        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(fromHsv[channel], source[channel], 1.0e-5f);
            EXPECT_NEAR(fromHsl[channel], source[channel], 1.0e-5f);
        }
    }
}
