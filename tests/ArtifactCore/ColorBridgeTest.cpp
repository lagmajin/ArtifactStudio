#include <gtest/gtest.h>
#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

import Color.Float;
import FloatRGBA;
import Color.Bridge;
import Color.Tagged;
import Color.TransferFunction;
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
    EXPECT_NEAR(fromHex.r(), 0.1f, kColorEpsilon);
    EXPECT_NEAR(fromHex.a(), 0.4f, kColorEpsilon);
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

TEST(TaggedColorTest, UnknownTransferPassesThroughUntouched)
{
    TaggedColor unknown = TaggedColor::srgbEncoded(0.3f, 0.4f, 0.5f);
    unknown.transfer = TransferFunction::HLG;
    unknown.transferKnown = false;

    const auto converted = unknown.toTransfer(TransferFunction::Linear);
    EXPECT_EQ(converted.rgba.r(), unknown.rgba.r());
    EXPECT_EQ(converted.transferKnown, false);
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
