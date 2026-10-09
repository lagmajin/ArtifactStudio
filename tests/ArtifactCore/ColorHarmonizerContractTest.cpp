#include <gtest/gtest.h>

#include <cmath>

import Color.Conversion;
import Color.Float;
import Color.Harmonizer;

using namespace ArtifactCore;

namespace {

float hueOf(const FloatColor& color)
{
    return ColorConversion::RGBToHSV(color.r(), color.g(), color.b()).h;
}

float hueDistance(float first, float second)
{
    float distance = std::fmod(std::fabs(first - second), 360.0f);
    return distance > 180.0f ? 360.0f - distance : distance;
}

void expectHueNear(const FloatColor& color, float expected, float tolerance = 1e-3f)
{
    EXPECT_NEAR(hueDistance(hueOf(color), expected), 0.0f, tolerance);
}

} // namespace

TEST(ColorHarmonizerContractTest, ComplementaryColorRotatesHueBy180AndKeepsAlpha)
{
    const FloatColor input(1.0f, 0.0f, 0.0f, 0.37f);
    const FloatColor result = ColorHarmonizer::getComplementary(input);

    expectHueNear(result, 180.0f);
    EXPECT_FLOAT_EQ(result.a(), input.a());
}

TEST(ColorHarmonizerContractTest, AnalogousColorsAreSymmetricAroundBaseHue)
{
    const FloatColor input(1.0f, 0.0f, 0.0f, 0.6f);
    const auto colors = ColorHarmonizer::getAnalogous(input, 25.0f);

    ASSERT_EQ(colors.size(), 2);
    expectHueNear(colors[0], 25.0f);
    expectHueNear(colors[1], 335.0f);
    EXPECT_FLOAT_EQ(colors[0].a(), input.a());
    EXPECT_FLOAT_EQ(colors[1].a(), input.a());
}

TEST(ColorHarmonizerContractTest, TriadicAndTetradicColorsUseExpectedHueIntervals)
{
    const FloatColor input(1.0f, 0.0f, 0.0f, 1.0f);
    const auto triadic = ColorHarmonizer::getTriadic(input);
    const auto tetradic = ColorHarmonizer::getTetradic(input);

    ASSERT_EQ(triadic.size(), 2);
    expectHueNear(triadic[0], 120.0f);
    expectHueNear(triadic[1], 240.0f);
    ASSERT_EQ(tetradic.size(), 3);
    expectHueNear(tetradic[0], 90.0f);
    expectHueNear(tetradic[1], 180.0f);
    expectHueNear(tetradic[2], 270.0f);
}

TEST(ColorHarmonizerContractTest, SplitComplementaryColorsStraddleTheComplement)
{
    const auto colors = ColorHarmonizer::getSplitComplementary(
        FloatColor(1.0f, 0.0f, 0.0f, 0.8f), 30.0f);

    ASSERT_EQ(colors.size(), 2);
    expectHueNear(colors[0], 150.0f);
    expectHueNear(colors[1], 210.0f);
    EXPECT_FLOAT_EQ(colors[0].a(), 0.8f);
    EXPECT_FLOAT_EQ(colors[1].a(), 0.8f);
}

TEST(ColorHarmonizerContractTest, MonochromaticReturnsRequestedCountWithStableHueAndAlpha)
{
    const FloatColor input(0.8f, 0.2f, 0.1f, 0.45f);
    const auto colors = ColorHarmonizer::getMonochromatic(input, 3);
    const float inputHue = hueOf(input);

    ASSERT_EQ(colors.size(), 3);
    for (const auto& color : colors) {
        expectHueNear(color, inputHue);
        EXPECT_FLOAT_EQ(color.a(), input.a());
    }
    EXPECT_TRUE(ColorHarmonizer::getMonochromatic(input, 0).isEmpty());
    EXPECT_TRUE(ColorHarmonizer::getMonochromatic(input, -3).isEmpty());
}

