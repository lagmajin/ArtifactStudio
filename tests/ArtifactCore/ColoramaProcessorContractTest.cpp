#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include <QImage>

import ImageProcessing.ColorTransform.Colorama;

using namespace ArtifactCore;

TEST(ColoramaProcessorContractTest, LumaModeSamplesRainbowEndpointAndWrapsPhase)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Rainbow;
    settings.phase = 0.0f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 1.0f, 1.0e-6f);
    EXPECT_NEAR(g, 0.2f, 1.0e-6f);
    EXPECT_NEAR(b, 0.2f, 1.0e-6f);

    settings.phase = 1.0f;
    processor.setSettings(settings);
    r = g = b = 0.0f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 1.0f, 1.0e-6f);
    EXPECT_NEAR(g, 0.2f, 1.0e-6f);
    EXPECT_NEAR(b, 0.2f, 1.0e-6f);
}

TEST(ColoramaProcessorContractTest, HueModeSelectsPaletteFromSourceHue)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Hue;
    settings.palette = ColoramaPalette::Rainbow;
    settings.phase = 0.25f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    float r = 1.0f;
    float g = 0.0f;
    float b = 0.0f;
    processor.applyPixel(r, g, b);

    EXPECT_NEAR(r, 1.0f, 1.0e-6f);
    EXPECT_NEAR(g, 0.8f, 1.0e-6f);
    EXPECT_NEAR(b, 0.2f, 1.0e-6f);
}

TEST(ColoramaProcessorContractTest, ZeroStrengthPreservesSourceAndPreserveLumaKeepsLuma)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.strength = 0.0f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    float r = 0.2f;
    float g = 0.4f;
    float b = 0.6f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 0.2f, 1.0e-6f);
    EXPECT_NEAR(g, 0.4f, 1.0e-6f);
    EXPECT_NEAR(b, 0.6f, 1.0e-6f);

    settings.strength = 1.0f;
    settings.phase = 0.1f;
    settings.preserveLuma = true;
    processor.setSettings(settings);
    r = 0.25f;
    g = 0.45f;
    b = 0.65f;
    const float originalLuma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    processor.applyPixel(r, g, b);
    const float outputLuma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    EXPECT_NEAR(outputLuma, originalLuma, 1.0e-5f);
}

TEST(ColoramaProcessorContractTest, EachPaletteMapsItsFirstRampColor)
{
    struct PaletteCase {
        ColoramaPalette palette;
        std::array<float, 3> expected;
    };
    constexpr PaletteCase cases[] = {
        {ColoramaPalette::Fire, {0.10f, 0.00f, 0.00f}},
        {ColoramaPalette::Ocean, {0.03f, 0.05f, 0.18f}},
        {ColoramaPalette::Neon, {0.00f, 0.95f, 0.75f}},
        {ColoramaPalette::Sunset, {0.05f, 0.04f, 0.16f}},
    };

    ColoramaProcessor processor;
    for (const auto& testCase : cases) {
        ColoramaSettings settings;
        settings.sourceMode = ColoramaSourceMode::Luma;
        settings.palette = testCase.palette;
        settings.phase = 0.0f;
        settings.preserveLuma = false;
        processor.setSettings(settings);

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        processor.applyPixel(r, g, b);
        EXPECT_NEAR(r, testCase.expected[0], 1.0e-6f);
        EXPECT_NEAR(g, testCase.expected[1], 1.0e-6f);
        EXPECT_NEAR(b, testCase.expected[2], 1.0e-6f);
    }
}

TEST(ColoramaProcessorContractTest, FirePaletteInterpolatesAndAppliesContrastAndSaturation)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.preserveLuma = false;
    settings.phase = 0.0f;

    settings.contrast = 1.0f;
    settings.saturationBoost = 1.0f;
    processor.setSettings(settings);
    float r = 0.125f;
    float g = 0.125f;
    float b = 0.125f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 0.325f, 1.0e-6f);
    EXPECT_NEAR(g, 0.05f, 1.0e-6f);
    EXPECT_NEAR(b, 0.0f, 1.0e-6f);

    settings.contrast = 2.0f;
    settings.saturationBoost = 1.0f;
    processor.setSettings(settings);
    r = g = b = 0.0f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 0.0f, 1.0e-6f);
    EXPECT_NEAR(g, 0.0f, 1.0e-6f);
    EXPECT_NEAR(b, 0.0f, 1.0e-6f);

    settings.contrast = 1.0f;
    settings.saturationBoost = 0.0f;
    processor.setSettings(settings);
    r = g = b = 0.0f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 0.05f, 1.0e-6f);
    EXPECT_NEAR(g, 0.05f, 1.0e-6f);
    EXPECT_NEAR(b, 0.05f, 1.0e-6f);
}

