#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

import Color.BlendMode;
import Color.Conversion;
import Color.Float;

using namespace ArtifactCore;

TEST(ColorBlendModeContractTest, OpacityZeroReturnsTheBaseExactly)
{
    const FloatColor base(0.2f, 0.4f, 0.7f, 0.65f);
    const FloatColor source(0.9f, 0.1f, 0.3f, 0.8f);

    const FloatColor result = ColorBlendMode::blend(base, source, BlendMode::Screen, 0.0f);

    EXPECT_EQ(result, base);
}

TEST(ColorBlendModeContractTest, OpacityOneNormalUsesSourceColorAndAlpha)
{
    const FloatColor base(0.2f, 0.4f, 0.7f, 1.0f);
    const FloatColor source(0.8f, 0.3f, 0.1f, 1.0f);

    const FloatColor result = ColorBlendMode::blend(base, source, BlendMode::Normal, 1.0f);

    EXPECT_FLOAT_EQ(result.r(), source.r());
    EXPECT_FLOAT_EQ(result.g(), source.g());
    EXPECT_FLOAT_EQ(result.b(), source.b());
    EXPECT_FLOAT_EQ(result.a(), 1.0f);
    EXPECT_EQ(base, FloatColor(0.2f, 0.4f, 0.7f, 1.0f));
    EXPECT_EQ(source, FloatColor(0.8f, 0.3f, 0.1f, 1.0f));
}

TEST(ColorBlendModeContractTest, MultiplyMatchesPerChannelProductForOpaqueInputs)
{
    const FloatColor base(0.2f, 0.5f, 0.8f, 1.0f);
    const FloatColor source(0.5f, 0.4f, 0.25f, 1.0f);

    const FloatColor result = ColorBlendMode::blend(base, source, BlendMode::Multiply);

    EXPECT_NEAR(result.r(), 0.1f, 1e-6f);
    EXPECT_NEAR(result.g(), 0.2f, 1e-6f);
    EXPECT_NEAR(result.b(), 0.2f, 1e-6f);
    EXPECT_FLOAT_EQ(result.a(), 1.0f);
}

