#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

import Core.ArtifactChrono;

using ArtifactCore::Duration;
using ArtifactCore::SteadyClock;
using ArtifactCore::Stopwatch;

TEST(ArtifactChronoContractTest, DefaultDurationIsZero)
{
    constexpr Duration duration;
    EXPECT_EQ(duration.nanos(), 0);
    EXPECT_EQ(duration, Duration::fromNanos(0));
}

TEST(ArtifactChronoContractTest, NanosecondFactoryPreservesInt64Endpoints)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

    EXPECT_EQ(Duration::fromNanos(minimum).nanos(), minimum);
    EXPECT_EQ(Duration::fromNanos(maximum).nanos(), maximum);
}

TEST(ArtifactChronoContractTest, UnitFactoriesConvertToNanoseconds)
{
    EXPECT_EQ(Duration::fromMicros(2.5).nanos(), 2'500);
    EXPECT_EQ(Duration::fromMillis(1.25).nanos(), 1'250'000);
    EXPECT_EQ(Duration::fromSeconds(1.5).nanos(), 1'500'000'000);
}

TEST(ArtifactChronoContractTest, UnitConversionsReturnExpectedFractions)
{
    constexpr auto duration = Duration::fromNanos(1'234'567'890);
    EXPECT_DOUBLE_EQ(duration.toMicros(), 1'234'567.89);
    EXPECT_DOUBLE_EQ(duration.toMillis(), 1'234.56789);
    EXPECT_DOUBLE_EQ(duration.toSeconds(), 1.23456789);
}

TEST(ArtifactChronoContractTest, UnitConversionsPreserveNegativeFractionalNanoseconds)
{
    constexpr auto duration = Duration::fromNanos(-1);

    EXPECT_DOUBLE_EQ(duration.toMicros(), -0.001);
    EXPECT_DOUBLE_EQ(duration.toMillis(), -0.000001);
    EXPECT_DOUBLE_EQ(duration.toSeconds(), -0.000000001);
}

TEST(ArtifactChronoContractTest, MaximumDurationConversionsRemainFiniteAndPositive)
{
    constexpr auto duration = Duration::fromNanos(std::numeric_limits<std::int64_t>::max());

    EXPECT_GT(duration.toMicros(), 9.22e15);
    EXPECT_LT(duration.toMicros(), 9.23e15);
    EXPECT_GT(duration.toMillis(), 9.22e12);
    EXPECT_LT(duration.toMillis(), 9.23e12);
    EXPECT_GT(duration.toSeconds(), 9.22e9);
    EXPECT_LT(duration.toSeconds(), 9.23e9);
}

TEST(ArtifactChronoContractTest, MinimumDurationConversionsRemainFiniteAndNegative)
{
    constexpr auto duration = Duration::fromNanos(std::numeric_limits<std::int64_t>::min());

    EXPECT_LT(duration.toMicros(), -9.22e15);
    EXPECT_GT(duration.toMicros(), -9.23e15);
    EXPECT_LT(duration.toMillis(), -9.22e12);
    EXPECT_GT(duration.toMillis(), -9.23e12);
    EXPECT_LT(duration.toSeconds(), -9.22e9);
    EXPECT_GT(duration.toSeconds(), -9.23e9);
}

TEST(ArtifactChronoContractTest, FloatingFactoriesRoundHalfAwayFromZero)
{
    EXPECT_EQ(Duration::fromNanos(0).nanos(), 0);
    EXPECT_EQ(Duration::fromMicros(0.0005).nanos(), 1);
    EXPECT_EQ(Duration::fromMicros(-0.0005).nanos(), -1);
}

TEST(ArtifactChronoContractTest, AdditionAndSubtractionPreserveSignedValues)
{
    constexpr auto a = Duration::fromNanos(19);
    constexpr auto b = Duration::fromNanos(-7);
    EXPECT_EQ((a + b).nanos(), 12);
    EXPECT_EQ((a - b).nanos(), 26);
}

TEST(ArtifactChronoContractTest, CompoundArithmeticUpdatesDuration)
{
    auto duration = Duration::fromNanos(20);
    duration += Duration::fromNanos(5);
    duration -= Duration::fromNanos(9);
    EXPECT_EQ(duration.nanos(), 16);
}

TEST(ArtifactChronoContractTest, ScalarMultiplicationTruncatesTowardZero)
{
    EXPECT_EQ((Duration::fromNanos(5) * 1.5).nanos(), 7);
    EXPECT_EQ((Duration::fromNanos(-5) * 1.5).nanos(), -7);
}

TEST(ArtifactChronoContractTest, DivisionTruncatesTowardZero)
{
    EXPECT_EQ((Duration::fromNanos(7) / 2).nanos(), 3);
    EXPECT_EQ((Duration::fromNanos(-7) / 2).nanos(), -3);
}

TEST(ArtifactChronoContractTest, DivisionByZeroReturnsZeroDuration)
{
    EXPECT_EQ((Duration::fromNanos(17) / 0).nanos(), 0);
}

TEST(ArtifactChronoContractTest, RelationalOperatorsOrderSignedDurations)
{
    constexpr auto negative = Duration::fromNanos(-1);
    constexpr auto zero = Duration::fromNanos(0);
    constexpr auto positive = Duration::fromNanos(1);
    EXPECT_LT(negative, zero);
    EXPECT_LE(zero, zero);
    EXPECT_GT(positive, zero);
    EXPECT_GE(zero, negative);
    EXPECT_NE(negative, zero);
    EXPECT_EQ(zero, Duration{});
}

TEST(ArtifactChronoContractTest, BetweenConvertsNanosecondTickDelta)
{
    const auto oneSecondInTicks = static_cast<SteadyClock::Tick>(
        1.0 / SteadyClock::secondsPerTick());
    EXPECT_NEAR(SteadyClock::between(0, oneSecondInTicks).toSeconds(), 1.0, 1e-6);
}

TEST(ArtifactChronoContractTest, BetweenClampsReversedTicksToZero)
{
    EXPECT_EQ(SteadyClock::between(100, 99).nanos(), 0);
    EXPECT_EQ(SteadyClock::between(100, 100).nanos(), 0);
}

TEST(ArtifactChronoContractTest, BetweenSaturatesUnrepresentablePositiveTickDelta)
{
    constexpr auto minimum = std::numeric_limits<SteadyClock::Tick>::min();
    constexpr auto maximum = std::numeric_limits<SteadyClock::Tick>::max();

    EXPECT_EQ(SteadyClock::between(minimum, maximum).nanos(), maximum);
}

TEST(ArtifactChronoContractTest, BetweenClampsExtremeReversedTickRangeToZero)
{
    constexpr auto minimum = std::numeric_limits<SteadyClock::Tick>::min();
    constexpr auto maximum = std::numeric_limits<SteadyClock::Tick>::max();

    EXPECT_EQ(SteadyClock::between(maximum, minimum).nanos(), 0);
}

TEST(ArtifactChronoContractTest, ClockScaleIsPositive)
{
    EXPECT_GT(SteadyClock::secondsPerTick(), 0.0);
}

TEST(ArtifactChronoContractTest, ClockTicksDoNotMoveBackwards)
{
    const auto first = SteadyClock::now();
    const auto second = SteadyClock::now();
    EXPECT_GE(second, first);
}

TEST(ArtifactChronoContractTest, DefaultStopwatchIsStoppedAtZero)
{
    const Stopwatch stopwatch;
    EXPECT_FALSE(stopwatch.isRunning());
    EXPECT_EQ(stopwatch.elapsed(), Duration{});
}

TEST(ArtifactChronoContractTest, StopOnUnstartedStopwatchKeepsZeroStoppedState)
{
    Stopwatch stopwatch;

    stopwatch.stop();

    EXPECT_FALSE(stopwatch.isRunning());
    EXPECT_EQ(stopwatch.elapsed(), Duration{});
}

TEST(ArtifactChronoContractTest, StartIsIdempotentWhileRunning)
{
    Stopwatch stopwatch;
    stopwatch.start();
    const auto before = stopwatch.elapsed();
    stopwatch.start();
    EXPECT_TRUE(stopwatch.isRunning());
    EXPECT_GE(stopwatch.elapsed(), before);
}

TEST(ArtifactChronoContractTest, StopFreezesAccumulatedDuration)
{
    Stopwatch stopwatch;
    stopwatch.start();
    stopwatch.stop();
    ASSERT_FALSE(stopwatch.isRunning());
    const auto stopped = stopwatch.elapsed();
    stopwatch.stop();
    EXPECT_EQ(stopwatch.elapsed(), stopped);
}

TEST(ArtifactChronoContractTest, ResetClearsAccumulationAndStops)
{
    Stopwatch stopwatch;
    stopwatch.start();
    stopwatch.stop();
    stopwatch.reset();
    EXPECT_FALSE(stopwatch.isRunning());
    EXPECT_EQ(stopwatch.elapsed(), Duration{});
}

TEST(ArtifactChronoContractTest, ResetWhileRunningClearsAndStops)
{
    Stopwatch stopwatch;
    stopwatch.start();
    ASSERT_TRUE(stopwatch.isRunning());

    stopwatch.reset();

    EXPECT_FALSE(stopwatch.isRunning());
    EXPECT_EQ(stopwatch.elapsed(), Duration{});
}

TEST(ArtifactChronoContractTest, RestartClearsAccumulationAndStarts)
{
    Stopwatch stopwatch;
    stopwatch.start();
    stopwatch.stop();
    stopwatch.restart();
    EXPECT_TRUE(stopwatch.isRunning());
    EXPECT_GE(stopwatch.elapsed().nanos(), 0);
}

TEST(ArtifactChronoContractTest, StartStopCyclesKeepAccumulatedElapsedMonotonic)
{
    Stopwatch stopwatch;
    stopwatch.start();
    stopwatch.stop();
    const Duration firstCycle = stopwatch.elapsed();

    stopwatch.start();
    stopwatch.stop();

    EXPECT_GE(stopwatch.elapsed(), firstCycle);
    EXPECT_FALSE(stopwatch.isRunning());
}