TEST(ColoramaProcessorContractTest, ImageApplyPreservesAlphaAndDoesNotModifySource)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(0, 0, 0, 77));
    const QImage result = processor.apply(source);

    EXPECT_EQ(result.format(), QImage::Format_ARGB32);
    EXPECT_NEAR(qRed(result.pixel(0, 0)), 25, 1);
    EXPECT_EQ(qGreen(result.pixel(0, 0)), 0);
    EXPECT_EQ(qBlue(result.pixel(0, 0)), 0);
    EXPECT_EQ(qAlpha(result.pixel(0, 0)), 77);
    EXPECT_EQ(source.pixel(0, 0), qRgba(0, 0, 0, 77));
}

TEST(ColoramaProcessorContractTest, ZeroStrengthImageApplyReturnsInputFormatAndPixels)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.strength = 0.0f;
    processor.setSettings(settings);

    QImage source(1, 1, QImage::Format_RGB32);
    source.setPixel(0, 0, qRgb(21, 43, 65));
    const QImage result = processor.apply(source);

    EXPECT_EQ(result.format(), source.format());
    EXPECT_EQ(result, source);
}

TEST(ColoramaProcessorContractTest, PresetFactoriesAndResetRestoreDocumentedSettings)
{
    const auto rainbow = ColoramaSettings::rainbow();
    EXPECT_EQ(rainbow.palette, ColoramaPalette::Rainbow);
    EXPECT_EQ(rainbow.sourceMode, ColoramaSourceMode::Luma);
    EXPECT_FLOAT_EQ(rainbow.phase, 0.0f);
    EXPECT_FLOAT_EQ(rainbow.strength, 1.0f);
    EXPECT_TRUE(rainbow.preserveLuma);

    const auto fire = ColoramaSettings::fire();
    EXPECT_EQ(fire.palette, ColoramaPalette::Fire);
    EXPECT_FLOAT_EQ(fire.saturationBoost, 1.15f);
    EXPECT_FLOAT_EQ(fire.contrast, 1.1f);

    const auto ocean = ColoramaSettings::ocean();
    EXPECT_EQ(ocean.palette, ColoramaPalette::Ocean);
    EXPECT_FLOAT_EQ(ocean.saturationBoost, 1.05f);
    EXPECT_FLOAT_EQ(ocean.contrast, 0.95f);

    ColoramaSettings changed;
    changed.palette = ColoramaPalette::Neon;
    changed.sourceMode = ColoramaSourceMode::Hue;
    changed.phase = 0.7f;
    changed.spread = 0.3f;
    changed.strength = 0.4f;
    changed.saturationBoost = 1.8f;
    changed.contrast = 2.0f;
    changed.preserveLuma = false;
    changed.reset();
    EXPECT_EQ(changed.palette, ColoramaPalette::Rainbow);
    EXPECT_EQ(changed.sourceMode, ColoramaSourceMode::Luma);
    EXPECT_FLOAT_EQ(changed.phase, 0.0f);
    EXPECT_FLOAT_EQ(changed.spread, 1.0f);
    EXPECT_FLOAT_EQ(changed.strength, 1.0f);
    EXPECT_FLOAT_EQ(changed.saturationBoost, 1.0f);
    EXPECT_FLOAT_EQ(changed.contrast, 1.0f);
    EXPECT_TRUE(changed.preserveLuma);
}

TEST(ColoramaProcessorContractTest, NegativePhaseWrapsAndSpreadScalesPaletteKey)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Rainbow;
    settings.phase = -0.25f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 0.2f, 1.0e-6f);
    EXPECT_NEAR(g, 0.7f, 1.0e-6f);
    EXPECT_NEAR(b, 1.0f, 1.0e-6f);

    settings.phase = 0.0f;
    settings.spread = 0.5f;
    processor.setSettings(settings);
    r = g = b = 0.5f;
    processor.applyPixel(r, g, b);
    EXPECT_NEAR(r, 1.0f, 1.0e-6f);
    EXPECT_NEAR(g, 0.8f, 1.0e-6f);
    EXPECT_NEAR(b, 0.2f, 1.0e-6f);
}

TEST(ColoramaProcessorContractTest, HueModeMapsAchromaticInputFromZeroHue)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Hue;
    settings.palette = ColoramaPalette::Rainbow;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    float r = 0.5f;
    float g = 0.5f;
    float b = 0.5f;
    processor.applyPixel(r, g, b);

    EXPECT_NEAR(r, 1.0f, 1.0e-6f);
    EXPECT_NEAR(g, 0.2f, 1.0e-6f);
    EXPECT_NEAR(b, 0.2f, 1.0e-6f);
}
