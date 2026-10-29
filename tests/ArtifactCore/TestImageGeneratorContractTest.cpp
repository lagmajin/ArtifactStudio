#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

import GenerateTestImage;

using ArtifactCore::TestImageGenerator;

namespace {
auto pixel(const std::vector<float>& pixels, int width, int x, int y)
    -> std::array<float, 4>
{
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

void expectPixel(const std::vector<float>& pixels, int width, int x, int y,
                 float r, float g, float b, float a = 1.0f)
{
    const auto actual = pixel(pixels, width, x, y);
    EXPECT_FLOAT_EQ(actual[0], r);
    EXPECT_FLOAT_EQ(actual[1], g);
    EXPECT_FLOAT_EQ(actual[2], b);
    EXPECT_FLOAT_EQ(actual[3], a);
}

auto makeBuffer(int width, int height) -> std::vector<float>
{
    return std::vector<float>(static_cast<std::size_t>(width * height * 4), -1.0f);
}
}

TEST(TestImageGeneratorContractTest, ColorBarsUseSevenOrderedSeventyFivePercentColors)
{
    auto pixels = makeBuffer(7, 1);
    TestImageGenerator::colorBars(pixels.data(), 7, 1);
    constexpr std::array<std::array<float, 3>, 7> expected = {{
        {{0.75f, 0.75f, 0.75f}}, {{0.75f, 0.75f, 0.0f}}, {{0.0f, 0.75f, 0.75f}},
        {{0.0f, 0.75f, 0.0f}}, {{0.75f, 0.0f, 0.75f}}, {{0.75f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.75f}},
    }};
    for (int x = 0; x < 7; ++x) {
        const auto color = expected[static_cast<std::size_t>(x)];
        expectPixel(pixels, 7, x, 0, color[0], color[1], color[2]);
    }
}

TEST(TestImageGeneratorContractTest, FullLevelColorBarsUseOpaquePrimaryAndSecondaryColors)
{
    auto pixels = makeBuffer(7, 1);
    TestImageGenerator::colorBars100(pixels.data(), 7, 1);
    expectPixel(pixels, 7, 0, 0, 1.0f, 1.0f, 1.0f);
    expectPixel(pixels, 7, 1, 0, 1.0f, 1.0f, 0.0f);
    expectPixel(pixels, 7, 2, 0, 0.0f, 1.0f, 1.0f);
    expectPixel(pixels, 7, 6, 0, 0.0f, 0.0f, 1.0f);
}

TEST(TestImageGeneratorContractTest, SmpteHdBarsSwitchFromTopToMiddleAndPlugeBands)
{
    auto pixels = makeBuffer(7, 12);
    TestImageGenerator::smpteHdBars(pixels.data(), 7, 12);

    expectPixel(pixels, 7, 0, 0, 0.75f, 0.75f, 0.75f);
    expectPixel(pixels, 7, 0, 9, 0.0f, 0.0f, 0.75f);
    expectPixel(pixels, 7, 0, 10, 0.0f, 0.0f, 0.035f);
    expectPixel(pixels, 7, 5, 10, 0.75f, 0.75f, 0.75f);
    expectPixel(pixels, 7, 6, 11, 0.0f, 0.0f, 0.0f);
}

TEST(TestImageGeneratorContractTest, HorizontalAndVerticalGradientsReachBothEndpoints)
{
    auto horizontal = makeBuffer(3, 2);
    TestImageGenerator::horizontalGradient(horizontal.data(), 3, 2);
    expectPixel(horizontal, 3, 0, 1, 0.0f, 0.0f, 0.0f);
    expectPixel(horizontal, 3, 1, 0, 0.5f, 0.5f, 0.5f);
    expectPixel(horizontal, 3, 2, 1, 1.0f, 1.0f, 1.0f);

    auto vertical = makeBuffer(2, 3);
    TestImageGenerator::verticalGradient(vertical.data(), 2, 3);
    expectPixel(vertical, 2, 1, 0, 0.0f, 0.0f, 0.0f);
    expectPixel(vertical, 2, 0, 1, 0.5f, 0.5f, 0.5f);
    expectPixel(vertical, 2, 1, 2, 1.0f, 1.0f, 1.0f);
}

TEST(TestImageGeneratorContractTest, RgbGradientDividesColumnsAndBrightensTowardTop)
{
    auto pixels = makeBuffer(6, 2);
    TestImageGenerator::rgbGradient(pixels.data(), 6, 2);
    expectPixel(pixels, 6, 0, 0, 1.0f, 0.0f, 0.0f);
    expectPixel(pixels, 6, 2, 1, 0.0f, 0.0f, 0.0f);
    expectPixel(pixels, 6, 3, 0, 0.0f, 1.0f, 0.0f);
    expectPixel(pixels, 6, 5, 0, 0.0f, 0.0f, 1.0f);
}

TEST(TestImageGeneratorContractTest, CheckerboardClampsCellSizeAndAlternatesCells)
{
    auto pixels = makeBuffer(4, 2);
    TestImageGenerator::checkerboard(pixels.data(), 4, 2, 0);
    expectPixel(pixels, 4, 0, 0, 0.8f, 0.8f, 0.8f);
    expectPixel(pixels, 4, 1, 0, 0.2f, 0.2f, 0.2f);
    expectPixel(pixels, 4, 0, 1, 0.2f, 0.2f, 0.2f);
    expectPixel(pixels, 4, 1, 1, 0.8f, 0.8f, 0.8f);
}

TEST(TestImageGeneratorContractTest, ZoneSystemPartitionsWidthIntoElevenOrderedLevels)
{
    auto pixels = makeBuffer(11, 1);
    TestImageGenerator::zoneSystem(pixels.data(), 11, 1);
    for (int x = 0; x < 11; ++x) {
        const float expected = static_cast<float>(x) / 10.0f;
        expectPixel(pixels, 11, x, 0, expected, expected, expected);
    }
}

TEST(TestImageGeneratorContractTest, SolidColorWritesEveryRgbaComponent)
{
    auto pixels = makeBuffer(2, 2);
    TestImageGenerator::solidColor(pixels.data(), 2, 2, 0.125f, 0.5f, 0.875f, 0.25f);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            expectPixel(pixels, 2, x, y, 0.125f, 0.5f, 0.875f, 0.25f);
        }
    }
}

