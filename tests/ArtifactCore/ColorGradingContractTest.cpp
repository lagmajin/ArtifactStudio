#include <gtest/gtest.h>

import ColorCollection.ColorGrading;

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

TEST(ColorGradingContractTest, CurveProcessingPreservesAlphaAndPixelBoundaries)
{
    ArtifactCore::ColorCurves curves;
    curves.applyInvert();
    float pixels[]{0.0f, 0.5f, 1.0f, 0.2f,
                   0.25f, 0.75f, 0.5f, 0.8f};

    curves.process(pixels, 2, 1);

    expectNear(pixels[0], 1.0f);
    expectNear(pixels[1], 128.0f / 255.0f);
    expectNear(pixels[2], 0.0f);
    EXPECT_FLOAT_EQ(pixels[3], 0.2f);
    expectNear(pixels[4], 192.0f / 255.0f);
    expectNear(pixels[5], 64.0f / 255.0f);
    expectNear(pixels[6], 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(pixels[7], 0.8f);
}
