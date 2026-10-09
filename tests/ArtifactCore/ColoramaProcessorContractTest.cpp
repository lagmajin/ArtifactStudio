#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>

#include <QColor>
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

TEST(ColoramaProcessorContractTest, EveryPaletteMatchesIndependentRampInterpolationGrid)
{
    struct PaletteReference {
        ColoramaPalette palette;
        std::array<std::array<float, 3>, 5> colors;
    };
    constexpr PaletteReference palettes[] = {
        {ColoramaPalette::Rainbow, {{{1.00f, 0.20f, 0.20f}, {1.00f, 0.80f, 0.20f},
                                      {0.20f, 1.00f, 0.35f}, {0.20f, 0.70f, 1.00f},
                                      {0.85f, 0.20f, 1.00f}}}},
        {ColoramaPalette::Fire, {{{0.10f, 0.00f, 0.00f}, {0.55f, 0.10f, 0.00f},
                                   {0.90f, 0.35f, 0.00f}, {1.00f, 0.74f, 0.20f},
                                   {1.00f, 0.95f, 0.75f}}}},
        {ColoramaPalette::Ocean, {{{0.03f, 0.05f, 0.18f}, {0.00f, 0.30f, 0.45f},
                                    {0.10f, 0.65f, 0.75f}, {0.30f, 0.85f, 0.95f},
                                    {0.80f, 0.98f, 1.00f}}}},
        {ColoramaPalette::Neon, {{{0.00f, 0.95f, 0.75f}, {0.85f, 0.10f, 1.00f},
                                   {0.10f, 0.90f, 1.00f}, {1.00f, 0.15f, 0.45f},
                                   {0.90f, 1.00f, 0.20f}}}},
        {ColoramaPalette::Sunset, {{{0.05f, 0.04f, 0.16f}, {0.35f, 0.09f, 0.40f},
                                     {0.80f, 0.20f, 0.30f}, {0.98f, 0.45f, 0.12f},
                                     {1.00f, 0.86f, 0.58f}}}},
    };
    constexpr float keys[] = {
        0.0f, 0.0625f, 0.125f, 0.1875f, 0.25f,
        0.3125f, 0.375f, 0.4375f, 0.5f, 0.5625f,
        0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f,
        0.9375f,
    };
    ColoramaProcessor processor;
    for (const PaletteReference& reference : palettes) {
        ColoramaSettings settings;
        settings.sourceMode = ColoramaSourceMode::Luma;
        settings.palette = reference.palette;
        settings.phase = 0.0f;
        settings.spread = 1.0f;
        settings.strength = 1.0f;
        settings.saturationBoost = 1.0f;
        settings.contrast = 1.0f;
        settings.preserveLuma = false;
        processor.setSettings(settings);

        for (const float key : keys) {
            const float scaled = key * 4.0f;
            const int segment = std::min(static_cast<int>(scaled), 3);
            const float amount = scaled - segment;
            float red = key;
            float green = key;
            float blue = key;
            processor.applyPixel(red, green, blue);

            SCOPED_TRACE(::testing::Message()
                << "palette=" << static_cast<int>(reference.palette)
                << " key=" << key);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float expected = reference.colors[segment][channel]
                    * (1.0f - amount) + reference.colors[segment + 1][channel] * amount;
                const float actual[] = {red, green, blue};
                EXPECT_NEAR(actual[channel], expected, 2.0e-6f)
                    << "channel " << channel;
            }
        }
    }
}

