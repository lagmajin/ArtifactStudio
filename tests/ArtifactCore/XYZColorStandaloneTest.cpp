#include <gtest/gtest.h>

#include <cmath>

import Color.XYZ;
import Color.Float;
import Utils.String.UniString;

using namespace ArtifactCore;

TEST(XYZColorStandaloneTest, DefaultConstructorCreatesBlackOrigin)
{
    const XYZColor color;
    EXPECT_FLOAT_EQ(color.X(), 0.0f);
    EXPECT_FLOAT_EQ(color.Y(), 0.0f);
    EXPECT_FLOAT_EQ(color.Z(), 0.0f);
}

TEST(XYZColorStandaloneTest, ComponentAccessorsRoundTripValues)
{
    XYZColor color(0.1f, 0.2f, 0.3f);
    color.setX(0.4f);
    color.setY(0.5f);
    color.setZ(0.6f);
    EXPECT_FLOAT_EQ(color.X(), 0.4f);
    EXPECT_FLOAT_EQ(color.Y(), 0.5f);
    EXPECT_FLOAT_EQ(color.Z(), 0.6f);
}

TEST(XYZColorStandaloneTest, CopyConstructionAndAssignmentAreIndependent)
{
    const XYZColor original(0.2f, 0.4f, 0.6f);
    XYZColor copied(original);
    XYZColor assigned;
    assigned = original;
    copied.setX(0.9f);
    assigned.setY(0.8f);
    EXPECT_FLOAT_EQ(original.X(), 0.2f);
    EXPECT_FLOAT_EQ(original.Y(), 0.4f);
    EXPECT_FLOAT_EQ(copied.X(), 0.9f);
    EXPECT_FLOAT_EQ(assigned.Y(), 0.8f);
}

TEST(XYZColorStandaloneTest, ClampConstrainsEachComponentToUnitInterval)
{
    XYZColor color(-0.2f, 0.45f, 1.4f);
    color.clamp();
    EXPECT_FLOAT_EQ(color.X(), 0.0f);
    EXPECT_FLOAT_EQ(color.Y(), 0.45f);
    EXPECT_FLOAT_EQ(color.Z(), 1.0f);
}

TEST(XYZColorStandaloneTest, LuminanceUsesTheYComponent)
{
    const XYZColor color(0.25f, 0.7f, 0.4f);
    EXPECT_FLOAT_EQ(color.luminance(), 0.7f);
}

TEST(XYZColorStandaloneTest, DeltaEIsEuclideanDistanceInXyzSpace)
{
    const XYZColor origin(0.0f, 0.0f, 0.0f);
    const XYZColor offset(2.0f, 3.0f, 6.0f);
    EXPECT_FLOAT_EQ(origin.deltaE(offset), 7.0f);
    EXPECT_FLOAT_EQ(offset.deltaE(origin), 7.0f);
}

TEST(XYZColorStandaloneTest, ArithmeticOperatorsCombineComponents)
{
    const XYZColor a(0.2f, 0.4f, 0.6f);
    const XYZColor b(0.1f, 0.3f, 0.5f);
    const XYZColor sum = a + b;
    const XYZColor difference = a - b;
    EXPECT_FLOAT_EQ(sum.X(), 0.3f);
    EXPECT_FLOAT_EQ(sum.Y(), 0.7f);
    EXPECT_FLOAT_EQ(sum.Z(), 1.1f);
    EXPECT_FLOAT_EQ(difference.X(), 0.1f);
    EXPECT_FLOAT_EQ(difference.Y(), 0.1f);
    EXPECT_FLOAT_EQ(difference.Z(), 0.1f);
}

TEST(XYZColorStandaloneTest, ScalarMultiplyAndDivideRoundTrip)
{
    const XYZColor color(0.2f, 0.4f, 0.6f);
    const XYZColor result = (color * 2.0f) / 2.0f;
    EXPECT_TRUE(result == color);
}

