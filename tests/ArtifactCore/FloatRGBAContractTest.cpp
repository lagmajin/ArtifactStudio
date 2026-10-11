#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

import Color.Float;
import FloatRGBA;

using namespace ArtifactCore;

TEST(FloatRGBAContractTest, DefaultAndRgbConstructorsHaveDistinctAlphaDefaults)
{
    EXPECT_EQ(FloatRGBA(), FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_EQ(FloatRGBA(0.2f, 0.4f, 0.6f), FloatRGBA(0.2f, 0.4f, 0.6f, 1.0f));
}

TEST(FloatRGBAContractTest, SettersAndSetRGBAUpdateOnlyTheirSpecifiedChannels)
{
    FloatRGBA color(0.1f, 0.2f, 0.3f, 0.4f);
    color.setRed(0.5f);
    color.setGreen(0.6f);
    color.setBlue(0.7f);
    color.setAlpha(0.8f);
    EXPECT_EQ(color, FloatRGBA(0.5f, 0.6f, 0.7f, 0.8f));

    color.setRGBA(0.9f, 0.8f, 0.7f);
    EXPECT_EQ(color, FloatRGBA(0.9f, 0.8f, 0.7f, 0.0f));
}

TEST(FloatRGBAContractTest, IndexingMapsEachValidIndexToItsChannel)
{
    FloatRGBA color(1.0f, 2.0f, 3.0f, 4.0f);
    for (int index = 0; index < 4; ++index) {
        EXPECT_FLOAT_EQ(color[index], static_cast<float>(index + 1));
        color[index] = static_cast<float>(index + 5);
    }
    EXPECT_EQ(color, FloatRGBA(5.0f, 6.0f, 7.0f, 8.0f));
}

TEST(FloatRGBAContractTest, IndexingRejectsNegativeAndPastEndIndices)
{
    FloatRGBA color;
    EXPECT_THROW((void)color[-1], std::out_of_range);
    EXPECT_THROW((void)color[4], std::out_of_range);
    const FloatRGBA& readOnly = color;
    EXPECT_THROW((void)readOnly[-1], std::out_of_range);
    EXPECT_THROW((void)readOnly[4], std::out_of_range);
}

TEST(FloatRGBAContractTest, ArithmeticAppliesToAlphaAsWellAsRgb)
{
    const FloatRGBA lhs(0.2f, 0.4f, 0.6f, 0.8f);
    const FloatRGBA rhs(0.1f, 0.2f, 0.3f, 0.4f);
    const auto expectNear = [](const FloatRGBA& actual, const FloatRGBA& expected) {
        EXPECT_NEAR(actual.r(), expected.r(), 1e-6f);
        EXPECT_NEAR(actual.g(), expected.g(), 1e-6f);
        EXPECT_NEAR(actual.b(), expected.b(), 1e-6f);
        EXPECT_NEAR(actual.a(), expected.a(), 1e-6f);
    };
    expectNear(lhs + rhs, FloatRGBA(0.3f, 0.6f, 0.9f, 1.2f));
    expectNear(lhs - rhs, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
    expectNear(lhs * rhs, FloatRGBA(0.02f, 0.08f, 0.18f, 0.32f));
    expectNear(lhs * 2.0f, FloatRGBA(0.4f, 0.8f, 1.2f, 1.6f));
    expectNear(lhs / 2.0f, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
}

TEST(FloatRGBAContractTest, ColorDivisionIsComponentwiseAndKeepsIeeeZeroResults)
{
    const FloatRGBA numerator(6.0f, -6.0f, 0.0f, 8.0f);
    const FloatRGBA denominator(2.0f, 3.0f, 0.0f, -0.0f);
    const FloatRGBA result = numerator / denominator;

    EXPECT_FLOAT_EQ(result.r(), 3.0f);
    EXPECT_FLOAT_EQ(result.g(), -2.0f);
    EXPECT_TRUE(std::isnan(result.b()));
    EXPECT_TRUE(std::isinf(result.a()));
    EXPECT_TRUE(std::signbit(result.a()));
}

TEST(FloatRGBAContractTest, CompoundOperationsAndUnaryMinusPreserveChannelOrder)
{
    FloatRGBA color(0.2f, 0.4f, 0.6f, 0.8f);
    color += FloatRGBA(0.1f, 0.1f, 0.1f, 0.1f);
    color *= FloatRGBA(2.0f, 1.0f, 0.5f, 0.25f);
    color /= 2.0f;
    EXPECT_NEAR(color.r(), 0.3f, 1e-6f);
    EXPECT_NEAR(color.g(), 0.25f, 1e-6f);
    EXPECT_NEAR(color.b(), 0.175f, 1e-6f);
    EXPECT_NEAR(color.a(), 0.1125f, 1e-6f);
    const FloatRGBA negated = -color;
    EXPECT_NEAR(negated.r(), -0.3f, 1e-6f);
    EXPECT_NEAR(negated.g(), -0.25f, 1e-6f);
    EXPECT_NEAR(negated.b(), -0.175f, 1e-6f);
    EXPECT_NEAR(negated.a(), -0.1125f, 1e-6f);
}

TEST(FloatRGBAContractTest, LerpClampsParameterAndInterpolatesAlpha)
{
    const FloatRGBA from(0.0f, 0.2f, 0.4f, 0.1f);
    const FloatRGBA to(1.0f, 0.8f, 0.6f, 0.9f);
    EXPECT_EQ(FloatRGBA::lerp(from, to, 0.5f), FloatRGBA(0.5f, 0.5f, 0.5f, 0.5f));
    EXPECT_EQ(FloatRGBA::lerp(from, to, -1.0f), from);
    EXPECT_EQ(FloatRGBA::lerp(from, to, 2.0f), to);
}

TEST(FloatRGBAContractTest, LerpPreservesHdrAndOutOfRangeChannelValues)
{
    const FloatRGBA from(-0.5f, 0.25f, 4.0f, -1.0f);
    const FloatRGBA to(1.5f, 1.25f, 0.0f, 2.0f);

    EXPECT_EQ(FloatRGBA::lerp(from, to, 0.25f),
              FloatRGBA(0.0f, 0.5f, 3.0f, -0.25f));
}

TEST(FloatRGBAContractTest, LerpClampsInfiniteParametersToEndpoints)
{
    const FloatRGBA from(0.1f, 0.2f, 0.3f, 0.4f);
    const FloatRGBA to(0.6f, 0.7f, 0.8f, 0.9f);

    EXPECT_EQ(FloatRGBA::lerp(
                  from, to, -std::numeric_limits<float>::infinity()),
              from);
    EXPECT_EQ(FloatRGBA::lerp(
                  from, to, std::numeric_limits<float>::infinity()),
              to);
}

TEST(FloatRGBAContractTest, LerpPropagatesNaNParameterToEachChannel)
{
    const FloatRGBA from(0.1f, 0.2f, 0.3f, 0.4f);
    const FloatRGBA to(0.6f, 0.7f, 0.8f, 0.9f);
    const FloatRGBA result = FloatRGBA::lerp(
        from, to, std::numeric_limits<float>::quiet_NaN());

    EXPECT_TRUE(std::isnan(result.r()));
    EXPECT_TRUE(std::isnan(result.g()));
    EXPECT_TRUE(std::isnan(result.b()));
    EXPECT_TRUE(std::isnan(result.a()));
}

TEST(FloatRGBAContractTest, FloatColorConversionPreservesAllFourComponents)
{
    const FloatRGBA rgba(0.15f, 0.35f, 0.65f, 0.45f);
    const FloatColor converted = static_cast<FloatColor>(rgba);
    EXPECT_FLOAT_EQ(converted.r(), rgba.r());
    EXPECT_FLOAT_EQ(converted.g(), rgba.g());
    EXPECT_FLOAT_EQ(converted.b(), rgba.b());
    EXPECT_FLOAT_EQ(converted.a(), rgba.a());
}

TEST(FloatRGBAContractTest, SwapExchangesEveryChannelAndEqualityTracksAllChannels)
{
    FloatRGBA lhs(0.1f, 0.2f, 0.3f, 0.4f);
    FloatRGBA rhs(0.5f, 0.6f, 0.7f, 0.8f);
    lhs.swap(rhs);
    EXPECT_EQ(lhs, FloatRGBA(0.5f, 0.6f, 0.7f, 0.8f));
    EXPECT_EQ(rhs, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
    EXPECT_NE(lhs, FloatRGBA(0.5f, 0.6f, 0.7f, 0.0f));
}

TEST(FloatRGBAContractTest, EveryCompoundArithmeticFormUpdatesAllFourChannels)
{
    FloatRGBA color(0.8f, 0.6f, 0.4f, 0.2f);
    color -= FloatRGBA(0.1f, 0.2f, 0.3f, 0.1f);
    EXPECT_NEAR(color.r(), 0.7f, 1e-6f);
    EXPECT_NEAR(color.g(), 0.4f, 1e-6f);
    EXPECT_NEAR(color.b(), 0.1f, 1e-6f);
    EXPECT_NEAR(color.a(), 0.1f, 1e-6f);
    color *= 2.0f;
    EXPECT_NEAR(color.r(), 1.4f, 1e-6f);
    EXPECT_NEAR(color.g(), 0.8f, 1e-6f);
    EXPECT_NEAR(color.b(), 0.2f, 1e-6f);
    EXPECT_NEAR(color.a(), 0.2f, 1e-6f);
    color /= FloatRGBA(2.0f, 4.0f, 0.5f, 0.1f);
    EXPECT_NEAR(color.r(), 0.7f, 1e-6f);
    EXPECT_NEAR(color.g(), 0.2f, 1e-6f);
    EXPECT_NEAR(color.b(), 0.4f, 1e-6f);
    EXPECT_NEAR(color.a(), 2.0f, 1e-6f);
}

TEST(FloatRGBAContractTest, ConstIndexingReadsWithoutChangingTheSource)
{
    const FloatRGBA color(0.125f, 0.25f, 0.5f, 1.0f);
    EXPECT_FLOAT_EQ(color[0], 0.125f);
    EXPECT_FLOAT_EQ(color[1], 0.25f);
    EXPECT_FLOAT_EQ(color[2], 0.5f);
    EXPECT_FLOAT_EQ(color[3], 1.0f);
    EXPECT_EQ(color, FloatRGBA(0.125f, 0.25f, 0.5f, 1.0f));
}

TEST(FloatRGBAContractTest, FloatColorSetterCopiesAllChannelsIncludingAlpha)
{
    FloatRGBA color;
    color.setFromFloatColor(FloatColor(0.31f, 0.42f, 0.53f, 0.64f));
    EXPECT_FLOAT_EQ(color.r(), 0.31f);
    EXPECT_FLOAT_EQ(color.g(), 0.42f);
    EXPECT_FLOAT_EQ(color.b(), 0.53f);
    EXPECT_FLOAT_EQ(color.a(), 0.64f);
}

TEST(FloatRGBAContractTest, CopyAndMoveAssignmentsRetainTheFourChannels)
{
    const FloatRGBA source(0.11f, 0.22f, 0.33f, 0.44f);
    FloatRGBA copied;
    copied = source;
    EXPECT_EQ(copied, source);

    FloatRGBA moved;
    moved = FloatRGBA(0.55f, 0.66f, 0.77f, 0.88f);
    EXPECT_EQ(moved, FloatRGBA(0.55f, 0.66f, 0.77f, 0.88f));
}

TEST(FloatRGBAContractTest, CopyAndMoveConstructionRetainTheFourChannels)
{
    const FloatRGBA source(0.13f, 0.24f, 0.35f, 0.46f);
    const FloatRGBA copied(source);
    FloatRGBA moved(std::move(source));

    EXPECT_EQ(copied, FloatRGBA(0.13f, 0.24f, 0.35f, 0.46f));
    EXPECT_EQ(moved, FloatRGBA(0.13f, 0.24f, 0.35f, 0.46f));
}
