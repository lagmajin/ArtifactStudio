#include <gtest/gtest.h>

#include <cmath>

import Color.Float;
import Color.Lab;
import Utils.String.UniString;

using namespace ArtifactCore;

TEST(LabColorContractTest, DefaultAndComponentAccessorsPreserveValues)
{
    const LabColor defaultColor;
    EXPECT_FLOAT_EQ(defaultColor.L(), 0.0f);
    EXPECT_FLOAT_EQ(defaultColor.a(), 0.0f);
    EXPECT_FLOAT_EQ(defaultColor.b(), 0.0f);

    LabColor color(52.0f, -12.5f, 8.25f);
    color.setL(61.0f);
    color.setA(3.5f);
    color.setB(-7.75f);
    EXPECT_FLOAT_EQ(color.L(), 61.0f);
    EXPECT_FLOAT_EQ(color.a(), 3.5f);
    EXPECT_FLOAT_EQ(color.b(), -7.75f);
    EXPECT_FLOAT_EQ(color.luminance(), color.L());
}

TEST(LabColorContractTest, CopyAndAssignmentKeepValuesIndependent)
{
    LabColor source(43.0f, -4.0f, 19.0f);
    LabColor copy(source);
    copy.setL(80.0f);
    EXPECT_FLOAT_EQ(source.L(), 43.0f);
    EXPECT_FLOAT_EQ(copy.L(), 80.0f);

    LabColor assigned;
    assigned = source;
    EXPECT_TRUE(assigned == source);
    assigned.setB(0.0f);
    EXPECT_FLOAT_EQ(source.b(), 19.0f);
}

TEST(LabColorContractTest, ArithmeticAndCompoundOperatorsOperatePerComponent)
{
    const LabColor first(40.0f, -10.0f, 20.0f);
    const LabColor second(5.0f, 2.0f, -4.0f);
    const LabColor sum = first + second;
    const LabColor difference = first - second;
    const LabColor scaled = first * 2.0f;
    const LabColor divided = first / 2.0f;
    EXPECT_FLOAT_EQ(sum.L(), 45.0f);
    EXPECT_FLOAT_EQ(sum.a(), -8.0f);
    EXPECT_FLOAT_EQ(sum.b(), 16.0f);
    EXPECT_FLOAT_EQ(difference.L(), 35.0f);
    EXPECT_FLOAT_EQ(difference.a(), -12.0f);
    EXPECT_FLOAT_EQ(difference.b(), 24.0f);
    EXPECT_FLOAT_EQ(scaled.a(), -20.0f);
    EXPECT_FLOAT_EQ(divided.b(), 10.0f);

    LabColor compound = first;
    compound += second;
    compound -= second;
    compound *= 2.0f;
    compound /= 2.0f;
    EXPECT_TRUE(compound == first);
}

TEST(LabColorContractTest, DeltaEIsEuclideanDistanceAndLerpClampsItsParameter)
{
    const LabColor origin(0.0f, 0.0f, 0.0f);
    const LabColor offset(3.0f, 4.0f, 12.0f);
    EXPECT_FLOAT_EQ(origin.deltaE(offset), 13.0f);

    const LabColor start(10.0f, -20.0f, 30.0f);
    const LabColor end(50.0f, 20.0f, -10.0f);
    const LabColor midpoint = LabColor::lerp(start, end, 0.5f);
    EXPECT_FLOAT_EQ(midpoint.L(), 30.0f);
    EXPECT_FLOAT_EQ(midpoint.a(), 0.0f);
    EXPECT_FLOAT_EQ(midpoint.b(), 10.0f);
    EXPECT_TRUE(LabColor::lerp(start, end, -1.0f) == start);
    EXPECT_TRUE(LabColor::lerp(start, end, 2.0f) == end);
}

TEST(LabColorContractTest, FloatColorRoundTripPreservesRepresentativeSrgbColors)
{
    const FloatColor colors[] = {
        FloatColor(0.0f, 0.0f, 0.0f, 0.2f),
        FloatColor(1.0f, 1.0f, 1.0f, 0.7f),
        FloatColor(0.25f, 0.5f, 0.75f, 0.4f),
    };
    for (const auto& original : colors) {
        const LabColor lab = LabColor::fromFloatColor(original);
        const FloatColor restored = lab.toFloatColor();
        EXPECT_NEAR(restored.r(), original.r(), 0.003f);
        EXPECT_NEAR(restored.g(), original.g(), 0.003f);
        EXPECT_NEAR(restored.b(), original.b(), 0.003f);
        EXPECT_FLOAT_EQ(restored.a(), 1.0f);
    }
}

TEST(LabColorContractTest, ParsesSignedComponentsAndDefaultsMalformedInputToZero)
{
    const LabColor parsed = LabColor::fromString(
        UniString("L:-12.5,a:+3.25,b:-0.75"));
    EXPECT_FLOAT_EQ(parsed.L(), -12.5f);
    EXPECT_FLOAT_EQ(parsed.a(), 3.25f);
    EXPECT_FLOAT_EQ(parsed.b(), -0.75f);

    const LabColor malformed = LabColor::fromString(UniString("not Lab data"));
    EXPECT_FLOAT_EQ(malformed.L(), 0.0f);
    EXPECT_FLOAT_EQ(malformed.a(), 0.0f);
    EXPECT_FLOAT_EQ(malformed.b(), 0.0f);
}

TEST(LabColorContractTest, SerializesOrdinarySignedDecimalComponents)
{
    const LabColor original(51.25f, -7.5f, 3.125f);
    const LabColor restored = LabColor::fromString(original.toString());
    EXPECT_NEAR(restored.L(), original.L(), 1e-4f);
    EXPECT_NEAR(restored.a(), original.a(), 1e-4f);
    EXPECT_NEAR(restored.b(), original.b(), 1e-4f);
}
