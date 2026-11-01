#include <gtest/gtest.h>

#include <array>

import ColorCollection.ColorGrading;
import Image.ImageF32x4_RGBA;
import ImageProcessing.ColorTransform.TriChromaticShift;

namespace {

void expectNear(float actual, float expected)
{
    EXPECT_NEAR(actual, expected, 1.0e-5f);
}

} // namespace

TEST(ColorGradingContractTest,
     LinkedLevelsShareRangesButTransformRgbChannelsIndependently)
{
    ArtifactCore::ColorWheelsProcessor processor;
    auto& levels = processor.levels();
    levels.inputBlack = 0.2f;
    levels.inputWhite = 0.8f;
    levels.outputBlack = 0.0f;
    levels.outputWhite = 1.0f;
    levels.gamma = 1.0f;
    levels.linkRGB = true;
    processor.setGainMaster(0.0f);

    float pixels[]{0.2f, 0.5f, 0.8f, 0.37f};
    processor.process(pixels, 1, 1);

    expectNear(pixels[0], 0.0f);
    expectNear(pixels[1], 0.5f);
    expectNear(pixels[2], 1.0f);
    EXPECT_FLOAT_EQ(pixels[3], 0.37f);
}

TEST(ColorGradingContractTest,
     UnlinkedLevelsUsePerChannelInputRangesWithoutChangingAlpha)
{
    ArtifactCore::ColorWheelsProcessor processor;
    auto& levels = processor.levels();
    levels.linkRGB = false;
    levels.inputBlackR = 0.0f;
    levels.inputWhiteR = 1.0f;
    levels.inputBlackG = 0.0f;
    levels.inputWhiteG = 0.5f;
    levels.inputBlackB = 0.0f;
    levels.inputWhiteB = 1.0f;
    levels.outputBlack = 0.0f;
    levels.outputWhite = 1.0f;
    levels.gamma = 1.0f;
    processor.setGainMaster(0.0f);

    float pixels[]{0.25f, 0.25f, 0.75f, 0.62f};
    processor.process(pixels, 1, 1);

    expectNear(pixels[0], 0.25f);
    expectNear(pixels[1], 0.5f);
    expectNear(pixels[2], 0.75f);
    EXPECT_FLOAT_EQ(pixels[3], 0.62f);
}

TEST(ColorGradingContractTest, DisabledGraderLeavesRgbValuesExactlyUnchanged)
{
    ArtifactCore::ColorGrader grader;
    grader.setEnabled(false);

    float r = 0.1234567f;
    float g = 0.7654321f;
    float b = 1.25f;
    grader.processPixel(r, g, b);

    EXPECT_FLOAT_EQ(r, 0.1234567f);
    EXPECT_FLOAT_EQ(g, 0.7654321f);
    EXPECT_FLOAT_EQ(b, 1.25f);
}

TEST(ColorGradingContractTest, IntensityZeroBypassesTheCombinedGrade)
{
    ArtifactCore::ColorGrader grader;
    grader.curves().applyInvert();
    grader.setIntensity(0.0f);

    float r = 0.2f;
    float g = 0.4f;
    float b = 0.6f;
    grader.processPixel(r, g, b);

    EXPECT_FLOAT_EQ(r, 0.2f);
    EXPECT_FLOAT_EQ(g, 0.4f);
    EXPECT_FLOAT_EQ(b, 0.6f);
}

TEST(ColorGradingContractTest, IntermediateIntensityBlendsOriginalAndGradedRgb)
{
    ArtifactCore::ColorGrader grader;
    grader.curves().applyInvert();
    grader.setIntensity(0.5f);

    float r = 0.2f;
    float g = 0.4f;
    float b = 0.6f;
    grader.processPixel(r, g, b);

    expectNear(r, 0.5f);
    expectNear(g, 0.5f);
    expectNear(b, 0.5f);
}