TEST(ColorHarmonizerContractTest, MonochromaticValueCycleMatchesCountAcrossSourceGrid)
{
    constexpr float hues[] = {0.0f, 47.0f, 181.0f, 359.0f};
    constexpr float saturations[] = {0.0f, 0.35f, 1.0f};
    constexpr float values[] = {0.0f, 0.2f, 0.75f, 1.0f};
    constexpr float alpha = 0.62f;

    for (const float hue : hues) {
        for (const float saturation : saturations) {
            for (const float value : values) {
                const auto rgb = ColorConversion::HSVToRGB({hue, saturation, value});
                const FloatColor input(rgb[0], rgb[1], rgb[2], alpha);
                const auto inputHsv = ColorConversion::RGBToHSV(
                    input.r(), input.g(), input.b());
                for (int count = 1; count <= 12; ++count) {
                    const auto colors = ColorHarmonizer::getMonochromatic(input, count);
                    ASSERT_EQ(colors.size(), count);
                    const float step = 1.0f / (count + 1.0f);
                    for (int index = 0; index < count; ++index) {
                        const auto hsv = ColorConversion::RGBToHSV(
                            colors[index].r(), colors[index].g(), colors[index].b());
                        const float expectedValue = std::fmod(
                            inputHsv.v + step * (index + 1), 1.0f);
                        SCOPED_TRACE(::testing::Message()
                            << "hue=" << hue << " saturation=" << saturation
                            << " value=" << value << " count=" << count
                            << " index=" << index);
                        EXPECT_NEAR(hsv.v, expectedValue, 2.0e-5f);
                        if (expectedValue <= 1.0e-6f) {
                            EXPECT_FLOAT_EQ(hsv.s, 0.0f);
                        } else {
                            EXPECT_NEAR(hueDistance(hsv.h, inputHsv.h), 0.0f, 2.0e-3f);
                            EXPECT_NEAR(hsv.s, inputHsv.s, 2.0e-5f);
                        }
                        EXPECT_FLOAT_EQ(colors[index].a(), alpha);
                    }
                }
            }
        }
    }
}

TEST(ColorHarmonizerContractTest, HueShiftSchemesPreserveSaturationValueAndAlpha)
{
    constexpr float sourceHue = 350.0f;
    constexpr float sourceSaturation = 0.72f;
    constexpr float sourceValue = 0.63f;
    constexpr float sourceAlpha = 0.38f;
    const auto rgb = ColorConversion::HSVToRGB(
        {sourceHue, sourceSaturation, sourceValue});
    const FloatColor source(rgb[0], rgb[1], rgb[2], sourceAlpha);

    const auto expectPreservedChannels = [&](const FloatColor& result) {
        const auto hsv = ColorConversion::RGBToHSV(result.r(), result.g(), result.b());
        EXPECT_NEAR(hsv.s, sourceSaturation, 1.0e-5f);
        EXPECT_NEAR(hsv.v, sourceValue, 1.0e-5f);
        EXPECT_FLOAT_EQ(result.a(), sourceAlpha);
    };

    const FloatColor complementary = ColorHarmonizer::getComplementary(source);
    expectHueNear(complementary, 170.0f);
    expectPreservedChannels(complementary);

    const auto analogous = ColorHarmonizer::getAnalogous(source, 25.0f);
    ASSERT_EQ(analogous.size(), 2);
    expectHueNear(analogous[0], 15.0f);
    expectHueNear(analogous[1], 325.0f);
    expectPreservedChannels(analogous[0]);
    expectPreservedChannels(analogous[1]);

    const auto triadic = ColorHarmonizer::getTriadic(source);
    expectHueNear(triadic[0], 110.0f);
    expectHueNear(triadic[1], 230.0f);
    expectPreservedChannels(triadic[0]);
    expectPreservedChannels(triadic[1]);

    const auto split = ColorHarmonizer::getSplitComplementary(source, 30.0f);
    expectHueNear(split[0], 140.0f);
    expectHueNear(split[1], 200.0f);
    expectPreservedChannels(split[0]);
    expectPreservedChannels(split[1]);

    const auto tetradic = ColorHarmonizer::getTetradic(source);
    expectHueNear(tetradic[0], 80.0f);
    expectHueNear(tetradic[1], 170.0f);
    expectHueNear(tetradic[2], 260.0f);
    expectPreservedChannels(tetradic[0]);
    expectPreservedChannels(tetradic[1]);
    expectPreservedChannels(tetradic[2]);
}

TEST(ColorHarmonizerContractTest, HueSchemesKeepBlackAndWhiteAchromatic)
{
    constexpr float alpha = 0.42f;
    const FloatColor black(0.0f, 0.0f, 0.0f, alpha);
    const FloatColor white(1.0f, 1.0f, 1.0f, alpha);

    const FloatColor blackComplement = ColorHarmonizer::getComplementary(black);
    const FloatColor whiteComplement = ColorHarmonizer::getComplementary(white);
    EXPECT_FLOAT_EQ(blackComplement.r(), 0.0f);
    EXPECT_FLOAT_EQ(blackComplement.g(), 0.0f);
    EXPECT_FLOAT_EQ(blackComplement.b(), 0.0f);
    EXPECT_FLOAT_EQ(whiteComplement.r(), 1.0f);
    EXPECT_FLOAT_EQ(whiteComplement.g(), 1.0f);
    EXPECT_FLOAT_EQ(whiteComplement.b(), 1.0f);
    EXPECT_FLOAT_EQ(blackComplement.a(), alpha);
    EXPECT_FLOAT_EQ(whiteComplement.a(), alpha);

    for (const FloatColor input : {black, white}) {
        const auto analogous = ColorHarmonizer::getAnalogous(input, 47.0f);
        const auto triadic = ColorHarmonizer::getTriadic(input);
        const auto split = ColorHarmonizer::getSplitComplementary(input, 35.0f);
        const auto tetradic = ColorHarmonizer::getTetradic(input);
        for (const FloatColor& result : analogous) {
            EXPECT_FLOAT_EQ(result.r(), result.g());
            EXPECT_FLOAT_EQ(result.g(), result.b());
            EXPECT_FLOAT_EQ(result.a(), alpha);
        }
        for (const auto* colors : {&triadic, &split, &tetradic}) {
            for (const FloatColor& result : *colors) {
                EXPECT_FLOAT_EQ(result.r(), result.g());
                EXPECT_FLOAT_EQ(result.g(), result.b());
                EXPECT_FLOAT_EQ(result.a(), alpha);
            }
        }
    }
}

