#include <gtest/gtest.h>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>

import Color.Float;
import FloatRGBA;
import Color.Bridge;
import Color.Tagged;
import Color.TransferFunction;
import Color.GamutConversion;
import Graphics.SurfaceColorContract;

using namespace ArtifactCore;

namespace {
constexpr float kColorEpsilon = 1.0f / 65535.0f * 2.0f;
}

TEST(ColorBridgeTest, QColorRoundTripPreservesChannels)
{
    const FloatColor color(0.25f, 0.5f, 0.75f, 0.125f);
    const QColor qt = toQColor(color);
    const FloatColor restored = toFloatColor(qt);

    EXPECT_NEAR(restored.r(), 0.25f, kColorEpsilon);
    EXPECT_NEAR(restored.g(), 0.5f, kColorEpsilon);
    EXPECT_NEAR(restored.b(), 0.75f, kColorEpsilon);
    EXPECT_NEAR(restored.a(), 0.125f, kColorEpsilon);

    const FloatRGBA rgba = toFloatRGBA(qt);
    EXPECT_NEAR(rgba.a(), 0.125f, kColorEpsilon);
}

TEST(ColorBridgeTest, ToQColorClampsOutOfRangeChannels)
{
    const FloatColor hot(1.5f, -0.25f, 0.5f, 2.0f);
    const QColor qt = toQColor(hot);
    EXPECT_EQ(qt.red(), 255);
    EXPECT_EQ(qt.green(), 0);
    EXPECT_EQ(qt.alpha(), 255);
}

TEST(ColorBridgeTest, JsonRoundTripForObjectAndHexStrings)
{
    const FloatColor color(0.1f, 0.2f, 0.3f, 0.4f);
    const QJsonObject json = colorToJson(color);

    const FloatColor parsed = floatColorFromJson(QJsonValue(json));
    EXPECT_NEAR(parsed.r(), 0.1f, kColorEpsilon);
    EXPECT_NEAR(parsed.g(), 0.2f, kColorEpsilon);
    EXPECT_NEAR(parsed.b(), 0.3f, kColorEpsilon);
    EXPECT_NEAR(parsed.a(), 0.4f, kColorEpsilon);

    const QString hex = colorToHexArgb(color);
    const FloatColor fromHex = floatColorFromJson(QJsonValue(hex));
    EXPECT_NEAR(fromHex.r(), 26.0f / 255.0f, kColorEpsilon);
    EXPECT_NEAR(fromHex.a(), 102.0f / 255.0f, kColorEpsilon);
}

TEST(ColorBridgeTest, HexArgbUsesAlphaRedGreenBlueOrder)
{
    const FloatColor color(64.0f / 255.0f, 128.0f / 255.0f,
                           191.0f / 255.0f, 32.0f / 255.0f);

    EXPECT_EQ(colorToHexArgb(color), QStringLiteral("#204080bf"));
}

TEST(ColorBridgeTest, JsonFallbackOnInvalidInput)
{
    const FloatColor fallback(0.9f, 0.8f, 0.7f, 0.6f);
    const auto parsed = floatColorFromJson(QJsonValue(QStringLiteral("not-a-color")), fallback);
    EXPECT_EQ(parsed, fallback);

    const auto rgba = floatRgbaFromJson(QJsonValue(42));
    EXPECT_FLOAT_EQ(rgba.a(), 1.0f);

    // Missing alpha defaults to opaque.
    QJsonObject noAlpha;
    noAlpha.insert(QStringLiteral("r"), 1.0);
    noAlpha.insert(QStringLiteral("g"), 0.5);
    noAlpha.insert(QStringLiteral("b"), 0.0);
    const FloatColor opaque = floatColorFromJson(QJsonValue(noAlpha));
    EXPECT_FLOAT_EQ(opaque.a(), 1.0f);
}

TEST(ColorBridgeTest, JsonObjectMissingRgbChannelUsesFallback)
{
    const FloatColor fallback(0.9f, 0.8f, 0.7f, 0.6f);
    QJsonObject incomplete;
    incomplete.insert(QStringLiteral("r"), 0.1);
    incomplete.insert(QStringLiteral("g"), 0.2);
    incomplete.insert(QStringLiteral("a"), 0.3);

    EXPECT_EQ(floatColorFromJson(QJsonValue(incomplete), fallback), fallback);
}

TEST(ColorBridgeTest, FloatRgbaUsesFallbackForIncompleteObject)
{
    const FloatRGBA fallback(0.15f, 0.25f, 0.35f, 0.45f);
    QJsonObject incomplete;
    incomplete.insert(QStringLiteral("r"), 0.9);
    incomplete.insert(QStringLiteral("b"), 0.1);

    EXPECT_EQ(floatRgbaFromJson(QJsonValue(incomplete), fallback), fallback);
}

TEST(ColorBridgeTest, FloatRgbaObjectWithoutAlphaDefaultsOpaque)
{
    QJsonObject noAlpha;
    noAlpha.insert(QStringLiteral("r"), 0.25);
    noAlpha.insert(QStringLiteral("g"), 0.5);
    noAlpha.insert(QStringLiteral("b"), 0.75);

    const FloatRGBA parsed = floatRgbaFromJson(QJsonValue(noAlpha));

    EXPECT_FLOAT_EQ(parsed.r(), 0.25f);
    EXPECT_FLOAT_EQ(parsed.g(), 0.5f);
    EXPECT_FLOAT_EQ(parsed.b(), 0.75f);
    EXPECT_FLOAT_EQ(parsed.a(), 1.0f);
}

TEST(ColorBridgeTest, FloatRgbaJsonRoundTripPreservesAllFourChannels)
{
    const FloatRGBA source(0.125f, 0.375f, 0.625f, 0.875f);
    const QJsonObject json = colorToJson(source);
    const FloatRGBA parsed = floatRgbaFromJson(QJsonValue(json));

    EXPECT_FLOAT_EQ(json.value(QStringLiteral("r")).toDouble(), source.r());
    EXPECT_FLOAT_EQ(json.value(QStringLiteral("g")).toDouble(), source.g());
    EXPECT_FLOAT_EQ(json.value(QStringLiteral("b")).toDouble(), source.b());
    EXPECT_FLOAT_EQ(json.value(QStringLiteral("a")).toDouble(), source.a());
    EXPECT_EQ(parsed, source);
}

TEST(ColorBridgeTest, JsonObjectAndHexInputsPreserveTheirDocumentedChannelRanges)
{
    constexpr float values[] = {-0.25f, 0.0f, 0.125f, 0.5f, 1.0f, 1.25f};
    for (const float red : values) {
        for (const float green : values) {
            for (const float blue : values) {
                QJsonObject object;
                object.insert(QStringLiteral("r"), red);
                object.insert(QStringLiteral("g"), green);
                object.insert(QStringLiteral("b"), blue);
                object.insert(QStringLiteral("a"), 0.375);

                const FloatColor parsedObject = floatColorFromJson(QJsonValue(object));
                EXPECT_FLOAT_EQ(parsedObject.r(), red);
                EXPECT_FLOAT_EQ(parsedObject.g(), green);
                EXPECT_FLOAT_EQ(parsedObject.b(), blue);
                EXPECT_FLOAT_EQ(parsedObject.a(), 0.375f);

                const QColor qt = toQColor(FloatColor(red, green, blue, 0.375f));
                const QString hex = qt.name(QColor::HexArgb);
                const FloatColor parsedHex = floatColorFromJson(QJsonValue(hex));
                constexpr float byteScale = 1.0f / 255.0f;
                EXPECT_NEAR(parsedHex.r(), qt.red() * byteScale, kColorEpsilon);
                EXPECT_NEAR(parsedHex.g(), qt.green() * byteScale, kColorEpsilon);
                EXPECT_NEAR(parsedHex.b(), qt.blue() * byteScale, kColorEpsilon);
                EXPECT_NEAR(parsedHex.a(), qt.alpha() * byteScale, kColorEpsilon);
            }
        }
    }
}

TEST(ColorBridgeTest, NonFiniteJsonChannelsAndMalformedHexUseTheProvidedFallback)
{
    const FloatColor colorFallback(0.17f, 0.29f, 0.43f, 0.61f);
    const FloatRGBA rgbaFallback(0.13f, 0.27f, 0.49f, 0.73f);
    constexpr const char* channelNames[] = {"r", "g", "b", "a"};

    for (const char* channelName : channelNames) {
        QJsonObject object;
        object.insert(QStringLiteral("r"), 0.1);
        object.insert(QStringLiteral("g"), 0.2);
        object.insert(QStringLiteral("b"), 0.3);
        object.insert(QStringLiteral("a"), 0.4);
        object.insert(QString::fromLatin1(channelName),
                      std::numeric_limits<double>::infinity());
        EXPECT_EQ(floatColorFromJson(QJsonValue(object), colorFallback), colorFallback);
        EXPECT_EQ(floatRgbaFromJson(QJsonValue(object), rgbaFallback), rgbaFallback);
    }

    constexpr const char* invalidHex[] = {
        "", "#", "#12345", "#1234567", "#gggggg", "red-ish",
    };
    for (const char* text : invalidHex) {
        EXPECT_EQ(floatColorFromJson(QJsonValue(QString::fromLatin1(text)), colorFallback),
                  colorFallback);
        EXPECT_EQ(floatRgbaFromJson(QJsonValue(QString::fromLatin1(text)), rgbaFallback),
                  rgbaFallback);
    }

    const QJsonValue nonColorValues[] = {
        QJsonValue(), QJsonValue(true), QJsonValue(1.0), QJsonValue(QJsonArray{}),
    };
    for (const QJsonValue& value : nonColorValues) {
        EXPECT_EQ(floatColorFromJson(value, colorFallback), colorFallback);
        EXPECT_EQ(floatRgbaFromJson(value, rgbaFallback), rgbaFallback);
    }
}