TEST(ColorBlendModeContractTest, ArithmeticModesMatchFormulaBoundaries)
{
    const auto checkRgb = [](const FloatColor& base, const FloatColor& source,
                             const BlendMode mode, const FloatColor& expected) {
        const FloatColor result = ColorBlendMode::blend(base, source, mode);
        EXPECT_NEAR(result.r(), expected.r(), 1e-6f);
        EXPECT_NEAR(result.g(), expected.g(), 1e-6f);
        EXPECT_NEAR(result.b(), expected.b(), 1e-6f);
        EXPECT_FLOAT_EQ(result.a(), 1.0f);
    };

    const FloatColor base(0.5f, 0.499f, 0.75f, 1.0f);
    const FloatColor source(0.4f, 0.4f, 0.5f, 1.0f);
    checkRgb(base, source, BlendMode::Overlay, FloatColor(0.4f, 0.3992f, 0.75f, 1.0f));
    checkRgb(base, source, BlendMode::HardLight, FloatColor(0.4f, 0.3992f, 0.75f, 1.0f));

    checkRgb(FloatColor(0.25f, 0.25f, 0.75f, 1.0f),
             FloatColor(1.0f, 0.5f, 0.5f, 1.0f), BlendMode::ColorDodge,
             FloatColor(1.0f, 0.5f, 1.0f, 1.0f));
    checkRgb(FloatColor(0.75f, 0.25f, 0.25f, 1.0f),
             FloatColor(0.0f, 0.5f, 0.5f, 1.0f), BlendMode::ColorBurn,
             FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    checkRgb(FloatColor(0.25f, 0.5f, 0.75f, 1.0f),
             FloatColor(0.0f, 0.5f, 1e-7f, 1.0f), BlendMode::Divide,
             FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
}

TEST(ColorBlendModeContractTest, SoftLightMatchesReferenceCurveAcrossBranchGrid)
{
    const auto referenceSoftLight = [](const float base, const float source) {
        if (source < 0.5f) {
            return base - (1.0f - 2.0f * source) * base * (1.0f - base);
        }

        const float curve = base <= 0.25f
            ? ((16.0f * base - 12.0f) * base + 4.0f) * base
            : std::sqrt(base);
        return base + (2.0f * source - 1.0f) * (curve - base);
    };
    constexpr float values[] = {
        0.0f, 0.1f, 0.25f, 0.49f, 0.5f, 0.51f, 0.75f, 0.9f, 1.0f
    };

    for (const float baseValue : values) {
        for (const float sourceValue : values) {
            const FloatColor base(baseValue, sourceValue, 1.0f - baseValue, 1.0f);
            const FloatColor source(sourceValue, baseValue, 1.0f - sourceValue, 1.0f);
            const FloatColor result = ColorBlendMode::blend(base, source, BlendMode::SoftLight);
            SCOPED_TRACE(::testing::Message()
                << "base=" << baseValue << " source=" << sourceValue);

            EXPECT_NEAR(result.r(), referenceSoftLight(baseValue, sourceValue), 2e-6f);
            EXPECT_NEAR(result.g(), referenceSoftLight(sourceValue, baseValue), 2e-6f);
            EXPECT_NEAR(result.b(), referenceSoftLight(1.0f - baseValue, 1.0f - sourceValue), 2e-6f);
            EXPECT_FLOAT_EQ(result.a(), 1.0f);
        }
    }
}

TEST(ColorBlendModeContractTest, StandardArithmeticModesMatchReferenceGridAndAlphaComposition)
{
    using Formula = float (*)(float, float);
    struct ModeCase {
        BlendMode mode;
        Formula formula;
    };
    const ModeCase cases[] = {
        {BlendMode::Add, [](float base, float source) {
             return std::min(base + source, 1.0f);
         }},
        {BlendMode::Subtract, [](float base, float source) {
             return std::max(base - source, 0.0f);
         }},
        {BlendMode::Screen, [](float base, float source) {
             return 1.0f - (1.0f - base) * (1.0f - source);
         }},
        {BlendMode::Darken, [](float base, float source) {
             return std::min(base, source);
         }},
        {BlendMode::Lighten, [](float base, float source) {
             return std::max(base, source);
         }},
        {BlendMode::Difference, [](float base, float source) {
             return std::abs(base - source);
         }},
        {BlendMode::Exclusion, [](float base, float source) {
             return base + source - 2.0f * base * source;
         }},
        {BlendMode::LinearBurn, [](float base, float source) {
             return std::max(base + source - 1.0f, 0.0f);
         }},
        {BlendMode::LinearLight, [](float base, float source) {
             return std::clamp(base + 2.0f * source - 1.0f, 0.0f, 1.0f);
         }},
        {BlendMode::HardMix, [](float base, float source) {
             return base + source >= 1.0f ? 1.0f : 0.0f;
         }},
    };
    constexpr float values[] = {
        0.0f, 0.1f, 0.25f, 0.49f, 0.5f, 0.51f, 0.75f, 0.9f, 1.0f,
    };
    constexpr float opacities[] = {0.15f, 0.5f, 1.0f};
    constexpr float baseAlpha = 0.37f;
    constexpr float sourceAlpha = 0.23f;

    for (const ModeCase& testCase : cases) {
        for (const float baseValue : values) {
            for (const float sourceValue : values) {
                const FloatColor base(baseValue, 1.0f - baseValue, 0.5f, baseAlpha);
                const FloatColor source(sourceValue, 0.5f, 1.0f - sourceValue, sourceAlpha);
                for (const float opacity : opacities) {
                    const float outAlpha = opacity + baseAlpha * (1.0f - opacity);
                    const auto expectedChannel = [&](const float baseChannel,
                                                     const float sourceChannel) {
                        const float blended = testCase.formula(baseChannel, sourceChannel);
                        const float premultiplied =
                            baseChannel * baseAlpha * (1.0f - opacity) +
                            (blended * baseAlpha + sourceChannel * (1.0f - baseAlpha)) * opacity;
                        return std::clamp(premultiplied / outAlpha, 0.0f, 1.0f);
                    };
                    const FloatColor result = ColorBlendMode::blend(
                        base, source, testCase.mode, opacity);
                    SCOPED_TRACE(::testing::Message()
                        << "mode=" << static_cast<int>(testCase.mode)
                        << " base=" << baseValue << " source=" << sourceValue
                        << " opacity=" << opacity);
                    EXPECT_NEAR(result.r(), expectedChannel(base.r(), source.r()), 1e-6f);
                    EXPECT_NEAR(result.g(), expectedChannel(base.g(), source.g()), 1e-6f);
                    EXPECT_NEAR(result.b(), expectedChannel(base.b(), source.b()), 1e-6f);
                    EXPECT_NEAR(result.a(), outAlpha, 1e-6f);
                }
            }
        }
    }
}

TEST(ColorBlendModeContractTest, OpacityScalesSourceAlphaOverTransparentBase)
{
    const FloatColor base(0.1f, 0.2f, 0.3f, 0.0f);
    const FloatColor source(0.8f, 0.4f, 0.2f, 0.5f);

    const FloatColor result = ColorBlendMode::blend(base, source, BlendMode::Normal, 0.5f);

    EXPECT_FLOAT_EQ(result.r(), source.r());
    EXPECT_FLOAT_EQ(result.g(), source.g());
    EXPECT_FLOAT_EQ(result.b(), source.b());
    EXPECT_NEAR(result.a(), 0.5f, 1e-6f);
}

TEST(ColorBlendModeContractTest, ArithmeticModesStayFiniteAndBoundedAtEndpoints)
{
    constexpr BlendMode modes[] = {
        BlendMode::Add, BlendMode::Subtract, BlendMode::Multiply,
        BlendMode::Screen, BlendMode::Overlay, BlendMode::ColorDodge,
        BlendMode::ColorBurn, BlendMode::Divide, BlendMode::VividLight,
        BlendMode::LinearLight, BlendMode::HardMix
    };
    const FloatColor base(0.0f, 0.5f, 1.0f, 1.0f);
    const FloatColor source(1.0f, 0.0f, 0.5f, 1.0f);

    for (const auto mode : modes) {
        const FloatColor result = ColorBlendMode::blend(base, source, mode);
        SCOPED_TRACE(static_cast<int>(mode));
        for (const float channel : {result.r(), result.g(), result.b(), result.a()}) {
            EXPECT_TRUE(std::isfinite(channel));
            EXPECT_GE(channel, 0.0f);
            EXPECT_LE(channel, 1.0f);
        }
    }
}

TEST(ColorBlendModeContractTest, NormalBlendMatchesOpacityGridForTranslucentBase)
{
    const FloatColor base(0.2f, 0.4f, 0.6f, 0.25f);
    const FloatColor source(0.8f, 0.3f, 0.1f, 1.0f);
    constexpr float opacities[] = {-0.5f, 0.0f, 0.25f, 0.5f, 1.0f, 1.5f};

    for (const float opacity : opacities) {
        const float alpha = std::clamp(opacity, 0.0f, 1.0f);
        const float outAlpha = alpha + base.a() * (1.0f - alpha);
        const FloatColor result = ColorBlendMode::blend(
            base, source, BlendMode::Normal, opacity);
        SCOPED_TRACE(opacity);

        EXPECT_NEAR(result.a(), outAlpha, 1e-6f);
        EXPECT_NEAR(result.r(), (base.r() * base.a() * (1.0f - alpha) + source.r() * alpha) / outAlpha, 1e-6f);
        EXPECT_NEAR(result.g(), (base.g() * base.a() * (1.0f - alpha) + source.g() * alpha) / outAlpha, 1e-6f);
        EXPECT_NEAR(result.b(), (base.b() * base.a() * (1.0f - alpha) + source.b() * alpha) / outAlpha, 1e-6f);
    }
}

TEST(ColorBlendModeContractTest, HslModesReplaceOnlyTheirNamedComponents)
{
    const auto baseRgb = ColorConversion::HSLToRGB({30.0f, 0.6f, 0.4f});
    const auto sourceRgb = ColorConversion::HSLToRGB({210.0f, 0.8f, 0.7f});
    const FloatColor base(baseRgb[0], baseRgb[1], baseRgb[2], 1.0f);
    const FloatColor source(sourceRgb[0], sourceRgb[1], sourceRgb[2], 1.0f);

    struct Case { BlendMode mode; float hue; float saturation; float lightness; };
    constexpr Case cases[] = {
        {BlendMode::Hue, 210.0f, 0.6f, 0.4f},
        {BlendMode::Saturation, 30.0f, 0.8f, 0.4f},
        {BlendMode::Color, 210.0f, 0.8f, 0.4f},
        {BlendMode::Luminosity, 30.0f, 0.6f, 0.7f},
    };

    for (const auto& testCase : cases) {
        const FloatColor result = ColorBlendMode::blend(base, source, testCase.mode);
        const HSLColor hsl = ColorConversion::RGBToHSL(result.r(), result.g(), result.b());
        SCOPED_TRACE(static_cast<int>(testCase.mode));
        EXPECT_NEAR(hsl.h, testCase.hue, 1e-3f);
        EXPECT_NEAR(hsl.s, testCase.saturation, 1e-5f);
        EXPECT_NEAR(hsl.l, testCase.lightness, 1e-5f);
    }
}

TEST(ColorBlendModeContractTest, HslModesPreserveAndReplaceComponentsAcrossBoundaryGrid)
{
    constexpr float hues[] = {0.0f, 30.0f, 60.0f, 120.0f, 180.0f, 240.0f, 330.0f};
    constexpr float saturations[] = {0.0f, 0.1f, 0.5f, 1.0f};
    constexpr float lightnesses[] = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};
    constexpr BlendMode modes[] = {
        BlendMode::Hue, BlendMode::Saturation,
        BlendMode::Color, BlendMode::Luminosity,
    };

    for (const float baseHue : hues) {
        for (const float sourceHue : hues) {
            for (const float baseSaturation : saturations) {
                for (const float sourceSaturation : saturations) {
                    for (const float baseLightness : lightnesses) {
                        for (const float sourceLightness : lightnesses) {
                            const auto baseRgb = ColorConversion::HSLToRGB(
                                {baseHue, baseSaturation, baseLightness});
                            const auto sourceRgb = ColorConversion::HSLToRGB(
                                {sourceHue, sourceSaturation, sourceLightness});
                            const FloatColor base(
                                baseRgb[0], baseRgb[1], baseRgb[2], 1.0f);
                            const FloatColor source(
                                sourceRgb[0], sourceRgb[1], sourceRgb[2], 1.0f);
                            const HSLColor baseHsl = ColorConversion::RGBToHSL(
                                base.r(), base.g(), base.b());
                            const HSLColor sourceHsl = ColorConversion::RGBToHSL(
                                source.r(), source.g(), source.b());

                            for (const BlendMode mode : modes) {
                                HSLColor expectedHsl = baseHsl;
                                if (mode == BlendMode::Hue || mode == BlendMode::Color) {
                                    expectedHsl.h = sourceHsl.h;
                                }
                                if (mode == BlendMode::Saturation || mode == BlendMode::Color) {
                                    expectedHsl.s = sourceHsl.s;
                                }
                                if (mode == BlendMode::Luminosity) {
                                    expectedHsl.l = sourceHsl.l;
                                }
                                const auto expected = ColorConversion::HSLToRGB(expectedHsl);
                                const FloatColor result = ColorBlendMode::blend(base, source, mode);
                                SCOPED_TRACE(::testing::Message()
                                    << "mode=" << static_cast<int>(mode)
                                    << " baseHSL=" << baseHue << ',' << baseSaturation
                                    << ',' << baseLightness
                                    << " sourceHSL=" << sourceHue << ',' << sourceSaturation
                                    << ',' << sourceLightness);
                                EXPECT_NEAR(result.r(), expected[0], 2e-6f);
                                EXPECT_NEAR(result.g(), expected[1], 2e-6f);
                                EXPECT_NEAR(result.b(), expected[2], 2e-6f);
                                EXPECT_FLOAT_EQ(result.a(), 1.0f);
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST(ColorBlendModeContractTest, StencilAndSilhouetteModesTransformOnlyBaseAlpha)
{
    const FloatColor base(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatColor source(0.25f, 0.5f, 0.75f, 0.3f);
    constexpr float opacity = 0.4f;
    const float sourceLuma = 0.2126f * source.r() + 0.7152f * source.g() + 0.0722f * source.b();

    const FloatColor stencilAlpha = ColorBlendMode::blend(base, source, BlendMode::StencilAlpha, opacity);
    const FloatColor stencilLuma = ColorBlendMode::blend(base, source, BlendMode::StencilLuma, opacity);
    const FloatColor silhouetteAlpha = ColorBlendMode::blend(base, source, BlendMode::SilhouetteAlpha, opacity);
    const FloatColor silhouetteLuma = ColorBlendMode::blend(base, source, BlendMode::SilhouetteLuma, opacity);

    EXPECT_NEAR(stencilAlpha.a(), base.a() * opacity, 1e-6f);
    EXPECT_NEAR(stencilLuma.a(), base.a() * sourceLuma * opacity, 1e-6f);
    EXPECT_NEAR(silhouetteAlpha.a(), base.a() * (1.0f - opacity), 1e-6f);
    EXPECT_NEAR(silhouetteLuma.a(), base.a() * (1.0f - sourceLuma * opacity), 1e-6f);
    for (const FloatColor* result : {&stencilAlpha, &stencilLuma, &silhouetteAlpha, &silhouetteLuma}) {
        EXPECT_FLOAT_EQ(result->r(), base.r());
        EXPECT_FLOAT_EQ(result->g(), base.g());
        EXPECT_FLOAT_EQ(result->b(), base.b());
    }
}

TEST(ColorBlendModeContractTest, StencilAndSilhouetteAlphaMatchClampedBoundaryGrid)
{
    constexpr BlendMode modes[] = {
        BlendMode::StencilAlpha, BlendMode::StencilLuma,
        BlendMode::SilhouetteAlpha, BlendMode::SilhouetteLuma,
    };
    constexpr float baseAlphas[] = {-0.25f, 0.0f, 0.2f, 0.75f, 1.0f, 1.25f};
    constexpr float opacities[] = {-0.5f, 0.0f, 0.25f, 0.75f, 1.0f, 1.5f};
    constexpr std::array<std::array<float, 3>, 4> sourceColors = {{
        {{0.0f, 0.0f, 0.0f}},
        {{0.25f, 0.5f, 0.75f}},
        {{1.0f, 1.0f, 1.0f}},
        {{-0.5f, 1.5f, 0.25f}},
    }};

    for (const BlendMode mode : modes) {
        for (const float baseAlpha : baseAlphas) {
            for (const float opacity : opacities) {
                for (const auto& rgb : sourceColors) {
                    const FloatColor base(0.2f, -0.1f, 1.25f, baseAlpha);
                    const FloatColor source(rgb[0], rgb[1], rgb[2], 0.37f);
                    const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
                    const float luma = 0.2126f * rgb[0] +
                                       0.7152f * rgb[1] +
                                       0.0722f * rgb[2];
                    const bool lumaMode = mode == BlendMode::StencilLuma ||
                                          mode == BlendMode::SilhouetteLuma;
                    const bool silhouette = mode == BlendMode::SilhouetteAlpha ||
                                            mode == BlendMode::SilhouetteLuma;
                    const float factor = lumaMode
                        ? std::clamp(luma * clampedOpacity, 0.0f, 1.0f)
                        : clampedOpacity;
                    const float expectedAlpha = opacity <= 0.0f
                        ? baseAlpha
                        : std::clamp(
                              baseAlpha * (silhouette ? 1.0f - factor : factor),
                              0.0f, 1.0f);

                    const FloatColor result = ColorBlendMode::blend(
                        base, source, mode, opacity);
                    SCOPED_TRACE(::testing::Message()
                        << "mode=" << static_cast<int>(mode)
                        << " baseAlpha=" << baseAlpha << " opacity=" << opacity
                        << " sourceRGB=" << rgb[0] << ',' << rgb[1] << ',' << rgb[2]);
                    EXPECT_FLOAT_EQ(result.r(), base.r());
                    EXPECT_FLOAT_EQ(result.g(), base.g());
                    EXPECT_FLOAT_EQ(result.b(), base.b());
                    EXPECT_NEAR(result.a(), expectedAlpha, 1.0e-6f);
                }
            }
        }
    }
}

TEST(ColorBlendModeContractTest, EveryDeclaredModeStaysFiniteAndBoundedAcrossRgbGrid)
{
    constexpr BlendMode modes[] = {
        BlendMode::Normal, BlendMode::Add, BlendMode::Subtract, BlendMode::Multiply,
        BlendMode::Screen, BlendMode::Overlay, BlendMode::Darken, BlendMode::Lighten,
        BlendMode::ColorDodge, BlendMode::ColorBurn, BlendMode::HardLight,
        BlendMode::SoftLight, BlendMode::Difference, BlendMode::Exclusion,
        BlendMode::Hue, BlendMode::Saturation, BlendMode::Color, BlendMode::Luminosity,
        BlendMode::LinearBurn, BlendMode::Divide, BlendMode::PinLight,
        BlendMode::VividLight, BlendMode::LinearLight, BlendMode::HardMix,
        BlendMode::Dissolve, BlendMode::DancingDissolve, BlendMode::ClassicColorBurn,
        BlendMode::LinearDodge, BlendMode::ClassicColorDodge,
        BlendMode::ClassicDifference, BlendMode::StencilAlpha, BlendMode::StencilLuma,
        BlendMode::SilhouetteAlpha, BlendMode::SilhouetteLuma
    };
    constexpr float values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    constexpr float opacities[] = {-0.5f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f};

    for (const auto mode : modes) {
        for (const float red : values) {
            for (const float green : values) {
                const FloatColor base(red, green, 1.0f - red, 0.5f);
                const FloatColor source(1.0f - green, red, green, 0.75f);
                const FloatColor fullOpacity =
                    ColorBlendMode::blend(base, source, mode, 1.0f);
                for (const float opacity : opacities) {
                    const FloatColor result = ColorBlendMode::blend(base, source, mode, opacity);
                    SCOPED_TRACE(::testing::Message()
                        << "mode=" << static_cast<int>(mode)
                        << " base=" << red << ',' << green
                        << " opacity=" << opacity);
                    for (const float channel : {result.r(), result.g(), result.b(), result.a()}) {
                        EXPECT_TRUE(std::isfinite(channel));
                        EXPECT_GE(channel, 0.0f);
                        EXPECT_LE(channel, 1.0f);
                    }
                    if (opacity <= 0.0f) {
                        EXPECT_EQ(result, base);
                    } else if (opacity >= 1.0f) {
                        EXPECT_EQ(result, fullOpacity);
                    }
                }
            }
        }
    }
}

TEST(ColorBlendModeContractTest, LegacyAliasesMatchTheirCanonicalModes)
{
    const FloatColor base(0.23f, 0.51f, 0.82f, 0.7f);
    const FloatColor source(0.76f, 0.39f, 0.17f, 0.4f);
    constexpr struct AliasCase { BlendMode alias; BlendMode canonical; } cases[] = {
        {BlendMode::ClassicColorBurn, BlendMode::ColorBurn},
        {BlendMode::LinearDodge, BlendMode::Add},
        {BlendMode::ClassicColorDodge, BlendMode::ColorDodge},
        {BlendMode::ClassicDifference, BlendMode::Difference}
    };

    for (const auto& testCase : cases) {
        const FloatColor alias = ColorBlendMode::blend(base, source, testCase.alias, 0.65f);
        const FloatColor canonical = ColorBlendMode::blend(base, source, testCase.canonical, 0.65f);
        SCOPED_TRACE(static_cast<int>(testCase.alias));
        EXPECT_EQ(alias, canonical);
    }
}

TEST(ColorBlendModeContractTest, UnknownModeReturnsBaseAtPositiveOpacity)
{
    const FloatColor base(0.23f, 0.51f, 0.82f, 0.7f);
    const FloatColor source(0.76f, 0.39f, 0.17f, 0.4f);
    constexpr auto unknown = static_cast<BlendMode>(255);

    EXPECT_EQ(ColorBlendMode::blend(base, source, unknown, 1.0f), base);
    EXPECT_EQ(ColorBlendMode::blend(base, source, unknown, 0.4f), base);
}

TEST(ColorBlendModeContractTest, NonPositiveOpacityReturnsBaseBeforeModeDispatch)
{
    const FloatColor base(0.23f, 0.51f, 0.82f, 0.7f);
    const FloatColor source(0.76f, 0.39f, 0.17f, 0.4f);
    constexpr auto unknown = static_cast<BlendMode>(255);

    EXPECT_EQ(ColorBlendMode::blend(base, source, BlendMode::Hue, 0.0f), base);
    EXPECT_EQ(ColorBlendMode::blend(base, source, unknown, -0.25f), base);
}