TEST(ColorHarmonizerContractTest, AnalogousAngleWrapsAcrossMultipleTurns)
{
    const auto rgb = ColorConversion::HSVToRGB({30.0f, 0.8f, 0.7f});
    const FloatColor input(rgb[0], rgb[1], rgb[2], 0.55f);
    const auto colors = ColorHarmonizer::getAnalogous(input, -400.0f);

    ASSERT_EQ(colors.size(), 2);
    expectHueNear(colors[0], 350.0f);
    expectHueNear(colors[1], 70.0f);
    EXPECT_FLOAT_EQ(colors[0].a(), input.a());
    EXPECT_FLOAT_EQ(colors[1].a(), input.a());
}

TEST(ColorHarmonizerContractTest, HueSchemesPreserveRgbPropertiesAcrossHueAndValueGrid)
{
    constexpr float saturations[] = {0.15f, 0.6f, 1.0f};
    constexpr float values[] = {0.05f, 0.5f, 1.0f};
    constexpr float alpha = 0.43f;
    constexpr float analogousAngle = 37.5f;
    constexpr float splitOffset = 37.5f;

    const auto expectShiftedColor = [&](const FloatColor& input,
                                        const FloatColor& output,
                                        const float expectedHue) {
        const auto inputHsv = ColorConversion::RGBToHSV(
            input.r(), input.g(), input.b());
        const auto outputHsv = ColorConversion::RGBToHSV(
            output.r(), output.g(), output.b());
        EXPECT_TRUE(std::isfinite(output.r()));
        EXPECT_TRUE(std::isfinite(output.g()));
        EXPECT_TRUE(std::isfinite(output.b()));
        expectHueNear(output, expectedHue, 2.0e-3f);
        EXPECT_NEAR(outputHsv.s, inputHsv.s, 2.0e-5f);
        EXPECT_NEAR(outputHsv.v, inputHsv.v, 2.0e-5f);
        EXPECT_FLOAT_EQ(output.a(), input.a());
    };

    for (int hue = 0; hue < 360; hue += 5) {
        for (const float saturation : saturations) {
            for (const float value : values) {
                const auto rgb = ColorConversion::HSVToRGB(
                    {static_cast<float>(hue), saturation, value});
                const FloatColor input(rgb[0], rgb[1], rgb[2], alpha);
                SCOPED_TRACE(::testing::Message()
                    << "hue=" << hue << " saturation=" << saturation
                    << " value=" << value);

                expectShiftedColor(input, ColorHarmonizer::getComplementary(input),
                                   static_cast<float>(hue + 180));

                const auto analogous = ColorHarmonizer::getAnalogous(
                    input, analogousAngle);
                ASSERT_EQ(analogous.size(), 2);
                expectShiftedColor(input, analogous[0], hue + analogousAngle);
                expectShiftedColor(input, analogous[1], hue - analogousAngle);

                const auto triadic = ColorHarmonizer::getTriadic(input);
                ASSERT_EQ(triadic.size(), 2);
                expectShiftedColor(input, triadic[0], hue + 120.0f);
                expectShiftedColor(input, triadic[1], hue + 240.0f);

                const auto split = ColorHarmonizer::getSplitComplementary(
                    input, splitOffset);
                ASSERT_EQ(split.size(), 2);
                expectShiftedColor(input, split[0], hue + 180.0f - splitOffset);
                expectShiftedColor(input, split[1], hue + 180.0f + splitOffset);

                const auto tetradic = ColorHarmonizer::getTetradic(input);
                ASSERT_EQ(tetradic.size(), 3);
                expectShiftedColor(input, tetradic[0], hue + 90.0f);
                expectShiftedColor(input, tetradic[1], hue + 180.0f);
                expectShiftedColor(input, tetradic[2], hue + 270.0f);
            }
        }
    }
}
