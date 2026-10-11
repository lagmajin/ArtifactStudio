#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>

import Utils.Numeric;

using namespace ArtifactCore;

static_assert(min_same(4, 9) == 4);
static_assert(max_same(-4, 9) == 9);
static_assert(std::is_same_v<decltype(min_same(1, 2.0)), double>);
static_assert(std::is_same_v<decltype(max_same(short{1}, 2)), int>);

TEST(NumericContractTest, MinAndMaxSelectValuesForAscendingInputs)
{
    EXPECT_EQ(min_same(2, 7), 2);
    EXPECT_EQ(max_same(2, 7), 7);
}

TEST(NumericContractTest, MinAndMaxSelectValuesForDescendingInputs)
{
    EXPECT_EQ(min_same(7, 2), 2);
    EXPECT_EQ(max_same(7, 2), 7);
}

TEST(NumericContractTest, EqualInputsReturnTheSharedValue)
{
    EXPECT_EQ(min_same(5, 5), 5);
    EXPECT_EQ(max_same(5, 5), 5);
}

TEST(NumericContractTest, NegativeValuesKeepSignedOrdering)
{
    EXPECT_EQ(min_same(-8, -3), -8);
    EXPECT_EQ(max_same(-8, -3), -3);
    EXPECT_EQ(min_same(-2, 6), -2);
    EXPECT_EQ(max_same(-2, 6), 6);
}

TEST(NumericContractTest, MixedIntegerAndFloatingPointInputsUseCommonType)
{
    const auto minimum = min_same(3, 2.5);
    const auto maximum = max_same(3, 2.5);
    static_assert(std::is_same_v<decltype(minimum), const double>);
    static_assert(std::is_same_v<decltype(maximum), const double>);
    EXPECT_DOUBLE_EQ(minimum, 2.5);
    EXPECT_DOUBLE_EQ(maximum, 3.0);
}

TEST(NumericContractTest, MixedWidthsCompareAfterCommonTypeConversion)
{
    const short narrow = 120;
    const long wide = 300;
    EXPECT_EQ(min_same(narrow, wide), 120L);
    EXPECT_EQ(max_same(narrow, wide), 300L);
}

TEST(NumericContractTest, UnsignedInputsRetainUnsignedResults)
{
    const unsigned short first = 4;
    const unsigned int second = 9;
    const auto minimum = min_same(first, second);
    const auto maximum = max_same(first, second);
    static_assert(std::is_same_v<decltype(minimum), const unsigned int>);
    static_assert(std::is_same_v<decltype(maximum), const unsigned int>);
    EXPECT_EQ(minimum, 4u);
    EXPECT_EQ(maximum, 9u);
}

TEST(NumericContractTest, FloatingPointInputsKeepFractionalOrdering)
{
    EXPECT_DOUBLE_EQ(min_same(0.125, 0.75), 0.125);
    EXPECT_DOUBLE_EQ(max_same(0.125, 0.75), 0.75);
}

TEST(NumericContractTest, InputsAreNotModified)
{
    const int first = 11;
    const double second = 4.5;
    (void)min_same(first, second);
    (void)max_same(first, second);
    EXPECT_EQ(first, 11);
    EXPECT_DOUBLE_EQ(second, 4.5);
}

TEST(NumericContractTest, IntegralLimitsRemainOrdered)
{
    constexpr int lowest = std::numeric_limits<int>::lowest();
    constexpr int highest = std::numeric_limits<int>::max();
    static_assert(min_same(highest, lowest) == lowest);
    static_assert(max_same(lowest, highest) == highest);
    EXPECT_EQ(min_same(highest, lowest), lowest);
    EXPECT_EQ(max_same(lowest, highest), highest);
}

TEST(NumericContractTest, InfiniteValuesFollowFloatingPointOrdering)
{
    const double infinity = std::numeric_limits<double>::infinity();
    EXPECT_DOUBLE_EQ(min_same(infinity, -infinity), -infinity);
    EXPECT_DOUBLE_EQ(max_same(-infinity, infinity), infinity);
    EXPECT_DOUBLE_EQ(min_same(4.0, infinity), 4.0);
    EXPECT_DOUBLE_EQ(max_same(4.0, -infinity), 4.0);
}

TEST(NumericContractTest, SmallestPositiveSubnormalIsNotRoundedAway)
{
    const double smallest = std::numeric_limits<double>::denorm_min();
    ASSERT_GT(smallest, 0.0);
    EXPECT_DOUBLE_EQ(min_same(smallest, 1.0), smallest);
    EXPECT_DOUBLE_EQ(max_same(0.0, smallest), smallest);
}

TEST(NumericContractTest, EqualMixedTypeValuesKeepCommonType)
{
    const auto minimum = min_same(7, 7.0);
    const auto maximum = max_same(7.0, 7);
    static_assert(std::is_same_v<decltype(minimum), const double>);
    static_assert(std::is_same_v<decltype(maximum), const double>);
    EXPECT_DOUBLE_EQ(minimum, 7.0);
    EXPECT_DOUBLE_EQ(maximum, 7.0);
}

TEST(NumericContractTest, EqualSignedZeroKeepsFirstOperandRepresentation)
{
    const double negativeZero = std::copysign(0.0, -1.0);
    const double positiveZero = 0.0;

    EXPECT_TRUE(std::signbit(min_same(negativeZero, positiveZero)));
    EXPECT_TRUE(std::signbit(max_same(negativeZero, positiveZero)));
    EXPECT_FALSE(std::signbit(min_same(positiveZero, negativeZero)));
    EXPECT_FALSE(std::signbit(max_same(positiveZero, negativeZero)));
}

TEST(NumericContractTest, NaNOrderingFollowsStandardMinMaxFirstOperandRules)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr double finite = 3.5;

    EXPECT_TRUE(std::isnan(min_same(nan, finite)));
    EXPECT_TRUE(std::isnan(max_same(nan, finite)));
    EXPECT_DOUBLE_EQ(min_same(finite, nan), finite);
    EXPECT_DOUBLE_EQ(max_same(finite, nan), finite);
}