TEST(TestImageGeneratorContractTest, RadialGradientIsBrightAtCenterAndDarkensAtCorners)
{
    auto pixels = makeBuffer(3, 3);
    TestImageGenerator::radialGradient(pixels.data(), 3, 3);
    EXPECT_GT(pixel(pixels, 3, 1, 1)[0], pixel(pixels, 3, 0, 0)[0]);
    EXPECT_NEAR(pixel(pixels, 3, 1, 1)[0],
                1.0f - std::sqrt(0.5f) / std::sqrt(4.5f), 1e-6f);
    EXPECT_FLOAT_EQ(pixel(pixels, 3, 0, 0)[0], 0.0f);
    EXPECT_FLOAT_EQ(pixel(pixels, 3, 0, 0)[3], 1.0f);
}

TEST(TestImageGeneratorContractTest, ColorWheelHasWhiteCenterSaturatedHueAndDarkOutside)
{
    auto pixels = makeBuffer(4, 4);
    TestImageGenerator::colorWheel(pixels.data(), 4, 4);
    expectPixel(pixels, 4, 2, 2, 1.0f, 1.0f, 1.0f);
    expectPixel(pixels, 4, 3, 2, 1.0f, 0.5f, 0.5f);
    expectPixel(pixels, 4, 0, 0, 0.1f, 0.1f, 0.1f);
}

TEST(TestImageGeneratorContractTest, NullOrInvalidSmpteInputIsIgnored)
{
    std::vector<float> pixels(4, 13.0f);
    TestImageGenerator::smpteHdBars(nullptr, 1, 1);
    TestImageGenerator::smpteHdBars(pixels.data(), 0, 1);
    TestImageGenerator::smpteHdBars(pixels.data(), 1, 0);
    EXPECT_EQ(pixels, (std::vector<float>{13.0f, 13.0f, 13.0f, 13.0f}));
}