TEST(XYZColorStandaloneTest, CompoundArithmeticMutatesAllComponents)
{
    XYZColor color(0.2f, 0.4f, 0.6f);
    color += XYZColor(0.1f, 0.2f, 0.3f);
    color *= 2.0f;
    color -= XYZColor(0.1f, 0.2f, 0.3f);
    color /= 2.0f;
    EXPECT_NEAR(color.X(), 0.25f, 1e-6f);
    EXPECT_NEAR(color.Y(), 0.5f, 1e-6f);
    EXPECT_NEAR(color.Z(), 0.75f, 1e-6f);
}

TEST(XYZColorStandaloneTest, EqualityUsesSmallComponentTolerance)
{
    const XYZColor a(0.2f, 0.4f, 0.6f);
    const XYZColor near(0.2000005f, 0.4f, 0.6f);
    const XYZColor far(0.20001f, 0.4f, 0.6f);
    EXPECT_TRUE(a == near);
    EXPECT_FALSE(a == far);
    EXPECT_TRUE(a != far);
}

TEST(XYZColorStandaloneTest, LerpInterpolatesAndClampsT)
{
    const XYZColor a(0.0f, 0.2f, 0.4f);
    const XYZColor b(1.0f, 0.8f, 0.6f);
    const XYZColor middle = XYZColor::lerp(a, b, 0.25f);
    EXPECT_FLOAT_EQ(middle.X(), 0.25f);
    EXPECT_FLOAT_EQ(middle.Y(), 0.35f);
    EXPECT_FLOAT_EQ(middle.Z(), 0.45f);
    EXPECT_TRUE(XYZColor::lerp(a, b, -1.0f) == a);
    EXPECT_TRUE(XYZColor::lerp(a, b, 2.0f) == b);
}

TEST(XYZColorStandaloneTest, FloatColorConversionRoundTripsPrimaryColors)
{
    const FloatColor primaries[] = {
        FloatColor(1.0f, 0.0f, 0.0f, 1.0f),
        FloatColor(0.0f, 1.0f, 0.0f, 1.0f),
        FloatColor(0.0f, 0.0f, 1.0f, 1.0f),
    };
    for (const auto& original : primaries) {
        const XYZColor xyz = XYZColor::fromFloatColor(original);
        const FloatColor converted = xyz.toFloatColor();
        EXPECT_NEAR(converted.r(), original.r(), 0.002f);
        EXPECT_NEAR(converted.g(), original.g(), 0.002f);
        EXPECT_NEAR(converted.b(), original.b(), 0.002f);
        EXPECT_FLOAT_EQ(converted.a(), 1.0f);
    }
}

TEST(XYZColorStandaloneTest, StringRoundTripPreservesOrdinaryPositiveComponents)
{
    const XYZColor original(0.25f, 0.5f, 0.75f);
    const XYZColor parsed = XYZColor::fromString(original.toString());
    EXPECT_NEAR(parsed.X(), original.X(), 1e-5f);
    EXPECT_NEAR(parsed.Y(), original.Y(), 1e-5f);
    EXPECT_NEAR(parsed.Z(), original.Z(), 1e-5f);
}

TEST(XYZColorStandaloneTest, StringParserAcceptsSignedValues)
{
    const XYZColor parsed = XYZColor::fromString(UniString("X:-0.5,Y:+0.25,Z:1.5"));
    EXPECT_FLOAT_EQ(parsed.X(), -0.5f);
    EXPECT_FLOAT_EQ(parsed.Y(), 0.25f);
    EXPECT_FLOAT_EQ(parsed.Z(), 1.5f);
}

TEST(XYZColorStandaloneTest, MalformedStringFallsBackToZeroComponents)
{
    const XYZColor parsed = XYZColor::fromString(UniString("not xyz data"));
    EXPECT_FLOAT_EQ(parsed.X(), 0.0f);
    EXPECT_FLOAT_EQ(parsed.Y(), 0.0f);
    EXPECT_FLOAT_EQ(parsed.Z(), 0.0f);
}
