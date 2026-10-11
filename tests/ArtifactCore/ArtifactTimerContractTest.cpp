#include <gtest/gtest.h>

#include <QElapsedTimer>

#include <limits>

import Core.ArtifactTimer;

using ArtifactCore::ArtifactTimer;

TEST(ArtifactTimerContractTest, DefaultTimerIsInvalid)
{
    const ArtifactTimer timer;
    EXPECT_FALSE(timer.isValid());
}

TEST(ArtifactTimerContractTest, StartMakesTimerValid)
{
    ArtifactTimer timer;
    timer.start();
    EXPECT_TRUE(timer.isValid());
}

TEST(ArtifactTimerContractTest, ElapsedValuesAreNonNegativeAfterStart)
{
    ArtifactTimer timer;
    timer.start();
    EXPECT_GE(timer.elapsedMs(), 0);
    EXPECT_GE(timer.elapsedNs(), 0);
}

TEST(ArtifactTimerContractTest, ElapsedMillisecondsAreMonotonic)
{
    ArtifactTimer timer;
    timer.start();
    const qint64 first = timer.elapsedMs();
    const qint64 second = timer.elapsedMs();
    EXPECT_GE(second, first);
}

TEST(ArtifactTimerContractTest, ElapsedNanosecondsAreMonotonic)
{
    ArtifactTimer timer;
    timer.start();
    const qint64 first = timer.elapsedNs();
    const qint64 second = timer.elapsedNs();
    EXPECT_GE(second, first);
}

TEST(ArtifactTimerContractTest, MillisecondElapsedDoesNotExceedLaterNanosecondElapsed)
{
    ArtifactTimer timer;
    timer.start();
    const qint64 elapsedMilliseconds = timer.elapsedMs();
    const qint64 elapsedNanoseconds = timer.elapsedNs();

    EXPECT_GE(elapsedNanoseconds, elapsedMilliseconds * 1'000'000);
}

TEST(ArtifactTimerContractTest, ZeroTimeoutIsAlreadyExpiredAfterStart)
{
    ArtifactTimer timer;
    timer.start();
    EXPECT_TRUE(timer.hasExpired(0));
}

TEST(ArtifactTimerContractTest, TimeoutEqualToObservedElapsedTimeIsExpired)
{
    ArtifactTimer timer;
    timer.start();
    const qint64 elapsed = timer.elapsedMs();

    EXPECT_TRUE(timer.hasExpired(elapsed));
}

TEST(ArtifactTimerContractTest, ZeroTimeoutIsNotExpiredBeforeTimerStarts)
{
    const ArtifactTimer timer;
    EXPECT_FALSE(timer.hasExpired(0));
}

TEST(ArtifactTimerContractTest, PositiveTimeoutIsNotExpiredBeforeTimerStarts)
{
    const ArtifactTimer timer;
    EXPECT_FALSE(timer.hasExpired(10));
}

TEST(ArtifactTimerContractTest, NegativeTimeoutIsNotExpiredBeforeTimerStarts)
{
    const ArtifactTimer timer;
    EXPECT_FALSE(timer.hasExpired(-1));
}

TEST(ArtifactTimerContractTest, NegativeTimeoutIsExpiredAfterStart)
{
    ArtifactTimer timer;
    timer.start();
    EXPECT_TRUE(timer.hasExpired(-1));
}

TEST(ArtifactTimerContractTest, MaximumTimeoutIsNotImmediatelyExpired)
{
    ArtifactTimer timer;
    timer.start();
    EXPECT_FALSE(timer.hasExpired(std::numeric_limits<qint64>::max()));
}

TEST(ArtifactTimerContractTest, RestartKeepsTimerValid)
{
    ArtifactTimer timer;
    timer.start();
    const qint64 beforeRestart = timer.elapsedNs();
    timer.restart();
    EXPECT_TRUE(timer.isValid());
    EXPECT_GE(timer.elapsedNs(), 0);
    EXPECT_GE(beforeRestart, 0);
}

TEST(ArtifactTimerContractTest, RestartOnInvalidTimerStartsIt)
{
    ArtifactTimer timer;
    EXPECT_FALSE(timer.isValid());
    timer.restart();
    EXPECT_TRUE(timer.isValid());
}