TEST(ColorBridgeTest, RgbHexStringDefaultsToOpaqueAndTrimsWhitespace)
{
    const FloatColor parsed = floatColorFromJson(QJsonValue(QStringLiteral("  #336699  ")));

    EXPECT_FLOAT_EQ(parsed.r(), 0x33 / 255.0f);
    EXPECT_FLOAT_EQ(parsed.g(), 0x66 / 255.0f);
    EXPECT_FLOAT_EQ(parsed.b(), 0x99 / 255.0f);
    EXPECT_FLOAT_EQ(parsed.a(), 1.0f);
}

TEST(ColorBridgeTest, InvalidQColorHasTypeSpecificFallback)
{
    const QColor invalid;
    const FloatColor floatColor = toFloatColor(invalid);
    const FloatRGBA floatRgba = toFloatRGBA(invalid);

    EXPECT_EQ(floatColor, FloatColor{});
    EXPECT_EQ(floatRgba, FloatRGBA(0.0f, 0.0f, 0.0f, 1.0f));
}

TEST(FloatRGBATest, ArithmeticOperatorsApplyToAllFourChannels)
{
    const FloatRGBA first(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatRGBA second(0.1f, 0.2f, 0.3f, 0.4f);

    const FloatRGBA sum = first + second;
    EXPECT_NEAR(sum.r(), 0.3f, 1e-6f);
    EXPECT_NEAR(sum.g(), 0.6f, 1e-6f);
    EXPECT_NEAR(sum.b(), 0.9f, 1e-6f);
    EXPECT_NEAR(sum.a(), 1.2f, 1e-6f);

    const FloatRGBA product = first * second;
    EXPECT_NEAR(product.r(), 0.02f, 1e-6f);
    EXPECT_NEAR(product.g(), 0.08f, 1e-6f);
    EXPECT_NEAR(product.b(), 0.18f, 1e-6f);
    EXPECT_NEAR(product.a(), 0.32f, 1e-6f);

    const FloatRGBA scaled = first * 0.5f;
    EXPECT_NEAR(scaled.r(), 0.1f, 1e-6f);
    EXPECT_NEAR(scaled.g(), 0.2f, 1e-6f);
    EXPECT_NEAR(scaled.b(), 0.3f, 1e-6f);
    EXPECT_NEAR(scaled.a(), 0.4f, 1e-6f);
}

TEST(FloatRGBATest, LerpInterpolatesAlphaAndClampsInterpolationParameter)
{
    const FloatRGBA first(0.0f, 0.2f, 0.4f, 0.1f);
    const FloatRGBA second(1.0f, 0.8f, 0.6f, 0.9f);

    const FloatRGBA midpoint = FloatRGBA::lerp(first, second, 0.5f);
    EXPECT_NEAR(midpoint.r(), 0.5f, 1e-6f);
    EXPECT_NEAR(midpoint.g(), 0.5f, 1e-6f);
    EXPECT_NEAR(midpoint.b(), 0.5f, 1e-6f);
    EXPECT_NEAR(midpoint.a(), 0.5f, 1e-6f);
    EXPECT_EQ(FloatRGBA::lerp(first, second, -1.0f), first);
    EXPECT_EQ(FloatRGBA::lerp(first, second, 2.0f), second);
}

TEST(FloatRGBATest, FloatColorConversionPreservesRgbaChannels)
{
    const FloatRGBA source(0.15f, 0.35f, 0.65f, 0.45f);
    const FloatColor converted = static_cast<FloatColor>(source);

    EXPECT_FLOAT_EQ(converted.r(), source.r());
    EXPECT_FLOAT_EQ(converted.g(), source.g());
    EXPECT_FLOAT_EQ(converted.b(), source.b());
    EXPECT_FLOAT_EQ(converted.a(), source.a());
}

TEST(FloatRGBATest, CompoundArithmeticAndSwapUpdateAllFourChannels)
{
    FloatRGBA value(0.8f, 0.6f, 0.4f, 0.2f);
    const FloatRGBA other(0.2f, 0.1f, 0.3f, 0.4f);
    const auto expectChannels = [](const FloatRGBA& actual, const FloatRGBA& expected) {
        EXPECT_NEAR(actual.r(), expected.r(), 1.0e-6f);
        EXPECT_NEAR(actual.g(), expected.g(), 1.0e-6f);
        EXPECT_NEAR(actual.b(), expected.b(), 1.0e-6f);
        EXPECT_NEAR(actual.a(), expected.a(), 1.0e-6f);
    };

    value += other;
    expectChannels(value, FloatRGBA(1.0f, 0.7f, 0.7f, 0.6f));
    value -= other;
    expectChannels(value, FloatRGBA(0.8f, 0.6f, 0.4f, 0.2f));
    value *= 0.5f;
    expectChannels(value, FloatRGBA(0.4f, 0.3f, 0.2f, 0.1f));
    value *= FloatRGBA(0.5f, 1.0f, 2.0f, 4.0f);
    expectChannels(value, FloatRGBA(0.2f, 0.3f, 0.4f, 0.4f));
    value /= 2.0f;
    expectChannels(value, FloatRGBA(0.1f, 0.15f, 0.2f, 0.2f));
    value /= FloatRGBA(0.5f, 0.5f, 2.0f, 4.0f);
    expectChannels(value, FloatRGBA(0.2f, 0.3f, 0.1f, 0.05f));

    FloatRGBA swapped(0.9f, 0.8f, 0.7f, 0.6f);
    value.swap(swapped);
    expectChannels(value, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));
    expectChannels(swapped, FloatRGBA(0.2f, 0.3f, 0.1f, 0.05f));
}

TEST(FloatRGBATest, IndexingAcceptsFourChannelsAndRejectsOutOfRangeIndices)
{
    FloatRGBA value;
    for (int index = 0; index < 4; ++index) {
        value[index] = static_cast<float>(index + 1) * 0.1f;
    }

    EXPECT_FLOAT_EQ(value[0], 0.1f);
    EXPECT_FLOAT_EQ(value[1], 0.2f);
    EXPECT_FLOAT_EQ(value[2], 0.3f);
    EXPECT_FLOAT_EQ(value[3], 0.4f);

    const FloatRGBA& readOnly = value;
    EXPECT_FLOAT_EQ(readOnly[0], 0.1f);
    EXPECT_FLOAT_EQ(readOnly[3], 0.4f);
    EXPECT_THROW((void)value[-1], std::out_of_range);
    EXPECT_THROW((void)value[4], std::out_of_range);
    EXPECT_THROW((void)readOnly[-1], std::out_of_range);
    EXPECT_THROW((void)readOnly[4], std::out_of_range);
}

TEST(FloatRGBATest, DefaultAndRgbConstructorsUseDifferentAlphaDefaults)
{
    const FloatRGBA zeroInitialized;
    const FloatRGBA rgb(0.2f, 0.4f, 0.6f);

    EXPECT_EQ(zeroInitialized, FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(rgb.r(), 0.2f);
    EXPECT_FLOAT_EQ(rgb.g(), 0.4f);
    EXPECT_FLOAT_EQ(rgb.b(), 0.6f);
    EXPECT_FLOAT_EQ(rgb.a(), 1.0f);
}

TEST(TaggedColorTest, TransferConversionMatchesCoreMath)
{
    const auto tagged = TaggedColor::srgbEncoded(0.5f, 0.25f, 0.75f, 1.0f);
    const auto linear = tagged.toTransfer(TransferFunction::Linear);

    EXPECT_EQ(linear.transfer, TransferFunction::Linear);
    EXPECT_FLOAT_EQ(linear.rgba.r(),
                    ColorTransferFunction::srgbToLinear(0.5f));
    EXPECT_FLOAT_EQ(linear.rgba.g(),
                    ColorTransferFunction::srgbToLinear(0.25f));

    const auto back = linear.toTransfer(TransferFunction::sRGB);
    EXPECT_EQ(back.transfer, TransferFunction::sRGB);
    EXPECT_NEAR(back.rgba.r(), 0.5f, 1e-5f);
    EXPECT_NEAR(back.rgba.g(), 0.25f, 1e-5f);
}

TEST(TaggedColorTest, DispatchedTransferCurvesPreserveMetadataAndAlphaOnRoundTrip)
{
    constexpr TransferFunction transfers[] = {
        TransferFunction::Linear,
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::SonySLog3,
        TransferFunction::Cineon,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    const TaggedColor source = TaggedColor::srgbEncoded(0.41f, 0.23f, 0.12f, 0.37f);

    for (const TransferFunction transfer : transfers) {
        const TaggedColor converted = source.toTransfer(transfer);
        SCOPED_TRACE(static_cast<int>(transfer));
        EXPECT_EQ(converted.transfer, transfer);
        EXPECT_TRUE(converted.transferKnown);
        EXPECT_EQ(converted.primaries, source.primaries);
        EXPECT_EQ(converted.alphaMode, source.alphaMode);
        EXPECT_FLOAT_EQ(converted.rgba.a(), source.rgba.a());

        const TaggedColor restored = converted.toTransfer(TransferFunction::sRGB);
        EXPECT_EQ(restored.transfer, TransferFunction::sRGB);
        EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 5.0e-4f);
        EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 5.0e-4f);
        EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 5.0e-4f);
        EXPECT_FLOAT_EQ(restored.rgba.a(), source.rgba.a());
    }
}