TEST(ColoramaProcessorContractTest, HueModeMatchesIndependentFullHueWheelReference)
{
    constexpr std::array<std::array<float, 3>, 5> rainbow = {{
        {{1.00f, 0.20f, 0.20f}}, {{1.00f, 0.80f, 0.20f}},
        {{0.20f, 1.00f, 0.35f}}, {{0.20f, 0.70f, 1.00f}},
        {{0.85f, 0.20f, 1.00f}},
    }};
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Hue;
    settings.palette = ColoramaPalette::Rainbow;
    settings.strength = 1.0f;
    settings.saturationBoost = 1.0f;
    settings.contrast = 1.0f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    for (int hue = 0; hue < 360; ++hue) {
        const int sector = hue / 60;
        const float fraction = (hue % 60) / 60.0f;
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        switch (sector) {
        case 0: red = 1.0f; green = fraction; break;
        case 1: red = 1.0f - fraction; green = 1.0f; break;
        case 2: green = 1.0f; blue = fraction; break;
        case 3: green = 1.0f - fraction; blue = 1.0f; break;
        case 4: red = fraction; blue = 1.0f; break;
        default: red = 1.0f; blue = 1.0f - fraction; break;
        }

        const float key = hue / 360.0f;
        const float scaled = key * 4.0f;
        const int segment = std::min(static_cast<int>(scaled), 3);
        const float amount = scaled - segment;
        const float expected[] = {
            rainbow[segment][0] * (1.0f - amount) + rainbow[segment + 1][0] * amount,
            rainbow[segment][1] * (1.0f - amount) + rainbow[segment + 1][1] * amount,
            rainbow[segment][2] * (1.0f - amount) + rainbow[segment + 1][2] * amount,
        };
        processor.applyPixel(red, green, blue);

        SCOPED_TRACE(hue);
        EXPECT_NEAR(red, expected[0], 2.0e-6f);
        EXPECT_NEAR(green, expected[1], 2.0e-6f);
        EXPECT_NEAR(blue, expected[2], 2.0e-6f);
    }
}

