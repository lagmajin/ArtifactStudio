#include <gtest/gtest.h>

#include <array>
#include <cstddef>
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