TEST(TaggedColorTest, TransferConversionPreservesAlphaAndPrimariesMetadata)
{
    constexpr SurfaceAlphaMode alphaModes[] = {
        SurfaceAlphaMode::Straight,
        SurfaceAlphaMode::Premultiplied,
        SurfaceAlphaMode::Opaque,
    };
    for (const SurfaceAlphaMode alphaMode : alphaModes) {
        TaggedColor source = TaggedColor::srgbEncoded(0.41f, 0.23f, 0.12f, 0.37f);
        source.alphaMode = alphaMode;
        source.primaries = SurfaceColorPrimaries::Rec2020_D65;

        const TaggedColor converted = source.toTransfer(TransferFunction::Gamma22);
        SCOPED_TRACE(static_cast<int>(alphaMode));
        EXPECT_EQ(converted.transfer, TransferFunction::Gamma22);
        EXPECT_TRUE(converted.transferKnown);
        EXPECT_EQ(converted.alphaMode, alphaMode);
        EXPECT_EQ(converted.primaries, source.primaries);
        EXPECT_FLOAT_EQ(converted.rgba.a(), source.rgba.a());
        EXPECT_NEAR(converted.rgba.r(),
                    ColorTransferFunction::encode(
                        ColorTransferFunction::decode(source.rgba.r(), source.transfer),
                        TransferFunction::Gamma22), 1e-7f);
        EXPECT_NEAR(converted.rgba.g(),
                    ColorTransferFunction::encode(
                        ColorTransferFunction::decode(source.rgba.g(), source.transfer),
                        TransferFunction::Gamma22), 1e-7f);
        EXPECT_NEAR(converted.rgba.b(),
                    ColorTransferFunction::encode(
                        ColorTransferFunction::decode(source.rgba.b(), source.transfer),
                        TransferFunction::Gamma22), 1e-7f);

        const TaggedColor restored = converted.toTransfer(source.transfer);
        EXPECT_EQ(restored.transfer, source.transfer);
        EXPECT_EQ(restored.alphaMode, source.alphaMode);
        EXPECT_EQ(restored.primaries, source.primaries);
        EXPECT_FLOAT_EQ(restored.rgba.a(), source.rgba.a());
        EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 2e-6f);
        EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 2e-6f);
        EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 2e-6f);
    }
}

TEST(ColorTransferFunctionTest, SRGBPiecewiseThresholdsRemainContinuous)
{
    constexpr float linearThreshold = 0.0031308f;
    constexpr float encodedThreshold = 0.04045f;

    const float encodedAtThreshold =
        ColorTransferFunction::linearToSRGB(linearThreshold);
    const float linearAtThreshold =
        ColorTransferFunction::srgbToLinear(encodedThreshold);

    EXPECT_NEAR(encodedAtThreshold, 12.92f * linearThreshold, 1e-7f);
    EXPECT_NEAR(linearAtThreshold, encodedThreshold / 12.92f, 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::srgbToLinear(encodedAtThreshold),
                linearThreshold, 2e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToSRGB(linearAtThreshold),
                encodedThreshold, 2e-7f);
}

TEST(ColorTransferFunctionTest, SRGBIsMonotonicAndInvertibleAcrossDenseUnitGrids)
{
    constexpr int subdivisions = 4096;
    float previousEncoded = -1.0f;
    float previousDecoded = -1.0f;

    for (int index = 0; index <= subdivisions; ++index) {
        const float linear = index / static_cast<float>(subdivisions);
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::sRGB);
        const float decoded = ColorTransferFunction::decode(
            encoded, TransferFunction::sRGB);
        SCOPED_TRACE(::testing::Message()
            << "linearIndex=" << index << " linear=" << linear);
        ASSERT_TRUE(std::isfinite(encoded));
        ASSERT_TRUE(std::isfinite(decoded));
        EXPECT_GE(encoded, previousEncoded);
        EXPECT_GE(decoded, previousDecoded);
        EXPECT_NEAR(decoded, linear, 2.0e-6f);
        previousEncoded = encoded;
        previousDecoded = decoded;
    }

    previousDecoded = -1.0f;
    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::sRGB);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::sRGB);
        SCOPED_TRACE(::testing::Message()
            << "codeIndex=" << index << " code=" << code);
        ASSERT_TRUE(std::isfinite(linear));
        EXPECT_GE(linear, previousDecoded);
        EXPECT_NEAR(restoredCode, code, 2.0e-6f);
        previousDecoded = linear;
    }
}

TEST(ColorTransferFunctionTest, DispatchedTransferCurvesRoundTripRepresentativeLinearValues)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::SonySLog3,
        TransferFunction::Cineon,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    constexpr float samples[] = {0.001f, 0.01f, 0.18f, 0.5f, 1.0f};

    for (const TransferFunction curve : curves) {
        for (const float linear : samples) {
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " linear=" << linear);
            const float encoded = ColorTransferFunction::encode(linear, curve);
            ASSERT_TRUE(std::isfinite(encoded));
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, linear, 3e-4f);
        }
    }
}

TEST(ColorTransferFunctionTest, TransferDispatchMatchesEachNamedCurveImplementation)
{
    using Curve = float (*)(float);
    struct CurveCase {
        TransferFunction transfer;
        Curve encode;
        Curve decode;
    };
    const CurveCase curves[] = {
        {TransferFunction::Linear,
         [](float value) { return value; }, [](float value) { return value; }},
        {TransferFunction::sRGB, ColorTransferFunction::linearToSRGB,
         ColorTransferFunction::srgbToLinear},
        {TransferFunction::Gamma22,
         [](float value) { return ColorTransferFunction::linearToGamma(value, 2.2f); },
         [](float value) { return ColorTransferFunction::gammaToLinear(value, 2.2f); }},
        {TransferFunction::Gamma24,
         [](float value) { return ColorTransferFunction::linearToGamma(value, 2.4f); },
         [](float value) { return ColorTransferFunction::gammaToLinear(value, 2.4f); }},
        {TransferFunction::Gamma26,
         [](float value) { return ColorTransferFunction::linearToGamma(value, 2.6f); },
         [](float value) { return ColorTransferFunction::gammaToLinear(value, 2.6f); }},
        {TransferFunction::Rec709, ColorTransferFunction::linearToRec709,
         ColorTransferFunction::rec709ToLinear},
        {TransferFunction::Rec2020_10, ColorTransferFunction::linearToRec2020,
         ColorTransferFunction::rec2020ToLinear},
        {TransferFunction::Rec2084_PQ, ColorTransferFunction::linearToPQ,
         ColorTransferFunction::pqToLinear},
        {TransferFunction::HLG, ColorTransferFunction::linearToHLG,
         ColorTransferFunction::hlgToLinear},
        {TransferFunction::ACEScc, ColorTransferFunction::linearToACEScc,
         ColorTransferFunction::acesccToLinear},
        {TransferFunction::ACEScct, ColorTransferFunction::linearToACEScct,
         ColorTransferFunction::acescctToLinear},
        {TransferFunction::DaVinciIntermediate,
         ColorTransferFunction::linearToDaVinciIntermediate,
         ColorTransferFunction::daVinciIntermediateToLinear},
        {TransferFunction::SonySLog3, ColorTransferFunction::linearToSLog3,
         ColorTransferFunction::sLog3ToLinear},
        {TransferFunction::Cineon, ColorTransferFunction::linearToCineon,
         ColorTransferFunction::cineonToLinear},
        {TransferFunction::CanonLog2, ColorTransferFunction::linearToCanonLog2,
         ColorTransferFunction::canonLog2ToLinear},
        {TransferFunction::CanonLog3, ColorTransferFunction::linearToCanonLog3,
         ColorTransferFunction::canonLog3ToLinear},
    };
    constexpr float linearSamples[] = {-0.01f, 0.0f, 0.001f, 0.018f, 0.18f, 1.0f, 4.0f};
    constexpr float encodedSamples[] = {-0.01f, 0.0f, 0.02f, 0.18f, 0.5f, 1.0f, 1.25f};

    for (const CurveCase& curve : curves) {
        for (const float linear : linearSamples) {
            EXPECT_NEAR(ColorTransferFunction::encode(linear, curve.transfer),
                        curve.encode(linear), 1e-7f)
                << "encode transfer=" << static_cast<int>(curve.transfer)
                << " linear=" << linear;
        }
        for (const float encoded : encodedSamples) {
            EXPECT_NEAR(ColorTransferFunction::decode(encoded, curve.transfer),
                        curve.decode(encoded), 1e-7f)
                << "decode transfer=" << static_cast<int>(curve.transfer)
                << " encoded=" << encoded;
        }
    }
}

TEST(ColorTransferFunctionTest, DisplayAndVideoCurvesRoundTripHdrHeadroom)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::HLG,
    };
    constexpr float linearSamples[] = {1.0f, 1.25f, 2.0f, 4.0f};

    for (const TransferFunction curve : curves) {
        for (const float linear : linearSamples) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " linear=" << linear << " encoded=" << encoded);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, linear, 2e-4f);
        }
    }
}

TEST(ColorTransferFunctionTest, PqAndHlgMatchPublishedReferenceValues)
{
    EXPECT_NEAR(ColorTransferFunction::linearToPQ(0.01f), 0.5080784f, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::linearToPQ(0.18f), 0.8159435f, 2e-6f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::linearToPQ(1.0f), 1.0f);
    EXPECT_NEAR(ColorTransferFunction::pqToLinear(0.5f), 0.00922437f, 2e-7f);

    EXPECT_NEAR(ColorTransferFunction::linearToHLG(0.5f), 0.8716435f, 2e-6f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::linearToHLG(1.0f), 1.0f);
    EXPECT_NEAR(ColorTransferFunction::hlgToLinear(0.5f), 1.0f / 12.0f, 1e-7f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::hlgToLinear(1.0f), 1.0f);
}

TEST(ColorTransferFunctionTest, PqAndHlgPinBlackAndWhiteEndpoints)
{
    for (const float nonPositive : {-1.0f, 0.0f}) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::linearToPQ(nonPositive), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::pqToLinear(nonPositive), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::linearToHLG(nonPositive), 0.0f);
        EXPECT_FLOAT_EQ(ColorTransferFunction::hlgToLinear(nonPositive), 0.0f);
    }

    EXPECT_FLOAT_EQ(ColorTransferFunction::linearToPQ(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::pqToLinear(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::linearToHLG(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ColorTransferFunction::hlgToLinear(1.0f), 1.0f);
}

TEST(ColorTransferFunctionTest, PqAndHlgAreMonotonicAndRoundTripDenseNormalizedGrid)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
    };
    constexpr int subdivisions = 1024;

    for (const TransferFunction curve : curves) {
        float previousEncoded = -1.0f;
        float previousDecoded = -1.0f;
        for (int index = 0; index <= subdivisions; ++index) {
            const float linear = index / static_cast<float>(subdivisions);
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve) << " index=" << index
                << " linear=" << linear);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_GE(encoded, previousEncoded);
            EXPECT_GE(decoded, previousDecoded);
            EXPECT_NEAR(decoded, linear, 1e-4f);
            previousEncoded = encoded;
            previousDecoded = decoded;
        }
    }
}