TEST(ColoramaProcessorContractTest, ContrastAndSaturationGridMatchesIndependentHslReference)
{
    struct PaletteReference {
        ColoramaPalette palette;
        std::array<std::array<float, 3>, 5> colors;
    };
    constexpr PaletteReference palettes[] = {
        {ColoramaPalette::Rainbow, {{{1.00f, 0.20f, 0.20f}, {1.00f, 0.80f, 0.20f},
                                      {0.20f, 1.00f, 0.35f}, {0.20f, 0.70f, 1.00f},
                                      {0.85f, 0.20f, 1.00f}}}},
        {ColoramaPalette::Fire, {{{0.10f, 0.00f, 0.00f}, {0.55f, 0.10f, 0.00f},
                                   {0.90f, 0.35f, 0.00f}, {1.00f, 0.74f, 0.20f},
                                   {1.00f, 0.95f, 0.75f}}}},
        {ColoramaPalette::Ocean, {{{0.03f, 0.05f, 0.18f}, {0.00f, 0.30f, 0.45f},
                                    {0.10f, 0.65f, 0.75f}, {0.30f, 0.85f, 0.95f},
                                    {0.80f, 0.98f, 1.00f}}}},
        {ColoramaPalette::Neon, {{{0.00f, 0.95f, 0.75f}, {0.85f, 0.10f, 1.00f},
                                   {0.10f, 0.90f, 1.00f}, {1.00f, 0.15f, 0.45f},
                                   {0.90f, 1.00f, 0.20f}}}},
        {ColoramaPalette::Sunset, {{{0.05f, 0.04f, 0.16f}, {0.35f, 0.09f, 0.40f},
                                     {0.80f, 0.20f, 0.30f}, {0.98f, 0.45f, 0.12f},
                                     {1.00f, 0.86f, 0.58f}}}},
    };
    constexpr float keys[] = {0.0f, 0.0625f, 0.125f, 0.25f, 0.375f,
                              0.5f, 0.625f, 0.75f, 0.875f, 0.9375f};
    constexpr float contrasts[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    constexpr float saturationBoosts[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    const auto referenceRgbToHsl = [](const float rgb[3], float& hue,
                                       float& saturation, float& lightness) {
        const float high = std::max({rgb[0], rgb[1], rgb[2]});
        const float low = std::min({rgb[0], rgb[1], rgb[2]});
        const float chroma = high - low;
        lightness = (high + low) * 0.5f;
        if (chroma == 0.0f) {
            hue = 0.0f;
            saturation = 0.0f;
            return;
        }
        saturation = chroma / (1.0f - std::abs(2.0f * lightness - 1.0f));
        if (high == rgb[0]) {
            hue = std::fmod((rgb[1] - rgb[2]) / chroma, 6.0f);
        } else if (high == rgb[1]) {
            hue = (rgb[2] - rgb[0]) / chroma + 2.0f;
        } else {
            hue = (rgb[0] - rgb[1]) / chroma + 4.0f;
        }
        hue *= 60.0f;
        if (hue < 0.0f) hue += 360.0f;
    };
    const auto referenceHslToRgb = [](const float hue, const float saturation,
                                       const float lightness, float rgb[3]) {
        const float chroma = (1.0f - std::abs(2.0f * lightness - 1.0f)) * saturation;
        const float sector = hue / 60.0f;
        const float secondary = chroma * (1.0f - std::abs(std::fmod(sector, 2.0f) - 1.0f));
        float prime[3]{};
        if (sector < 1.0f) { prime[0] = chroma; prime[1] = secondary; }
        else if (sector < 2.0f) { prime[0] = secondary; prime[1] = chroma; }
        else if (sector < 3.0f) { prime[1] = chroma; prime[2] = secondary; }
        else if (sector < 4.0f) { prime[1] = secondary; prime[2] = chroma; }
        else if (sector < 5.0f) { prime[0] = secondary; prime[2] = chroma; }
        else { prime[0] = chroma; prime[2] = secondary; }
        const float match = lightness - chroma * 0.5f;
        for (int channel = 0; channel < 3; ++channel) rgb[channel] = prime[channel] + match;
    };

    ColoramaProcessor processor;
    for (const PaletteReference& palette : palettes) {
        for (const float key : keys) {
            const float scaledKey = key * 4.0f;
            const int segment = std::min(static_cast<int>(scaledKey), 3);
            const float amount = scaledKey - segment;
            const float rampSample[] = {
                palette.colors[segment][0] * (1.0f - amount) + palette.colors[segment + 1][0] * amount,
                palette.colors[segment][1] * (1.0f - amount) + palette.colors[segment + 1][1] * amount,
                palette.colors[segment][2] * (1.0f - amount) + palette.colors[segment + 1][2] * amount,
            };
            for (const float contrast : contrasts) {
                for (const float saturationBoost : saturationBoosts) {
                    float expected[3] = {
                        std::clamp((rampSample[0] - 0.5f) * contrast + 0.5f, 0.0f, 1.0f),
                        std::clamp((rampSample[1] - 0.5f) * contrast + 0.5f, 0.0f, 1.0f),
                        std::clamp((rampSample[2] - 0.5f) * contrast + 0.5f, 0.0f, 1.0f),
                    };
                    float hue = 0.0f;
                    float saturation = 0.0f;
                    float lightness = 0.0f;
                    referenceRgbToHsl(expected, hue, saturation, lightness);
                    referenceHslToRgb(hue, std::clamp(saturation * saturationBoost,
                                                     0.0f, 1.0f), lightness, expected);

                    ColoramaSettings settings;
                    settings.sourceMode = ColoramaSourceMode::Luma;
                    settings.palette = palette.palette;
                    settings.phase = 0.0f;
                    settings.spread = 1.0f;
                    settings.strength = 1.0f;
                    settings.contrast = contrast;
                    settings.saturationBoost = saturationBoost;
                    settings.preserveLuma = false;
                    processor.setSettings(settings);
                    float red = key;
                    float green = key;
                    float blue = key;
                    processor.applyPixel(red, green, blue);
                    const float actual[] = {red, green, blue};
                    SCOPED_TRACE(::testing::Message()
                        << "palette=" << static_cast<int>(palette.palette)
                        << " key=" << key << " contrast=" << contrast
                        << " saturationBoost=" << saturationBoost);
                    for (std::size_t channel = 0; channel < 3; ++channel) {
                        EXPECT_NEAR(actual[channel], expected[channel], 3.0e-6f)
                            << "channel " << channel;
                    }
                }
            }
        }
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

TEST(ColoramaProcessorContractTest, StrengthGridMatchesIndependentBlendReference)
{
    constexpr std::array<std::array<float, 3>, 5> fire = {{
        {{0.10f, 0.00f, 0.00f}}, {{0.55f, 0.10f, 0.00f}},
        {{0.90f, 0.35f, 0.00f}}, {{1.00f, 0.74f, 0.20f}},
        {{1.00f, 0.95f, 0.75f}},
    }};
    constexpr float keys[] = {0.0f, 0.0625f, 0.2f, 0.375f, 0.6f, 0.875f};
    constexpr float strengths[] = {0.0f, 0.1f, 0.25f, 0.5f, 0.8f, 1.0f};
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.saturationBoost = 1.0f;
    settings.contrast = 1.0f;
    settings.preserveLuma = false;

    for (const float key : keys) {
        const float scaled = key * 4.0f;
        const int segment = std::min(static_cast<int>(scaled), 3);
        const float amount = scaled - segment;
        const float mapped[] = {
            fire[segment][0] * (1.0f - amount) + fire[segment + 1][0] * amount,
            fire[segment][1] * (1.0f - amount) + fire[segment + 1][1] * amount,
            fire[segment][2] * (1.0f - amount) + fire[segment + 1][2] * amount,
        };
        for (const float strength : strengths) {
            settings.strength = strength;
            processor.setSettings(settings);
            float red = key;
            float green = key;
            float blue = key;
            processor.applyPixel(red, green, blue);
            const float actual[] = {red, green, blue};
            SCOPED_TRACE(::testing::Message()
                << "key=" << key << " strength=" << strength);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float expected = key * (1.0f - strength) + mapped[channel] * strength;
                EXPECT_NEAR(actual[channel], expected, 2.0e-6f)
                    << "channel " << channel;
            }
        }
    }
}

TEST(ColoramaProcessorContractTest, PreserveLumaMatchesIndependentScaleReferenceGrid)
{
    constexpr std::array<std::array<float, 3>, 5> fire = {{
        {{0.10f, 0.00f, 0.00f}}, {{0.55f, 0.10f, 0.00f}},
        {{0.90f, 0.35f, 0.00f}}, {{1.00f, 0.74f, 0.20f}},
        {{1.00f, 0.95f, 0.75f}},
    }};
    constexpr float sourceValues[] = {0.0f, 0.025f, 0.1f, 0.2f, 0.3f};
    constexpr float strengths[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
    constexpr float lumaWeights[] = {0.2126f, 0.7152f, 0.0722f};
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.saturationBoost = 1.0f;
    settings.contrast = 1.0f;
    settings.preserveLuma = true;

    for (const float sourceValue : sourceValues) {
        const float scaled = sourceValue * 4.0f;
        const int segment = std::min(static_cast<int>(scaled), 3);
        const float amount = scaled - segment;
        const float mapped[] = {
            fire[segment][0] * (1.0f - amount) + fire[segment + 1][0] * amount,
            fire[segment][1] * (1.0f - amount) + fire[segment + 1][1] * amount,
            fire[segment][2] * (1.0f - amount) + fire[segment + 1][2] * amount,
        };
        for (const float strength : strengths) {
            settings.strength = strength;
            processor.setSettings(settings);
            float expected[] = {
                sourceValue * (1.0f - strength) + mapped[0] * strength,
                sourceValue * (1.0f - strength) + mapped[1] * strength,
                sourceValue * (1.0f - strength) + mapped[2] * strength,
            };
            const float targetLuma = sourceValue;
            const float mappedLuma = expected[0] * lumaWeights[0]
                + expected[1] * lumaWeights[1] + expected[2] * lumaWeights[2];
            if (mappedLuma > 1.0e-6f) {
                const float scale = targetLuma / mappedLuma;
                for (float& channel : expected) {
                    channel = std::clamp(channel * scale, 0.0f, 1.0f);
                }
            }

            float red = sourceValue;
            float green = sourceValue;
            float blue = sourceValue;
            processor.applyPixel(red, green, blue);
            const float actual[] = {red, green, blue};
            SCOPED_TRACE(::testing::Message()
                << "source=" << sourceValue << " strength=" << strength);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                EXPECT_NEAR(actual[channel], expected[channel], 2.0e-6f)
                    << "channel " << channel;
            }
            const float actualLuma = red * lumaWeights[0]
                + green * lumaWeights[1] + blue * lumaWeights[2];
            EXPECT_NEAR(actualLuma, targetLuma, 2.0e-6f);
        }
    }
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

TEST(ColoramaProcessorContractTest, ImageApplyKeepsNullInputNullAtPositiveAndZeroStrength)
{
    const QImage nullImage;
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.strength = 1.0f;
    processor.setSettings(settings);
    EXPECT_TRUE(processor.apply(nullImage).isNull());

    settings.strength = 0.0f;
    processor.setSettings(settings);
    EXPECT_TRUE(processor.apply(nullImage).isNull());
}

TEST(ColoramaProcessorContractTest, ImageApplyConvertsGrayscaleAndPremultipliedFormats)
{
    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.strength = 1.0f;
    settings.saturationBoost = 1.0f;
    settings.contrast = 1.0f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    QImage grayscale(1, 1, QImage::Format_Grayscale8);
    grayscale.setPixelColor(0, 0, QColor(64, 64, 64));
    QImage premultiplied(1, 1, QImage::Format_ARGB32_Premultiplied);
    premultiplied.setPixelColor(0, 0, QColor(64, 64, 64, 128));

    const auto expectConvertedPixel = [&processor](const QImage& source,
                                                   const int expectedAlpha) {
        const QColor sourceColor = source.pixelColor(0, 0);
        const float key = sourceColor.redF() * 0.2126f
            + sourceColor.greenF() * 0.7152f + sourceColor.blueF() * 0.0722f;
        const float scaled = key * 4.0f;
        const int segment = std::min(static_cast<int>(scaled), 3);
        const float amount = scaled - segment;
        constexpr float fire[5][3] = {
            {0.10f, 0.00f, 0.00f}, {0.55f, 0.10f, 0.00f},
            {0.90f, 0.35f, 0.00f}, {1.00f, 0.74f, 0.20f},
            {1.00f, 0.95f, 0.75f},
        };
        int expected[3]{};
        for (int channel = 0; channel < 3; ++channel) {
            const float value = fire[segment][channel] * (1.0f - amount)
                + fire[segment + 1][channel] * amount;
            expected[channel] = static_cast<int>(value * 255.0f);
        }
        const QImage result = processor.apply(source);
        EXPECT_EQ(result.format(), QImage::Format_ARGB32);
        const QRgb pixel = result.pixel(0, 0);
        EXPECT_EQ(qRed(pixel), expected[0]);
        EXPECT_EQ(qGreen(pixel), expected[1]);
        EXPECT_EQ(qBlue(pixel), expected[2]);
        EXPECT_EQ(qAlpha(pixel), expectedAlpha);
    };

    expectConvertedPixel(grayscale, 255);
    expectConvertedPixel(premultiplied, 128);
    EXPECT_EQ(grayscale.format(), QImage::Format_Grayscale8);
    EXPECT_EQ(premultiplied.format(), QImage::Format_ARGB32_Premultiplied);
}

TEST(ColoramaProcessorContractTest,
     ImageApplyMatchesStraightAndPremultipliedColorPixelsAcrossAlphaValues)
{
    constexpr std::array<int, 8> alphaValues = {32, 64, 96, 128, 192, 224, 254, 255};
    constexpr std::array<std::array<int, 3>, 8> rgbValues = {{
        {{231, 71, 33}}, {{28, 194, 87}}, {{45, 92, 226}}, {{210, 177, 49}},
        {{153, 42, 202}}, {{12, 161, 211}}, {{246, 119, 68}}, {{77, 208, 143}},
    }};
    QImage straight(static_cast<int>(alphaValues.size()), 1, QImage::Format_RGBA8888);
    QImage premultiplied(static_cast<int>(alphaValues.size()), 1,
                         QImage::Format_RGBA8888_Premultiplied);
    ASSERT_FALSE(straight.isNull());
    ASSERT_FALSE(premultiplied.isNull());
    for (int x = 0; x < static_cast<int>(alphaValues.size()); ++x) {
        const auto& rgb = rgbValues[static_cast<std::size_t>(x)];
        const QColor color(rgb[0], rgb[1], rgb[2], alphaValues[static_cast<std::size_t>(x)]);
        straight.setPixelColor(x, 0, color);
        premultiplied.setPixelColor(x, 0, color);
    }
    const QImage originalStraight = straight.copy();
    const QImage originalPremultiplied = premultiplied.copy();

    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Hue;
    settings.palette = ColoramaPalette::Sunset;
    settings.phase = 0.137f;
    settings.strength = 0.73f;
    settings.contrast = 0.94f;
    settings.saturationBoost = 1.08f;
    settings.preserveLuma = false;
    processor.setSettings(settings);

    const QImage straightResult = processor.apply(straight);
    const QImage premultipliedResult = processor.apply(premultiplied);
    ASSERT_EQ(straightResult.format(), QImage::Format_ARGB32);
    ASSERT_EQ(premultipliedResult.format(), QImage::Format_ARGB32);
    ASSERT_EQ(straightResult.size(), premultipliedResult.size());
    for (int x = 0; x < static_cast<int>(alphaValues.size()); ++x) {
        const QRgb straightPixel = straightResult.pixel(x, 0);
        const QRgb premultipliedPixel = premultipliedResult.pixel(x, 0);
        SCOPED_TRACE(::testing::Message() << "pixel=" << x
            << " alpha=" << alphaValues[static_cast<std::size_t>(x)]);
        EXPECT_NEAR(qRed(straightPixel), qRed(premultipliedPixel), 3);
        EXPECT_NEAR(qGreen(straightPixel), qGreen(premultipliedPixel), 3);
        EXPECT_NEAR(qBlue(straightPixel), qBlue(premultipliedPixel), 3);
        EXPECT_EQ(qAlpha(straightPixel), alphaValues[static_cast<std::size_t>(x)]);
        EXPECT_EQ(qAlpha(premultipliedPixel), alphaValues[static_cast<std::size_t>(x)]);
    }
    EXPECT_EQ(straight, originalStraight);
    EXPECT_EQ(premultiplied, originalPremultiplied);
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

TEST(ColoramaProcessorContractTest, ImageApplyProcessesPixelsAcrossTileEdgesAndPreservesEveryAlpha)
{
    constexpr int width = 35;
    constexpr int height = 33;
    QImage source(width, height, QImage::Format_RGBA8888);
    ASSERT_FALSE(source.isNull());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int alpha = (x * 19 + y * 23) & 0xff;
            source.setPixel(x, y, qRgba(0, 0, 0, alpha));
        }
    }
    const QImage original = source.copy();

    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.phase = 0.0f;
    settings.strength = 1.0f;
    settings.saturationBoost = 1.0f;
    settings.contrast = 1.0f;
    settings.preserveLuma = false;
    processor.setSettings(settings);
    const QImage result = processor.apply(source);

    ASSERT_EQ(result.size(), source.size());
    EXPECT_EQ(result.format(), QImage::Format_ARGB32);
    EXPECT_EQ(source, original);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QRgb pixel = result.pixel(x, y);
            SCOPED_TRACE(::testing::Message() << "x=" << x << " y=" << y);
            EXPECT_EQ(qRed(pixel), 25);
            EXPECT_EQ(qGreen(pixel), 0);
            EXPECT_EQ(qBlue(pixel), 0);
            EXPECT_EQ(qAlpha(pixel), qAlpha(original.pixel(x, y)));
        }
    }
}

TEST(ColoramaProcessorContractTest,
     ImageApplyMatchesIndependentVariedPixelReferenceAcrossTileEdges)
{
    constexpr int width = 65;
    constexpr int height = 67;
    constexpr std::array<std::array<float, 3>, 5> fire = {{
        {{0.10f, 0.00f, 0.00f}}, {{0.55f, 0.10f, 0.00f}},
        {{0.90f, 0.35f, 0.00f}}, {{1.00f, 0.74f, 0.20f}},
        {{1.00f, 0.95f, 0.75f}},
    }};
    constexpr float lumaWeights[] = {0.2126f, 0.7152f, 0.0722f};
    constexpr float phase = 0.071f;
    constexpr float spread = 1.15f;
    constexpr float strength = 0.73f;

    QImage source(width, height, QImage::Format_RGBA8888);
    ASSERT_FALSE(source.isNull());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int red = (x * 37 + y * 17 + 11) & 0xff;
            const int green = (x * 13 + y * 53 + 79) & 0xff;
            const int blue = (x * 61 + y * 7 + 131) & 0xff;
            const int alpha = (x * 29 + y * 31 + 3) & 0xff;
            source.setPixelColor(x, y, QColor(red, green, blue, alpha));
        }
    }
    const QImage original = source.copy();

    ColoramaProcessor processor;
    ColoramaSettings settings;
    settings.sourceMode = ColoramaSourceMode::Luma;
    settings.palette = ColoramaPalette::Fire;
    settings.phase = phase;
    settings.spread = spread;
    settings.strength = strength;
    settings.saturationBoost = 1.0f;
    settings.contrast = 1.0f;
    settings.preserveLuma = false;
    processor.setSettings(settings);
    const QImage result = processor.apply(source);

    ASSERT_EQ(result.size(), source.size());
    ASSERT_EQ(result.format(), QImage::Format_ARGB32);
    EXPECT_EQ(source, original);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QColor color = original.pixelColor(x, y);
            const float input[] = {
                color.redF(), color.greenF(), color.blueF()};
            const float keyLuma = input[0] * lumaWeights[0]
                + input[1] * lumaWeights[1] + input[2] * lumaWeights[2];
            float key = std::fmod(keyLuma * spread + phase, 1.0f);
            if (key < 0.0f) key += 1.0f;
            const float scaled = key * 4.0f;
            const int segment = std::min(static_cast<int>(scaled), 3);
            const float amount = scaled - segment;
            const QRgb actual = result.pixel(x, y);

            SCOPED_TRACE(::testing::Message() << "x=" << x << " y=" << y
                << " luma=" << keyLuma << " key=" << key);
            for (int channel = 0; channel < 3; ++channel) {
                const float mapped = fire[segment][channel] * (1.0f - amount)
                    + fire[segment + 1][channel] * amount;
                const float output = input[channel] * (1.0f - strength)
                    + mapped * strength;
                const int expected = static_cast<int>(std::clamp(output, 0.0f, 1.0f) * 255.0f);
                const int actualChannel = channel == 0 ? qRed(actual)
                    : (channel == 1 ? qGreen(actual) : qBlue(actual));
                EXPECT_NEAR(actualChannel, expected, 1);
            }
            EXPECT_EQ(qAlpha(actual), color.alpha());
        }
    }
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
