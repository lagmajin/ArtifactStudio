#include <gtest/gtest.h>

#include <utility>

import Color.Float;

using namespace ArtifactCore;

TEST(FloatColorStandaloneTest, DefaultAndNamedAccessorsExposeRgbaComponents)
{
    const FloatColor color;
    EXPECT_FLOAT_EQ(color.r(), 0.0f);
    EXPECT_FLOAT_EQ(color.g(), 0.0f);
    EXPECT_FLOAT_EQ(color.b(), 0.0f);
    EXPECT_FLOAT_EQ(color.a(), 1.0f);
    EXPECT_FLOAT_EQ(color.red(), color.r());
    EXPECT_FLOAT_EQ(color.green(), color.g());
    EXPECT_FLOAT_EQ(color.blue(), color.b());
    EXPECT_FLOAT_EQ(color.alpha(), color.a());
}

TEST(FloatColorStandaloneTest, CopyAndMoveOperationsPreserveAllChannels)
{
    const FloatColor source(0.15f, 0.35f, 0.55f, 0.75f);
    FloatColor copied(source);
    FloatColor copyAssigned;
    copyAssigned = source;

    FloatColor moved(std::move(copied));
    FloatColor moveAssigned;
    moveAssigned = std::move(copyAssigned);

    EXPECT_EQ(moved, source);
    EXPECT_EQ(moveAssigned, source);
}

TEST(FloatColorStandaloneTest, SettersAndAggregatesIncludeTheExpectedChannels)
{
    FloatColor color(0.2f, 0.4f, 0.6f, 0.8f);
    EXPECT_FLOAT_EQ(color.sumRGB(), 1.2f);
    EXPECT_FLOAT_EQ(color.averageRGB(), 0.4f);
    EXPECT_FLOAT_EQ(color.sumRGBA(), 2.0f);
    EXPECT_FLOAT_EQ(color.averageRGBA(), 0.5f);

    color.setRed(0.1f);
    color.setGreen(0.3f);
    color.setBlue(0.5f);
    color.setAlpha(0.7f);
    EXPECT_EQ(color, FloatColor(0.1f, 0.3f, 0.5f, 0.7f));
    color.setColor(0.25f, 0.5f, 0.75f);
    EXPECT_EQ(color, FloatColor(0.25f, 0.5f, 0.75f, 0.7f));
}

TEST(FloatColorStandaloneTest, FourChannelSetColorReplacesAlpha)
{
    FloatColor color(0.1f, 0.2f, 0.3f, 0.4f);

    color.setColor(0.6f, 0.7f, 0.8f, 0.9f);

    EXPECT_EQ(color, FloatColor(0.6f, 0.7f, 0.8f, 0.9f));
}

TEST(FloatColorStandaloneTest, ClampBoundsAlphaAlongWithRgb)
{
    FloatColor color(-0.2f, 0.4f, 1.4f, 1.8f);

    color.clamp();

    EXPECT_EQ(color, FloatColor(0.0f, 0.4f, 1.0f, 1.0f));
}

TEST(FloatColorStandaloneTest, SrgbLinearConversionsTransformRgbAndPreserveAlpha)
{
    const FloatColor encoded(0.04045f, 0.5f, 1.0f, 0.37f);

    const FloatColor linear = encoded.toLinear();
    EXPECT_NEAR(linear.r(), 0.04045f / 12.92f, 1.0e-7f);
    EXPECT_NEAR(linear.g(), 0.21404114f, 1.0e-7f);
    EXPECT_FLOAT_EQ(linear.b(), 1.0f);
    EXPECT_FLOAT_EQ(linear.a(), encoded.a());

    const FloatColor roundTripped = linear.fromLinear();
    EXPECT_NEAR(roundTripped.r(), encoded.r(), 1.0e-7f);
    EXPECT_NEAR(roundTripped.g(), encoded.g(), 1.0e-6f);
    EXPECT_FLOAT_EQ(roundTripped.b(), encoded.b());
    EXPECT_FLOAT_EQ(roundTripped.a(), encoded.a());
}

TEST(FloatColorStandaloneTest, SrgbLinearConversionsClampNegativeRgbToZero)
{
    const FloatColor negativeEncoded(-0.1f, -1.0f, 0.5f, 0.42f);
    const FloatColor linear = negativeEncoded.toLinear();
    EXPECT_FLOAT_EQ(linear.r(), 0.0f);
    EXPECT_FLOAT_EQ(linear.g(), 0.0f);
    EXPECT_GT(linear.b(), 0.0f);
    EXPECT_FLOAT_EQ(linear.a(), negativeEncoded.a());

    const FloatColor negativeLinear(-0.1f, -1.0f, 0.5f, 0.42f);
    const FloatColor encoded = negativeLinear.fromLinear();
    EXPECT_FLOAT_EQ(encoded.r(), 0.0f);
    EXPECT_FLOAT_EQ(encoded.g(), 0.0f);
    EXPECT_GT(encoded.b(), 0.0f);
    EXPECT_FLOAT_EQ(encoded.a(), negativeLinear.a());
}