TEST(ColorTransferFunctionTest, HlgEncodeAndDecodeMatchPiecewiseReferencesAtAdjacentFloats)
{
    constexpr float sceneLinearBoundary = 1.0f / 12.0f;
    constexpr float encodedBoundary = 0.5f;
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    const float belowLinear = std::nextafter(sceneLinearBoundary, 0.0f);
    const float aboveLinear = std::nextafter(sceneLinearBoundary, 1.0f);
    const float belowEncoded = std::nextafter(encodedBoundary, 0.0f);
    const float aboveEncoded = std::nextafter(encodedBoundary, 1.0f);

    EXPECT_NEAR(ColorTransferFunction::linearToHLG(belowLinear),
                std::sqrt(3.0f * belowLinear), 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToHLG(sceneLinearBoundary),
                std::sqrt(3.0f * sceneLinearBoundary), 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToHLG(aboveLinear),
                a * std::log(12.0f * aboveLinear - b) + c, 1e-7f);

    EXPECT_NEAR(ColorTransferFunction::hlgToLinear(belowEncoded),
                (belowEncoded * belowEncoded) / 3.0f, 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::hlgToLinear(encodedBoundary),
                (encodedBoundary * encodedBoundary) / 3.0f, 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::hlgToLinear(aboveEncoded),
                (std::exp((aboveEncoded - c) / a) + b) / 12.0f, 1e-7f);
}

TEST(ColorTransferFunctionTest, Rec709AndRec2020DecodeDenseNormalizedCodeGrid)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
    };
    constexpr int subdivisions = 4096;

    for (const TransferFunction curve : curves) {
        float previousLinear = -std::numeric_limits<float>::infinity();
        float previousCode = -1.0f;
        const float breakpoint = curve == TransferFunction::Rec709
            ? 0.081f : 0.081242858298635f;
        for (int index = 0; index <= subdivisions; ++index) {
            const float code = index / static_cast<float>(subdivisions);
            const float linear = ColorTransferFunction::decode(code, curve);
            const float restoredCode = ColorTransferFunction::encode(linear, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " index=" << index << " code=" << code);
            ASSERT_TRUE(std::isfinite(linear));
            if (previousCode < breakpoint && code >= breakpoint) {
                previousLinear = -std::numeric_limits<float>::infinity();
            }
            EXPECT_GE(linear, previousLinear);
            EXPECT_NEAR(restoredCode, code, 4.0e-4f);
            previousLinear = linear;
            previousCode = code;
        }
    }

    constexpr float rec709Breakpoint = 0.081f;
    const float rec709Below = ColorTransferFunction::decode(
        std::nextafter(rec709Breakpoint, 0.0f), TransferFunction::Rec709);
    const float rec709At = ColorTransferFunction::decode(
        rec709Breakpoint, TransferFunction::Rec709);
    EXPECT_LT(rec709At, rec709Below);
    EXPECT_LT(rec709Below - rec709At, 1.0e-4f);

    constexpr float rec2020Breakpoint = 0.081242858298635f;
    const float rec2020Below = ColorTransferFunction::decode(
        std::nextafter(rec2020Breakpoint, 0.0f), TransferFunction::Rec2020_10);
    const float rec2020At = ColorTransferFunction::decode(
        rec2020Breakpoint, TransferFunction::Rec2020_10);
    EXPECT_NEAR(rec2020At, rec2020Below, 2.0e-7f);
}

TEST(ColorTransferFunctionTest, Rec709AndRec2020EncodeDenseLinearAndHdrGrids)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
    };
    constexpr int unitSubdivisions = 4096;
    constexpr int hdrSubdivisions = 1024;

    for (const TransferFunction curve : curves) {
        float previousEncoded = -1.0f;
        for (int index = 0; index <= unitSubdivisions; ++index) {
            const float linear = index / static_cast<float>(unitSubdivisions);
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float restored = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " unitIndex=" << index << " linear=" << linear);
            ASSERT_TRUE(std::isfinite(encoded));
            EXPECT_GE(encoded, previousEncoded);
            EXPECT_NEAR(restored, linear, 2.0e-6f);
            previousEncoded = encoded;
        }

        previousEncoded = 0.0f;
        for (int index = 0; index <= hdrSubdivisions; ++index) {
            const float linear = 1.0f + 15.0f * index / hdrSubdivisions;
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float restored = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " hdrIndex=" << index << " linear=" << linear);
            ASSERT_TRUE(std::isfinite(encoded));
            EXPECT_GE(encoded, previousEncoded);
            EXPECT_NEAR(restored, linear, 2.0e-5f * linear);
            previousEncoded = encoded;
        }
    }
}

TEST(ColorTransferFunctionTest, SimpleGammaCurvesAreMonotonicAndInvertDenseUnitGrids)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
    };
    constexpr int subdivisions = 2048;

    for (const TransferFunction curve : curves) {
        float previousEncoded = -1.0f;
        float previousDecoded = -1.0f;
        for (int index = 0; index <= subdivisions; ++index) {
            const float linear = index / static_cast<float>(subdivisions);
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " index=" << index << " linear=" << linear);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_GE(encoded, previousEncoded);
            EXPECT_GE(decoded, previousDecoded);
            EXPECT_NEAR(decoded, linear, 2.0e-6f);
            previousEncoded = encoded;
            previousDecoded = decoded;
        }

        float previousLinear = -1.0f;
        for (int index = 0; index <= subdivisions; ++index) {
            const float code = index / static_cast<float>(subdivisions);
            const float linear = ColorTransferFunction::decode(code, curve);
            const float restoredCode = ColorTransferFunction::encode(linear, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " decodeIndex=" << index << " code=" << code);
            ASSERT_TRUE(std::isfinite(linear));
            EXPECT_GE(linear, previousLinear);
            EXPECT_NEAR(restoredCode, code, 2.0e-6f);
            previousLinear = linear;
        }
    }
}

TEST(ColorTransferFunctionTest, SimpleGammaCurvesClampNegativeValuesAndKeepHdrHeadroom)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
    };
    constexpr float negativeValues[] = {-16.0f, -1.0f, -1.0e-6f, 0.0f};
    constexpr float hdrValues[] = {1.0f, 1.25f, 4.0f, 16.0f};

    for (const TransferFunction curve : curves) {
        for (const float value : negativeValues) {
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve) << " negative=" << value);
            EXPECT_FLOAT_EQ(ColorTransferFunction::encode(value, curve), 0.0f);
            EXPECT_FLOAT_EQ(ColorTransferFunction::decode(value, curve), 0.0f);
        }

        for (const float linear : hdrValues) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float restored = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve) << " hdr=" << linear);
            EXPECT_TRUE(std::isfinite(encoded));
            if (linear > 1.0f) {
                EXPECT_GT(encoded, 1.0f);
            } else {
                EXPECT_FLOAT_EQ(encoded, 1.0f);
            }
            EXPECT_NEAR(restored, linear, 2.0e-5f * linear);
        }
    }
}

TEST(ColorTransferFunctionTest, UnknownTransferDispatchFallsBackToIdentity)
{
    const auto unknown = static_cast<TransferFunction>(-1);
    constexpr float samples[] = {-0.25f, 0.0f, 0.18f, 1.5f};

    for (const float sample : samples) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(sample, unknown), sample);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(sample, unknown), sample);
    }
}

TEST(ColorTransferFunctionTest, DeclaredAceslogTransferCurrentlyFallsBackToIdentity)
{
    constexpr float samples[] = {-0.25f, 0.0f, 0.18f, 1.0f, 1.5f};
    for (const float sample : samples) {
        EXPECT_FLOAT_EQ(ColorTransferFunction::encode(sample, TransferFunction::ACESlog),
                        sample);
        EXPECT_FLOAT_EQ(ColorTransferFunction::decode(sample, TransferFunction::ACESlog),
                        sample);
    }
}

TEST(ColorTransferFunctionTest, LogAndCameraCurvesStayFiniteAndOrderedAcrossSceneAndHdrRange)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::SonySLog3,
        TransferFunction::Cineon,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    constexpr float linearSamples[] = {0.001f, 0.01f, 0.18f, 1.0f, 4.0f};

    for (const TransferFunction curve : curves) {
        const bool decreasing = curve == TransferFunction::ACEScc;
        float previous = decreasing
            ? std::numeric_limits<float>::infinity()
            : -std::numeric_limits<float>::infinity();
        for (const float linear : linearSamples) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve) << " linear=" << linear);
            ASSERT_TRUE(std::isfinite(encoded));
            if (decreasing) {
                EXPECT_LE(encoded, previous);
            } else {
                EXPECT_GE(encoded, previous);
            }
            previous = encoded;
        }
    }
}

TEST(ColorTransferFunctionTest, AcesCcIsFiniteAndNonIncreasingAcrossDensePositiveHdrGrid)
{
    constexpr int subdivisions = 2048;
    constexpr float minimumLinear = 0.001f;
    constexpr float maximumLinear = 16.0f;
    float previous = std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float linear = minimumLinear +
            (maximumLinear - minimumLinear) * index / subdivisions;
        const float encoded = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScc);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " linear=" << linear);
        ASSERT_TRUE(std::isfinite(encoded));
        EXPECT_LE(encoded, previous);
        previous = encoded;
    }
}