TEST(ColorGradingContractTest, CurveProcessingPreservesAlphaAndPixelBoundaries)
{
    ArtifactCore::ColorCurves curves;
    curves.applyInvert();
    float pixels[]{0.0f, 0.5f, 1.0f, 0.2f,
                   0.25f, 0.75f, 0.5f, 0.8f};

    curves.process(pixels, 2, 1);

    expectNear(pixels[0], 1.0f);
    expectNear(pixels[1], 127.0f / 255.0f);
    expectNear(pixels[2], 0.0f);
    EXPECT_FLOAT_EQ(pixels[3], 0.2f);
    expectNear(pixels[4], 192.0f / 255.0f);
    expectNear(pixels[5], 64.0f / 255.0f);
    expectNear(pixels[6], 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(pixels[7], 0.8f);
}

TEST(ColorGradingContractTest, CurvesClampOutOfRangeRgbBeforeLutLookupAndPreserveAlpha)
{
    ArtifactCore::ColorCurves curves;
    float pixels[]{-0.25f, 0.5f, 1.5f, 0.17f,
                   1.25f, -0.5f, 0.25f, 0.83f};

    curves.process(pixels, 2, 1);

    EXPECT_FLOAT_EQ(pixels[0], 0.0f);
    expectNear(pixels[1], 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(pixels[2], 1.0f);
    EXPECT_FLOAT_EQ(pixels[3], 0.17f);
    EXPECT_FLOAT_EQ(pixels[4], 1.0f);
    EXPECT_FLOAT_EQ(pixels[5], 0.0f);
    expectNear(pixels[6], 63.0f / 255.0f);
    EXPECT_FLOAT_EQ(pixels[7], 0.83f);
}

TEST(TriChromaticShiftContractTest, MapsGrayInputToConfiguredTriadColor)
{
    ArtifactCore::TriChromaticProcessor processor;
    ArtifactCore::TriChromaticSettings settings;
    settings.triad.shadowHue = 0.0;
    settings.triad.shadowSat = 1.0;
    settings.triad.shadowLum = 0.5;
    settings.triad.midtoneHue = 0.0;
    settings.triad.midtoneSat = 1.0;
    settings.triad.midtoneLum = 0.5;
    settings.triad.highlightHue = 0.0;
    settings.triad.highlightSat = 1.0;
    settings.triad.highlightLum = 0.5;
    settings.colorMix = 1.0;
    settings.masterStrength = 1.0;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    float red = 0.2f;
    float green = 0.2f;
    float blue = 0.2f;
    processor.applyPixel(red, green, blue);

    EXPECT_FLOAT_EQ(red, 1.0f);
    EXPECT_FLOAT_EQ(green, 0.0f);
    EXPECT_FLOAT_EQ(blue, 0.0f);
}

TEST(TriChromaticShiftContractTest, ImageApplyUsesRgbaChannelsAndPreservesAlpha)
{
    ArtifactCore::TriChromaticProcessor processor;
    ArtifactCore::TriChromaticSettings settings;
    settings.triad.shadowHue = 0.0;
    settings.triad.shadowSat = 1.0;
    settings.triad.shadowLum = 0.5;
    settings.triad.midtoneHue = 0.0;
    settings.triad.midtoneSat = 1.0;
    settings.triad.midtoneLum = 0.5;
    settings.triad.highlightHue = 0.0;
    settings.triad.highlightSat = 1.0;
    settings.triad.highlightLum = 0.5;
    settings.colorMix = 1.0;
    settings.masterStrength = 1.0;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    constexpr std::array<float, 4> source{0.2f, 0.2f, 0.2f, 0.37f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(source.data(), 1, 1);
    processor.apply(image);

    const float* pixel = image.rgba32fData();
    ASSERT_NE(pixel, nullptr);
    EXPECT_FLOAT_EQ(pixel[0], 0.0f);
    EXPECT_FLOAT_EQ(pixel[1], 0.0f);
    EXPECT_FLOAT_EQ(pixel[2], 1.0f);
    EXPECT_FLOAT_EQ(pixel[3], 0.37f);
}

TEST(TriChromaticShiftContractTest, ZeroMasterStrengthLeavesRawBufferExactlyUnchanged)
{
    ArtifactCore::TriChromaticProcessor processor;
    ArtifactCore::TriChromaticSettings settings;
    settings.masterStrength = 0.0;
    processor.setSettings(settings);

    float4 pixel{0.23f, 0.41f, 0.67f, 0.89f};
    processor.apply(&pixel, 1, 1);

    EXPECT_FLOAT_EQ(pixel.x, 0.23f);
    EXPECT_FLOAT_EQ(pixel.y, 0.41f);
    EXPECT_FLOAT_EQ(pixel.z, 0.67f);
    EXPECT_FLOAT_EQ(pixel.w, 0.89f);
}