TEST(FloatColorStandaloneTest, LerpInterpolatesAlphaAndClampsInterpolationFactor)
{
    const FloatColor start(0.0f, 0.2f, 0.4f, 0.1f);
    const FloatColor end(1.0f, 0.8f, 0.6f, 0.9f);

    EXPECT_TRUE(FloatColor::lerp(start, end, 0.25f).approximatelyEqual(
        FloatColor(0.25f, 0.35f, 0.45f, 0.3f), 1.0e-6f));
    EXPECT_EQ(FloatColor::lerp(start, end, -1.0f), start);
    EXPECT_EQ(FloatColor::lerp(start, end, 2.0f), end);
}

TEST(FloatColorStandaloneTest, ApproximateEqualityIncludesAlphaAndUsesStrictEpsilon)
{
    const FloatColor reference(0.2f, 0.4f, 0.6f, 0.8f);
    EXPECT_TRUE(reference.approximatelyEqual(
        FloatColor(0.2005f, 0.4f, 0.6f, 0.8f), 0.001f));
    EXPECT_FALSE(reference.approximatelyEqual(
        FloatColor(0.2f, 0.4f, 0.6f, 0.801f), 0.001f));
    EXPECT_FALSE(reference.approximatelyEqual(
        FloatColor(0.2f, 0.4f, 0.6f, 0.8f), 0.0f));
}

TEST(FloatColorStandaloneTest, ExactEqualityAndInequalityIncludeAlpha)
{
    const FloatColor color(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatColor identical(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatColor differentAlpha(0.2f, 0.4f, 0.6f, 0.9f);

    EXPECT_TRUE(color == identical);
    EXPECT_FALSE(color != identical);
    EXPECT_FALSE(color == differentAlpha);
    EXPECT_TRUE(color != differentAlpha);
}

TEST(FloatColorStandaloneTest, ComponentwiseArithmeticAndScalarLeftMultiplyIncludeAlpha)
{
    const FloatColor left(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatColor right(0.5f, 0.25f, 0.5f, 0.5f);

    EXPECT_TRUE((left * right).approximatelyEqual(
        FloatColor(0.1f, 0.1f, 0.3f, 0.4f), 1.0e-6f));
    EXPECT_TRUE((left / right).approximatelyEqual(
        FloatColor(0.4f, 1.6f, 1.2f, 1.6f), 1.0e-6f));
    EXPECT_TRUE((2.0f * left).approximatelyEqual(
        FloatColor(0.4f, 0.8f, 1.2f, 1.6f), 1.0e-6f));
}

TEST(FloatColorStandaloneTest, BinaryAdditionSubtractionAndScalarArithmeticIncludeAlpha)
{
    const FloatColor color(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatColor offset(0.1f, 0.2f, 0.3f, 0.1f);

    EXPECT_TRUE((color + offset).approximatelyEqual(
        FloatColor(0.3f, 0.6f, 0.9f, 0.9f), 1.0e-6f));
    EXPECT_TRUE((color - offset).approximatelyEqual(
        FloatColor(0.1f, 0.2f, 0.3f, 0.7f), 1.0e-6f));
    EXPECT_TRUE((color * 2.0f).approximatelyEqual(
        FloatColor(0.4f, 0.8f, 1.2f, 1.6f), 1.0e-6f));
    EXPECT_TRUE((color / 2.0f).approximatelyEqual(
        FloatColor(0.1f, 0.2f, 0.3f, 0.4f), 1.0e-6f));
}

TEST(FloatColorStandaloneTest, CompoundArithmeticAndUnaryNegationUpdateEveryChannel)
{
    const FloatColor increment(0.1f, 0.2f, 0.3f, 0.1f);
    const FloatColor divisor(0.5f, 0.25f, 0.5f, 0.5f);
    FloatColor value(0.2f, 0.4f, 0.6f, 0.8f);

    value += increment;
    EXPECT_TRUE(value.approximatelyEqual(FloatColor(0.3f, 0.6f, 0.9f, 0.9f), 1.0e-6f));
    value -= increment;
    EXPECT_TRUE(value.approximatelyEqual(FloatColor(0.2f, 0.4f, 0.6f, 0.8f), 1.0e-6f));
    value *= 2.0f;
    EXPECT_TRUE(value.approximatelyEqual(FloatColor(0.4f, 0.8f, 1.2f, 1.6f), 1.0e-6f));
    value /= 2.0f;
    value *= divisor;
    EXPECT_TRUE(value.approximatelyEqual(FloatColor(0.1f, 0.1f, 0.3f, 0.4f), 1.0e-6f));
    value /= divisor;
    EXPECT_TRUE(value.approximatelyEqual(FloatColor(0.2f, 0.4f, 0.6f, 0.8f), 1.0e-6f));

    EXPECT_TRUE((-value).approximatelyEqual(FloatColor(-0.2f, -0.4f, -0.6f, -0.8f), 1.0e-6f));
}