TEST(ColorTransferFunctionTest, AcesCcDecodeIsFiniteAndNonIncreasingAcrossNormalizedCodeGrid)
{
    constexpr int subdivisions = 2048;
    float previous = std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::ACEScc);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScc);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " code=" << code);
        ASSERT_TRUE(std::isfinite(linear));
        EXPECT_LE(linear, previous);
        EXPECT_NEAR(restoredCode, code, 2e-6f);
        previous = linear;
    }
}

TEST(ColorTransferFunctionTest, AcesCctDecodeIsFiniteAndNonDecreasingAcrossNormalizedCodeGrid)
{
    constexpr int subdivisions = 2048;
    float previous = -std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::ACEScct);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::ACEScct);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " code=" << code);
        ASSERT_TRUE(std::isfinite(linear));
        EXPECT_GE(linear, previous);
        EXPECT_NEAR(restoredCode, code, 2e-6f);
        previous = linear;
    }
}

TEST(ColorTransferFunctionTest, DaVinciIntermediateDecodeIsFiniteAndOrderedAcrossNormalizedCodeGrid)
{
    constexpr int subdivisions = 2048;
    float previous = -std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::DaVinciIntermediate);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::DaVinciIntermediate);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " code=" << code);
        ASSERT_TRUE(std::isfinite(linear));
        EXPECT_GE(linear, previous);
        EXPECT_NEAR(restoredCode, code, 2e-6f);
        previous = linear;
    }
}

TEST(ColorTransferFunctionTest, CineonDecodeIsFiniteAndOrderedAcrossNormalizedCodeGrid)
{
    constexpr int subdivisions = 2048;
    const float blackCode = ColorTransferFunction::encode(
        0.0f, TransferFunction::Cineon);
    float previous = -std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::Cineon);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::Cineon);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " code=" << code
            << " blackCode=" << blackCode);
        ASSERT_TRUE(std::isfinite(linear));
        EXPECT_GE(linear, previous);
        if (code < blackCode) {
            EXPECT_LT(linear, 0.0f);
            EXPECT_FLOAT_EQ(restoredCode, blackCode);
        } else {
            EXPECT_NEAR(restoredCode, code, 2e-6f);
        }
        previous = linear;
    }
}

TEST(ColorTransferFunctionTest, SLog3DecodeIsFiniteAndOrderedAcrossNormalizedCodeGrid)
{
    constexpr int subdivisions = 2048;
    const float blackCode = ColorTransferFunction::encode(
        0.0f, TransferFunction::SonySLog3);
    float previous = -std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::SonySLog3);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::SonySLog3);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " code=" << code
            << " blackCode=" << blackCode);
        ASSERT_TRUE(std::isfinite(linear));
        EXPECT_GE(linear, previous);
        if (code < blackCode) {
            EXPECT_LT(linear, 0.0f);
            EXPECT_FLOAT_EQ(restoredCode, blackCode);
        } else {
            EXPECT_NEAR(restoredCode, code, 2e-6f);
        }
        previous = linear;
    }
}

TEST(ColorTransferFunctionTest, CanonLog3DecodeGridCharacterizesEachPiecewiseRegion)
{
    constexpr int subdivisions = 2048;
    constexpr float lowCode = 0.04076162f;
    constexpr float highCode = 0.105357102f;
    float previousLow = -std::numeric_limits<float>::infinity();
    float previousLinear = -std::numeric_limits<float>::infinity();
    float previousHigh = -std::numeric_limits<float>::infinity();

    for (int index = 0; index <= subdivisions; ++index) {
        const float code = index / static_cast<float>(subdivisions);
        const float linear = ColorTransferFunction::decode(
            code, TransferFunction::CanonLog3);
        const float restoredCode = ColorTransferFunction::encode(
            linear, TransferFunction::CanonLog3);
        SCOPED_TRACE(::testing::Message()
            << "index=" << index << " code=" << code);
        ASSERT_TRUE(std::isfinite(linear));
        if (code < lowCode) {
            EXPECT_GE(linear, previousLow);
            EXPECT_NEAR(restoredCode, code, 2e-6f);
            previousLow = linear;
        } else if (code <= highCode) {
            EXPECT_GE(linear, previousLinear);
            previousLinear = linear;
        } else {
            EXPECT_GE(linear, previousHigh);
            EXPECT_NEAR(restoredCode, code, 2e-6f);
            previousHigh = linear;
        }
    }

    const float belowLow = ColorTransferFunction::decode(
        std::nextafter(lowCode, 0.0f), TransferFunction::CanonLog3);
    const float atLow = ColorTransferFunction::decode(
        lowCode, TransferFunction::CanonLog3);
    EXPECT_LT(atLow, belowLow);
}

TEST(ColorTransferFunctionTest, CanonLog3DecodeConnectsAtLinearCodeBoundaries)
{
    constexpr float lowCode = 0.04076162f;
    constexpr float highCode = 0.105357102f;
    constexpr float toe = 0.069886632f;
    constexpr float slope = 0.42889912f;
    constexpr float scale = 14.98325f;
    const float lowLinear =
        -(std::pow(10.0f, (toe - lowCode) / slope) - 1.0f) / scale;
    const float highLinear = (highCode - 0.073059361f) / 2.3069815f;

    const float lowBelow = ColorTransferFunction::encode(
        std::nextafter(lowLinear, -std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    const float lowAt = ColorTransferFunction::encode(
        lowLinear, TransferFunction::CanonLog3);
    const float lowAbove = ColorTransferFunction::encode(
        std::nextafter(lowLinear, std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    EXPECT_NEAR(lowBelow, lowCode, 2e-7f);
    EXPECT_NEAR(lowAt, 0.0470002f, 2e-7f);
    EXPECT_NEAR(lowAbove, lowAt, 2e-6f);

    const float highBelow = ColorTransferFunction::encode(
        std::nextafter(highLinear, -std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    const float highAt = ColorTransferFunction::encode(
        highLinear, TransferFunction::CanonLog3);
    const float highAbove = ColorTransferFunction::encode(
        std::nextafter(highLinear, std::numeric_limits<float>::infinity()),
        TransferFunction::CanonLog3);
    EXPECT_NEAR(highBelow, highCode, 2e-7f);
    EXPECT_NEAR(highAt, highCode, 2e-7f);
    EXPECT_NEAR(highAbove, highAt, 2e-6f);
}

TEST(ColorTransferFunctionTest, LogAndCameraCurvesRoundTripHdrHeadroom)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::SonySLog3,
        TransferFunction::Cineon,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    constexpr float linearSamples[] = {1.0f, 2.0f, 4.0f, 16.0f};

    for (const TransferFunction curve : curves) {
        for (const float linear : linearSamples) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve) << " linear=" << linear
                << " encoded=" << encoded);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, linear, 2e-3f);
        }
    }
}

TEST(ColorTransferFunctionTest, LogAndCameraCurvesRoundTripDenseSceneAndHdrGrid)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::SonySLog3,
        TransferFunction::Cineon,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };

    for (const TransferFunction curve : curves) {
        const auto checkRoundTrip = [curve](const float linear) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " linear=" << linear << " encoded=" << encoded);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            if (linear == 0.0f) {
                // Some log curves use a finite black code that is not an exact
                // encode/decode inverse; see the transfer-curve Insight entry.
                return;
            }
            EXPECT_NEAR(decoded, linear, 2.0e-5f * std::max(1.0f, linear));
        };

        checkRoundTrip(0.0f);
        for (int quarterStop = -64; quarterStop <= 24; ++quarterStop) {
            checkRoundTrip(std::exp2(quarterStop / 4.0f));
        }
    }
}

TEST(ColorTransferFunctionTest, SignedSceneLinearToeCurvesRoundTripNegativeValues)
{
    constexpr TransferFunction curves[] = {
        TransferFunction::ACEScct,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    constexpr float samples[] = {-0.0001f, 0.0f, 0.001f};

    for (const TransferFunction curve : curves) {
        for (const float linear : samples) {
            const float encoded = ColorTransferFunction::encode(linear, curve);
            const float decoded = ColorTransferFunction::decode(encoded, curve);
            SCOPED_TRACE(::testing::Message()
                << "curve=" << static_cast<int>(curve)
                << " linear=" << linear << " encoded=" << encoded);
            ASSERT_TRUE(std::isfinite(encoded));
            ASSERT_TRUE(std::isfinite(decoded));
            EXPECT_NEAR(decoded, linear, 2e-6f);
        }
    }
}

TEST(ColorTransferFunctionTest, LogAndCameraCurvesMatchSceneAndHdrReferenceCodes)
{
    struct ReferenceCase {
        TransferFunction curve;
        float linear;
        float encoded;
    };
    constexpr ReferenceCase references[] = {
        {TransferFunction::ACEScct, 0.18f, 0.4135884f},
        {TransferFunction::ACEScct, 1.0f, 0.5547945f},
        {TransferFunction::ACEScct, 4.0f, 0.6689498f},
        {TransferFunction::DaVinciIntermediate, 0.18f, 0.7751680f},
        {TransferFunction::DaVinciIntermediate, 1.0f, 0.9669392f},
        {TransferFunction::DaVinciIntermediate, 4.0f, 1.1219728f},
        {TransferFunction::SonySLog3, 0.18f, 0.4105572f},
        {TransferFunction::SonySLog3, 1.0f, 0.5960273f},
        {TransferFunction::SonySLog3, 4.0f, 0.7490989f},
        {TransferFunction::Cineon, 0.18f, 0.4573196f},
        {TransferFunction::Cineon, 1.0f, 0.6695992f},
        {TransferFunction::Cineon, 4.0f, 0.8451208f},
        {TransferFunction::CanonLog2, 0.18f, 0.3798646f},
        {TransferFunction::CanonLog2, 1.0f, 0.5836042f},
        {TransferFunction::CanonLog2, 4.0f, 0.7522561f},
        {TransferFunction::CanonLog3, 0.18f, 0.3134360f},
        {TransferFunction::CanonLog3, 1.0f, 0.5861375f},
        {TransferFunction::CanonLog3, 4.0f, 0.8354083f},
    };

    for (const ReferenceCase& testCase : references) {
        SCOPED_TRACE(::testing::Message()
            << "curve=" << static_cast<int>(testCase.curve)
            << " linear=" << testCase.linear);
        EXPECT_NEAR(ColorTransferFunction::encode(testCase.linear, testCase.curve),
                    testCase.encoded, 2e-6f);
    }
}

TEST(ColorTransferFunctionTest, Rec709AndRec2020UseTheirDistinctLinearToeSlopes)
{
    constexpr float rec709Breakpoint = 0.018f;
    constexpr float rec2020Breakpoint = 0.018053968510807f;
    constexpr float rec2020Alpha = 1.09929682680944f;

    EXPECT_NEAR(ColorTransferFunction::linearToRec709(rec709Breakpoint - 1e-6f),
                4.5f * (rec709Breakpoint - 1e-6f), 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToRec709(rec709Breakpoint),
                1.099f * std::pow(rec709Breakpoint, 0.45f) - 0.099f, 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToRec2020(rec2020Breakpoint - 1e-6f),
                4.5f * (rec2020Breakpoint - 1e-6f), 1e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToRec2020(rec2020Breakpoint),
                rec2020Alpha * std::pow(rec2020Breakpoint, 0.45f) -
                    (rec2020Alpha - 1.0f), 1e-7f);
}

TEST(ColorTransferFunctionTest, AcesCctSLog3AndCanonLog3PiecewiseBreakpointsAreCharacterized)
{
    constexpr float acescctBreakpoint = 0.0078125f;
    const float acescctBelow = std::nextafter(acescctBreakpoint, 0.0f);
    const float acescctAbove = std::nextafter(acescctBreakpoint, 1.0f);
    const float acescctAtValue = ColorTransferFunction::linearToACEScct(acescctBreakpoint);
    EXPECT_NEAR(ColorTransferFunction::linearToACEScct(acescctBelow),
                acescctAtValue, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::linearToACEScct(acescctAbove),
                acescctAtValue, 2e-6f);

    constexpr float slog3Breakpoint = 0.01125000f;
    const float slog3Below = std::nextafter(slog3Breakpoint, 0.0f);
    const float slog3Above = std::nextafter(slog3Breakpoint, 1.0f);
    const float slog3AtValue = ColorTransferFunction::linearToSLog3(slog3Breakpoint);
    EXPECT_NEAR(ColorTransferFunction::linearToSLog3(slog3Below), slog3AtValue, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::linearToSLog3(slog3Above), slog3AtValue, 2e-6f);

    constexpr float canonLow = 0.04076162f;
    constexpr float canonHigh = 0.105357102f;
    constexpr float canonToe = 0.069886632f;
    constexpr float canonSlope = 0.42889912f;
    constexpr float canonScale = 14.98325f;
    const float canonLowLinear =
        -(std::pow(10.0f, (canonToe - canonLow) / canonSlope) - 1.0f) / canonScale;
    const float canonHighLinear = (canonHigh - 0.073059361f) / 2.3069815f;
    const float canonLowBelow = std::nextafter(canonLowLinear,
                                                -std::numeric_limits<float>::infinity());
    const float canonLowAbove = std::nextafter(canonLowLinear,
                                                std::numeric_limits<float>::infinity());
    const float canonHighBelow = std::nextafter(canonHighLinear, 0.0f);
    const float canonHighAbove = std::nextafter(canonHighLinear, 1.0f);

    const float canonAtLow = ColorTransferFunction::linearToCanonLog3(canonLowLinear);
    const float canonBelowLow = ColorTransferFunction::linearToCanonLog3(canonLowBelow);
    EXPECT_NEAR(canonBelowLow, canonLow, 2e-7f);
    EXPECT_NEAR(canonAtLow, 0.0470002f, 2e-7f);
    EXPECT_NEAR(ColorTransferFunction::linearToCanonLog3(canonLowAbove), canonAtLow, 2e-6f);
    EXPECT_NEAR(canonAtLow - canonBelowLow, 0.0062386f, 2e-7f);
    const float canonAtHigh = ColorTransferFunction::linearToCanonLog3(canonHighLinear);
    EXPECT_NEAR(ColorTransferFunction::linearToCanonLog3(canonHighBelow), canonAtHigh, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::linearToCanonLog3(canonHighAbove), canonAtHigh, 2e-6f);
}

TEST(ColorTransferFunctionTest, AcesCctSLog3AndCanonLog3DecodeBreakpointsAreCharacterized)
{
    constexpr float acescctBreakpoint = 0.155251141552511f;
    const float acescctBelow = std::nextafter(acescctBreakpoint, 0.0f);
    const float acescctAbove = std::nextafter(acescctBreakpoint, 1.0f);
    const float acescctAt = ColorTransferFunction::acescctToLinear(acescctBreakpoint);
    EXPECT_NEAR(ColorTransferFunction::acescctToLinear(acescctBelow), acescctAt, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::acescctToLinear(acescctAbove), acescctAt, 2e-6f);

    constexpr float slog3Breakpoint = 171.2102946929f / 1023.0f;
    const float slog3Below = std::nextafter(slog3Breakpoint, 0.0f);
    const float slog3Above = std::nextafter(slog3Breakpoint, 1.0f);
    const float slog3At = ColorTransferFunction::sLog3ToLinear(slog3Breakpoint);
    EXPECT_NEAR(ColorTransferFunction::sLog3ToLinear(slog3Below), slog3At, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::sLog3ToLinear(slog3Above), slog3At, 2e-6f);

    constexpr float canonLog3Low = 0.04076162f;
    constexpr float canonLog3High = 0.105357102f;
    const float canonLowBelow = std::nextafter(canonLog3Low, 0.0f);
    const float canonLowAbove = std::nextafter(canonLog3Low, 1.0f);
    const float canonHighBelow = std::nextafter(canonLog3High, 0.0f);
    const float canonHighAbove = std::nextafter(canonLog3High, 1.0f);
    const float canonBelowLow = ColorTransferFunction::canonLog3ToLinear(canonLowBelow);
    const float canonAtLow = ColorTransferFunction::canonLog3ToLinear(canonLog3Low);
    const float canonAtHigh = ColorTransferFunction::canonLog3ToLinear(canonLog3High);
    EXPECT_NEAR(canonBelowLow, -0.0112958f, 2e-7f);
    EXPECT_NEAR(canonAtLow, -0.014f, 2e-7f);
    EXPECT_NEAR(canonBelowLow - canonAtLow, 0.0027042f, 2e-7f);
    EXPECT_NEAR(ColorTransferFunction::canonLog3ToLinear(canonLowAbove), canonAtLow, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::canonLog3ToLinear(canonHighBelow), canonAtHigh, 2e-6f);
    EXPECT_NEAR(ColorTransferFunction::canonLog3ToLinear(canonHighAbove), canonAtHigh, 2e-6f);
}

TEST(ColorTransferFunctionTest, AcesCctSLog3AndCanonLog2ToeRoundTripAdjacentValues)
{
    const auto expectRoundTrip = [](const TransferFunction curve, const float linear,
                                    const float tolerance) {
        const float encoded = ColorTransferFunction::encode(linear, curve);
        const float decoded = ColorTransferFunction::decode(encoded, curve);
        SCOPED_TRACE(::testing::Message()
            << "curve=" << static_cast<int>(curve) << " linear=" << linear
            << " encoded=" << encoded);
        ASSERT_TRUE(std::isfinite(encoded));
        ASSERT_TRUE(std::isfinite(decoded));
        EXPECT_NEAR(decoded, linear, tolerance);
    };

    constexpr float acescctToe = 0.0078125f;
    for (const float linear : {
             std::nextafter(acescctToe, 0.0f), acescctToe,
             std::nextafter(acescctToe, 1.0f)}) {
        expectRoundTrip(TransferFunction::ACEScct, linear, 2e-6f);
    }

    constexpr float slog3Toe = 0.01125000f;
    for (const float linear : {
             std::nextafter(slog3Toe, 0.0f), slog3Toe,
             std::nextafter(slog3Toe, 1.0f)}) {
        expectRoundTrip(TransferFunction::SonySLog3, linear, 2e-6f);
    }

    constexpr float canonToe = 0.035388128f;
    for (const float linear : {
             -1.0e-4f, 0.0f, 0.01f}) {
        expectRoundTrip(TransferFunction::CanonLog2, linear, 2e-6f);
    }
}

TEST(ColorTransferFunctionTest, CanonLog2NegativeToeBoundaryBehaviorIsCharacterized)
{
    constexpr float toe = 0.035388128f;
    constexpr float slope = 0.281863093f;
    constexpr float scale = 87.09937546f;
    const float linearToe = -(std::pow(10.0f, toe / slope) - 1.0f) / scale;
    const float encodedAtLinearToe = ColorTransferFunction::linearToCanonLog2(linearToe);
    const float decodedAtEncodedValue = ColorTransferFunction::canonLog2ToLinear(encodedAtLinearToe);

    EXPECT_NEAR(encodedAtLinearToe, -0.01459125f, 2e-7f);
    EXPECT_NEAR(decodedAtEncodedValue, -0.00578928f, 2e-7f);
    EXPECT_NEAR(decodedAtEncodedValue - linearToe, -0.00194065f, 2e-7f);
}

TEST(ColorTransferFunctionTest, LogCurveBlackAndToeValuesRemainFinite)
{
    const float acesCcBlack = ColorTransferFunction::linearToACEScc(0.0f);
    EXPECT_FLOAT_EQ(acesCcBlack, -0.3584474886f);
    EXPECT_TRUE(std::isfinite(acesCcBlack));

    const float acesCctBlack = ColorTransferFunction::linearToACEScct(0.0f);
    EXPECT_FLOAT_EQ(acesCctBlack, 0.0729055341958355f);
    EXPECT_NEAR(ColorTransferFunction::acescctToLinear(acesCctBlack), 0.0f, 1e-7f);

    for (const float value : {0.0f, 1e-4f, 0.01f, 0.18f}) {
        EXPECT_TRUE(std::isfinite(ColorTransferFunction::linearToSLog3(value)));
        EXPECT_TRUE(std::isfinite(ColorTransferFunction::linearToCineon(value)));
        EXPECT_TRUE(std::isfinite(ColorTransferFunction::linearToCanonLog2(value)));
        EXPECT_TRUE(std::isfinite(ColorTransferFunction::linearToCanonLog3(value)));
    }
}

TEST(TaggedColorTest, UnknownTransferPassesThroughUntouched)
{
    TaggedColor unknown = TaggedColor::sceneLinear(
        -0.25f, 0.4f, 1.5f, 0.37f, SurfaceColorPrimaries::ACES_AP1);
    unknown.transfer = TransferFunction::HLG;
    unknown.transferKnown = false;
    unknown.alphaMode = SurfaceAlphaMode::Premultiplied;

    const auto converted = unknown.toTransfer(TransferFunction::Linear);
    EXPECT_EQ(converted, unknown);
}

TEST(TaggedColorTest, UnsupportedPrimariesConversionsPreserveTaggedColorExactly)
{
    const TaggedColor source = TaggedColor::sceneLinear(
        -0.25f, 0.4f, 1.5f, 0.37f, SurfaceColorPrimaries::Rec2020_D65);
    constexpr SurfaceColorPrimaries unsupportedTargets[] = {
        SurfaceColorPrimaries::Unknown,
        static_cast<SurfaceColorPrimaries>(255),
    };

    for (const SurfaceColorPrimaries target : unsupportedTargets) {
        SCOPED_TRACE(static_cast<int>(target));
        EXPECT_EQ(source.toPrimaries(target), source);
    }

    TaggedColor unknownSource = source;
    unknownSource.primaries = SurfaceColorPrimaries::Unknown;
    EXPECT_EQ(unknownSource.toPrimaries(SurfaceColorPrimaries::ACES_AP1),
              unknownSource);
}

TEST(TaggedColorTest, AlphaModeRoundTrip)
{
    const auto straight = TaggedColor::srgbEncoded(0.8f, 0.6f, 0.4f, 0.5f);
    const auto premul = straight.premultiplied();
    EXPECT_EQ(premul.alphaMode, SurfaceAlphaMode::Premultiplied);
    EXPECT_NEAR(premul.rgba.r(), 0.4f, 1e-6f);

    const auto restored = premul.straight();
    EXPECT_EQ(restored.alphaMode, SurfaceAlphaMode::Straight);
    EXPECT_NEAR(restored.rgba.r(), 0.8f, 1e-5f);

    // Fully transparent premultiplied colors collapse to black, not NaN.
    const auto transparent = TaggedColor::srgbEncoded(0.7f, 0.7f, 0.7f, 0.0f);
    const auto restoredTransparent = transparent.premultiplied().straight();
    EXPECT_FLOAT_EQ(restoredTransparent.rgba.r(), 0.0f);
}

TEST(TaggedColorTest, PremultiplicationClampsAlphaFactorButPreservesSignedHdrRgb)
{
    constexpr float sourceRed = -1.5f;
    constexpr float sourceGreen = 0.5f;
    constexpr float sourceBlue = 2.0f;
    constexpr float alphas[] = {-0.5f, 0.0f, 0.25f, 1.0f, 1.5f};

    for (const float alpha : alphas) {
        const TaggedColor source = TaggedColor::sceneLinear(
            sourceRed, sourceGreen, sourceBlue, alpha);
        const TaggedColor result = source.premultiplied();
        const float factor = std::clamp(alpha, 0.0f, 1.0f);
        SCOPED_TRACE(alpha);

        EXPECT_EQ(result.alphaMode, SurfaceAlphaMode::Premultiplied);
        EXPECT_FLOAT_EQ(result.rgba.a(), alpha);
        EXPECT_FLOAT_EQ(result.rgba.r(), sourceRed * factor);
        EXPECT_FLOAT_EQ(result.rgba.g(), sourceGreen * factor);
        EXPECT_FLOAT_EQ(result.rgba.b(), sourceBlue * factor);
        EXPECT_EQ(source.alphaMode, SurfaceAlphaMode::Straight);
        EXPECT_FLOAT_EQ(source.rgba.r(), sourceRed);
        EXPECT_FLOAT_EQ(source.rgba.g(), sourceGreen);
        EXPECT_FLOAT_EQ(source.rgba.b(), sourceBlue);
    }
}

TEST(TaggedColorTest, SignedHdrAlphaModeRoundTripPreservesSceneLinearMetadata)
{
    constexpr float alphas[] = {1.1e-6f, 0.125f, 0.5f, 1.0f};
    for (const float alpha : alphas) {
        const TaggedColor source = TaggedColor::sceneLinear(
            -1.25f, 0.375f, 2.5f, alpha);
        const TaggedColor premultiplied = source.premultiplied();
        const TaggedColor restored = premultiplied.straight();
        SCOPED_TRACE(alpha);

        EXPECT_EQ(premultiplied.alphaMode, SurfaceAlphaMode::Premultiplied);
        EXPECT_EQ(restored.alphaMode, SurfaceAlphaMode::Straight);
        EXPECT_EQ(restored.transfer, source.transfer);
        EXPECT_EQ(restored.transferKnown, source.transferKnown);
        EXPECT_EQ(restored.primaries, source.primaries);
        EXPECT_FLOAT_EQ(restored.rgba.a(), source.rgba.a());
        EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 2e-6f);
        EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 2e-6f);
        EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 2e-6f);
        EXPECT_FLOAT_EQ(source.rgba.r(), -1.25f);
        EXPECT_FLOAT_EQ(source.rgba.g(), 0.375f);
        EXPECT_FLOAT_EQ(source.rgba.b(), 2.5f);
    }
}

TEST(TaggedColorTest, AlphaModeRoundTripPreservesEveryTransferMetadata)
{
    constexpr TransferFunction transfers[] = {
        TransferFunction::Linear,
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Gamma24,
        TransferFunction::Gamma26,
        TransferFunction::Rec709,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::ACESlog,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::Cineon,
        TransferFunction::SonySLog3,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    constexpr float alphas[] = {0.125f, 0.5f, 1.0f, 1.25f};

    for (const TransferFunction transfer : transfers) {
        for (const float alpha : alphas) {
            TaggedColor source = TaggedColor::sceneLinear(
                0.41f, 0.23f, 0.12f, alpha,
                SurfaceColorPrimaries::Rec2020_D65);
            source.transfer = transfer;
            source.alphaMode = SurfaceAlphaMode::Straight;
            const TaggedColor premultiplied = source.premultiplied();
            const TaggedColor restored = premultiplied.straight();
            SCOPED_TRACE(::testing::Message()
                << "transfer=" << static_cast<int>(transfer)
                << " alpha=" << alpha);

            EXPECT_EQ(premultiplied.alphaMode, SurfaceAlphaMode::Premultiplied);
            EXPECT_EQ(restored.alphaMode, SurfaceAlphaMode::Straight);
            EXPECT_EQ(restored.transfer, source.transfer);
            EXPECT_EQ(restored.transferKnown, source.transferKnown);
            EXPECT_EQ(restored.primaries, source.primaries);
            EXPECT_FLOAT_EQ(restored.rgba.a(), source.rgba.a());
            EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 2.0e-6f);
            EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 2.0e-6f);
            EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 2.0e-6f);
        }
    }
}

TEST(TaggedColorTest, UnpremultiplicationUsesStrictEpsilonBoundary)
{
    constexpr float epsilon = 1.0e-6f;
    const float justAbove = std::nextafter(epsilon,
                                           std::numeric_limits<float>::infinity());
    for (const float alpha : {0.0f, epsilon, justAbove}) {
        TaggedColor premultiplied = TaggedColor::sceneLinear(
            0.25f * alpha, 0.5f * alpha, 0.75f * alpha, alpha);
        premultiplied.alphaMode = SurfaceAlphaMode::Premultiplied;
        const TaggedColor straight = premultiplied.straight();
        SCOPED_TRACE(alpha);
        EXPECT_EQ(straight.alphaMode, SurfaceAlphaMode::Straight);
        EXPECT_FLOAT_EQ(straight.rgba.a(), alpha);
        if (alpha <= epsilon) {
            EXPECT_FLOAT_EQ(straight.rgba.r(), 0.0f);
            EXPECT_FLOAT_EQ(straight.rgba.g(), 0.0f);
            EXPECT_FLOAT_EQ(straight.rgba.b(), 0.0f);
        } else {
            EXPECT_NEAR(straight.rgba.r(), 0.25f, 1.0e-6f);
            EXPECT_NEAR(straight.rgba.g(), 0.5f, 1.0e-6f);
            EXPECT_NEAR(straight.rgba.b(), 0.75f, 1.0e-6f);
        }
    }
}

TEST(TaggedColorTest, OpaqueAlphaModeIsUnchangedByAlphaModeConversions)
{
    TaggedColor opaque = TaggedColor::sceneLinear(0.2f, 0.4f, 0.6f, 0.37f);
    opaque.alphaMode = SurfaceAlphaMode::Opaque;

    EXPECT_EQ(opaque.premultiplied(), opaque);
    EXPECT_EQ(opaque.straight(), opaque);
}

TEST(TaggedColorTest, SurfaceDescriptorMatchesValue)
{
    const auto linear = TaggedColor::sceneLinear(0.1f, 0.2f, 0.3f);
    const auto descriptor = linear.surfaceDescriptor();
    EXPECT_EQ(descriptor.storage, SurfacePixelStorage::RGBA32Float);
    EXPECT_EQ(descriptor.transfer, TransferFunction::Linear);
    EXPECT_EQ(descriptor.range, SurfaceColorRange::SceneReferred);
    EXPECT_EQ(descriptor.primaries, SurfaceColorPrimaries::SRGB_Rec709_D65);

    const auto encoded = TaggedColor::srgbEncoded(0.1f, 0.2f, 0.3f);
    EXPECT_EQ(encoded.surfaceDescriptor().range,
              SurfaceColorRange::DisplayReferred);
}

TEST(TaggedColorTest, GamutVocabularyBridgesPrimaries)
{
    const auto rec709 =
        gamutForPrimaries(SurfaceColorPrimaries::SRGB_Rec709_D65);
    ASSERT_TRUE(rec709.has_value());
    EXPECT_EQ(*rec709, Gamut::Rec709);

    const auto ap1 = gamutForPrimaries(SurfaceColorPrimaries::ACES_AP1);
    ASSERT_TRUE(ap1.has_value());
    EXPECT_EQ(*ap1, Gamut::ACES_AP1);
    EXPECT_FALSE(gamutForPrimaries(SurfaceColorPrimaries::Unknown).has_value());

    const auto displayP3 = primariesForGamut(Gamut::DisplayP3);
    ASSERT_TRUE(displayP3.has_value());
    EXPECT_EQ(*displayP3, SurfaceColorPrimaries::DisplayP3_D65);
    // DCI-P3 has no surface-contract counterpart.
    EXPECT_FALSE(primariesForGamut(Gamut::DCI_P3).has_value());
}

TEST(TaggedColorTest, GammaEncodedValuesRoundTripAcrossSupportedPrimaries)
{
    constexpr SurfaceColorPrimaries primaries[] = {
        SurfaceColorPrimaries::SRGB_Rec709_D65,
        SurfaceColorPrimaries::DisplayP3_D65,
        SurfaceColorPrimaries::Rec2020_D65,
        SurfaceColorPrimaries::ACES_AP0,
        SurfaceColorPrimaries::ACES_AP1,
    };

    for (const auto sourcePrimaries : primaries) {
        for (const auto targetPrimaries : primaries) {
            TaggedColor source = TaggedColor::srgbEncoded(0.24f, 0.28f, 0.32f, 0.37f);
            source.primaries = sourcePrimaries;
            source.transfer = TransferFunction::Gamma22;
            source.alphaMode = SurfaceAlphaMode::Straight;
            const TaggedColor converted = source.toPrimaries(targetPrimaries);
            const TaggedColor restored = converted.toPrimaries(sourcePrimaries);
            SCOPED_TRACE(::testing::Message()
                << "source=" << static_cast<int>(sourcePrimaries)
                << " target=" << static_cast<int>(targetPrimaries));

            EXPECT_EQ(converted.primaries, targetPrimaries);
            EXPECT_EQ(converted.transfer, source.transfer);
            EXPECT_EQ(converted.transferKnown, source.transferKnown);
            EXPECT_EQ(converted.alphaMode, source.alphaMode);
            EXPECT_FLOAT_EQ(converted.rgba.a(), source.rgba.a());
            EXPECT_EQ(restored.primaries, sourcePrimaries);
            EXPECT_EQ(restored.transfer, source.transfer);
            EXPECT_EQ(restored.transferKnown, source.transferKnown);
            EXPECT_EQ(restored.alphaMode, source.alphaMode);
            EXPECT_FLOAT_EQ(restored.rgba.a(), source.rgba.a());
            EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 1.0e-3f);
            EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 1.0e-3f);
            EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 1.0e-3f);
        }
    }
}

TEST(TaggedColorTest, PrimariesConversionRoundTrips)
{
    const auto source = TaggedColor::sceneLinear(0.7f, 0.2f, 0.05f);
    const auto wide = source.toPrimaries(SurfaceColorPrimaries::Rec2020_D65);
    EXPECT_EQ(wide.primaries, SurfaceColorPrimaries::Rec2020_D65);
    EXPECT_EQ(wide.transfer, TransferFunction::Linear);

    // Values actually move between gamuts.
    EXPECT_NE(wide.rgba.g(), source.rgba.g());

    const auto restored = wide.toPrimaries(SurfaceColorPrimaries::SRGB_Rec709_D65);
    EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 1e-4f);
    EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 1e-4f);
    EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 1e-4f);

    // Same primaries is a no-op; unknown transfer passes through.
    EXPECT_EQ(source.toPrimaries(SurfaceColorPrimaries::SRGB_Rec709_D65), source);
    TaggedColor unknown = TaggedColor::sceneLinear(0.1f, 0.2f, 0.3f);
    unknown.transferKnown = false;
    EXPECT_EQ(unknown.toPrimaries(SurfaceColorPrimaries::Rec2020_D65), unknown);
}

TEST(TaggedColorTest, PrimariesRoundTripPreservesNonlinearTransferAndAlpha)
{
    TaggedColor source = TaggedColor::srgbEncoded(0.41f, 0.23f, 0.12f, 0.37f);
    source.transfer = TransferFunction::Gamma22;
    const auto wide = source.toPrimaries(SurfaceColorPrimaries::Rec2020_D65);

    EXPECT_EQ(wide.primaries, SurfaceColorPrimaries::Rec2020_D65);
    EXPECT_EQ(wide.transfer, source.transfer);
    EXPECT_EQ(wide.transferKnown, source.transferKnown);
    EXPECT_EQ(wide.alphaMode, source.alphaMode);
    EXPECT_FLOAT_EQ(wide.rgba.a(), source.rgba.a());

    const auto restored = wide.toPrimaries(source.primaries);
    EXPECT_EQ(restored.primaries, source.primaries);
    EXPECT_EQ(restored.transfer, source.transfer);
    EXPECT_FLOAT_EQ(restored.rgba.a(), source.rgba.a());
    EXPECT_NEAR(restored.rgba.r(), source.rgba.r(), 1.0e-4f);
    EXPECT_NEAR(restored.rgba.g(), source.rgba.g(), 1.0e-4f);
    EXPECT_NEAR(restored.rgba.b(), source.rgba.b(), 1.0e-4f);
}

TEST(TaggedColorTest, PrimariesConversionMatchesExplicitTransferAndGamutPipeline)
{
    constexpr SurfaceColorPrimaries primaries[] = {
        SurfaceColorPrimaries::SRGB_Rec709_D65,
        SurfaceColorPrimaries::DisplayP3_D65,
        SurfaceColorPrimaries::Rec2020_D65,
        SurfaceColorPrimaries::ACES_AP0,
        SurfaceColorPrimaries::ACES_AP1,
    };
    constexpr TransferFunction transfers[] = {
        TransferFunction::Linear,
        TransferFunction::sRGB,
        TransferFunction::Gamma22,
        TransferFunction::Rec2020_10,
        TransferFunction::Rec2084_PQ,
        TransferFunction::HLG,
        TransferFunction::ACEScc,
        TransferFunction::ACEScct,
        TransferFunction::DaVinciIntermediate,
        TransferFunction::Cineon,
        TransferFunction::SonySLog3,
        TransferFunction::CanonLog2,
        TransferFunction::CanonLog3,
    };
    constexpr FloatRGBA encodedSource(0.41f, 0.23f, 0.12f, 0.37f);

    for (const SurfaceColorPrimaries sourcePrimaries : primaries) {
        for (const SurfaceColorPrimaries targetPrimaries : primaries) {
            const auto sourceGamut = gamutForPrimaries(sourcePrimaries);
            const auto targetGamut = gamutForPrimaries(targetPrimaries);
            ASSERT_TRUE(sourceGamut.has_value());
            ASSERT_TRUE(targetGamut.has_value());

            for (const TransferFunction transfer : transfers) {
                TaggedColor source = TaggedColor::srgbEncoded(
                    encodedSource.r(), encodedSource.g(), encodedSource.b(),
                    encodedSource.a());
                source.primaries = sourcePrimaries;
                source.transfer = transfer;

                const float linearR = ColorTransferFunction::decode(
                    source.rgba.r(), transfer);
                const float linearG = ColorTransferFunction::decode(
                    source.rgba.g(), transfer);
                const float linearB = ColorTransferFunction::decode(
                    source.rgba.b(), transfer);
                float mappedLinearR = 0.0f;
                float mappedLinearG = 0.0f;
                float mappedLinearB = 0.0f;
                ColorGamutConversion::convert(
                    linearR, linearG, linearB, *sourceGamut, *targetGamut,
                    mappedLinearR, mappedLinearG, mappedLinearB);
                const std::array<float, 3> expected = {
                    ColorTransferFunction::encode(mappedLinearR, transfer),
                    ColorTransferFunction::encode(mappedLinearG, transfer),
                    ColorTransferFunction::encode(mappedLinearB, transfer),
                };
                const std::array<float, 3> expectedResult =
                    sourcePrimaries == targetPrimaries
                        ? std::array<float, 3>{encodedSource.r(), encodedSource.g(),
                                               encodedSource.b()}
                        : expected;

                const TaggedColor converted = source.toPrimaries(targetPrimaries);
                SCOPED_TRACE(::testing::Message()
                    << "source=" << static_cast<int>(sourcePrimaries)
                    << " target=" << static_cast<int>(targetPrimaries)
                    << " transfer=" << static_cast<int>(transfer));
                EXPECT_EQ(converted.primaries, targetPrimaries);
                EXPECT_EQ(converted.transfer, transfer);
                EXPECT_TRUE(converted.transferKnown);
                EXPECT_EQ(converted.alphaMode, source.alphaMode);
                EXPECT_FLOAT_EQ(converted.rgba.a(), source.rgba.a());
                EXPECT_NEAR(converted.rgba.r(), expectedResult[0], 1.0e-6f);
                EXPECT_NEAR(converted.rgba.g(), expectedResult[1], 1.0e-6f);
                EXPECT_NEAR(converted.rgba.b(), expectedResult[2], 1.0e-6f);
            }
        }
    }
}
