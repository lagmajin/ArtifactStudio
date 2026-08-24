#include <gtest/gtest.h>
#include <QJsonObject>
#include <QString>
#include <cmath>
#include <limits>

import Frame.Position;
import Frame.Range;
import Frame.Rate;
import Frame.Offset;
import Time.Code;
import Time.Rational;

using namespace ArtifactCore;

namespace {

constexpr double kAbsError = 1e-6;

}

TEST(FrameRateTest, KeepsIntegerFractionExactly)
{
    const auto rate = FrameRate::fromRational(30000, 1001);
    ASSERT_TRUE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 30000);
    EXPECT_EQ(rate.denominator(), 1001);
    EXPECT_NEAR(rate.exactFps(), 29.97003, kAbsError);
    EXPECT_TRUE(rate.hasDropframe());

    // Fraction strings stay exact instead of collapsing to float.
    FrameRate parsed(QStringLiteral("30000/1001"));
    EXPECT_TRUE(parsed.hasExactRational());
    EXPECT_EQ(parsed.denominator(), 1001);

    // Plain float assignment drops exactness.
    FrameRate plain(30.0f);
    EXPECT_FALSE(plain.hasExactRational());
    EXPECT_FALSE(plain.hasDropframe());

    // Float-proximity detection still covers 23.976 media.
    FrameRate ntscFilm(23.976f);
    EXPECT_TRUE(ntscFilm.hasDropframe());
}

TEST(FrameRateTest, JsonRoundTripPreservesRational)
{
    const auto original = FrameRate::fromRational(30000, 1001);
    QJsonObject json;
    original.writeToJson(json);

    FrameRate restored;
    restored.setFromJson(json);
    ASSERT_TRUE(restored.hasExactRational());
    EXPECT_EQ(restored.numerator(), 30000);
    EXPECT_EQ(restored.denominator(), 1001);
    EXPECT_DOUBLE_EQ(restored.exactFps(), original.exactFps());
}

TEST(FramePositionTest, ConvertsToAndFromRationalTime)
{
    const auto rate = FrameRate::fromRational(30000, 1001);
    const FramePosition position(1001);

    const auto time = position.toRationalTime(rate);
    EXPECT_EQ(time.value(), 1001 * 1001);
    EXPECT_EQ(time.scale(), 30000);
    EXPECT_NEAR(time.toSeconds(), 33.3667, 1e-3);

    const auto restored = FramePosition::fromRationalTime(time, rate);
    EXPECT_EQ(restored.framePosition(), 1001);

    // Non-exact rates fall back to the rounded nominal fps.
    FrameRate plain(30.0f);
    const FramePosition frames(90);
    EXPECT_EQ(frames.toRationalTime(plain).value(), 90);
    EXPECT_EQ(frames.toRationalTime(plain).scale(), 30);
    EXPECT_EQ(FramePosition::fromRationalTime(RationalTime(45, 15), plain)
                  .framePosition(),
              90);
}

TEST(FramePositionTest, HashMatchesEqualPositions)
{
    const FramePosition a(5);
    const FramePosition b(5);
    const FramePosition c(7);
    EXPECT_EQ(qHash(a), qHash(b));
    EXPECT_NE(qHash(a), qHash(c));
}

TEST(TimeCodeTest, DropFrameUsesSemicolonSeparator)
{
    TimeCode code(107892, 29.97);
    code.setDropFrame(true);
    EXPECT_EQ(code.toString(), QStringLiteral("01:00:00;00"));

    TimeCode parsed(0, 29.97);
    parsed.setDropFrame(true);
    parsed.setFromQString(QStringLiteral("01:00:00;00"));
    EXPECT_EQ(parsed.frame(), 107892);

    // Non-drop output keeps ':'.
    TimeCode plain(900, 30.0);
    EXPECT_EQ(plain.toString(), QStringLiteral("00:00:30:00"));
}

TEST(FrameRangeTest, ToTimecodeDelegatesToTimeCodeSemantics)
{
    const FrameRange range(600, 1200);
    EXPECT_EQ(range.toTimecode(30.0),
              QStringLiteral("00:00:20:00 - 00:00:40:00"));

    const auto drop = FrameRate::fromRational(30000, 1001);
    const FrameRange hourRange(0, 107892);
    EXPECT_EQ(hourRange.toTimecode(drop),
              QStringLiteral("00:00:00;00 - 01:00:00;00"));
}

TEST(FrameRangeTest, RangeOperationsStayConsistent)
{
    const FrameRange range = FrameRange::fromDuration(10, 20);
    EXPECT_EQ(range.start(), 10);
    EXPECT_EQ(range.end(), 30);
    EXPECT_TRUE(range.contains(15));
    EXPECT_TRUE(range.overlaps(FrameRange(25, 40)));
    EXPECT_FALSE(range.overlaps(FrameRange(31, 40)));

    const auto united = range.united(FrameRange(40, 50));
    EXPECT_EQ(united.start(), 10);
    EXPECT_EQ(united.end(), 50);

    const auto intersection = range.intersected(FrameRange(0, 15));
    EXPECT_EQ(intersection.start(), 10);
    EXPECT_EQ(intersection.end(), 15);

    EXPECT_EQ(range.durationSeconds(30.0), 20.0 / 30.0);
}

TEST(RationalTimeTest, RescaledToRoundsHalfAwayFromZero)
{
    // 1/8 s at 30 fps = 3.75 frames; rounding picks 4 (truncation gave 3).
    EXPECT_EQ(RationalTime(1, 8).toFrameCount(30), 4);
    EXPECT_EQ(RationalTime(-1, 8).toFrameCount(30), -4);
    // Exact conversions stay exact.
    EXPECT_EQ(RationalTime(1, 3).toFrameCount(30), 10);
    EXPECT_EQ(RationalTime(45, 15).rescaledTo(30), 90);
}

TEST(RationalTimeTest, CrossScaleAdditionStaysExactAndSafe)
{
    const auto a = RationalTime::fromSeconds(1.5);
    const auto b = RationalTime::fromSeconds(2.25);
    const auto sum = a + b;
    EXPECT_NEAR(sum.toSeconds(), 3.75, 1e-9);

    // Same-scale arithmetic is unchanged.
    const auto same = RationalTime(100, 30) + RationalTime(50, 30);
    EXPECT_EQ(same.value(), 150);
    EXPECT_EQ(same.scale(), 30);

    // Extreme magnitudes must not wrap around int64.
    const auto huge1 = RationalTime(std::numeric_limits<std::int64_t>::max() / 2,
                                    10000007);
    const auto huge2 = RationalTime(std::numeric_limits<std::int64_t>::max() / 2,
                                    10000009);
    const auto safeSum = huge1 + huge2;
    EXPECT_NEAR(safeSum.toSeconds(), huge1.toSeconds() + huge2.toSeconds(),
                std::abs(huge1.toSeconds()) * 1e-12);

    const auto difference = huge1 - huge2;
    EXPECT_NEAR(difference.toSeconds(),
                huge1.toSeconds() - huge2.toSeconds(), 1e-6);
}

TEST(FrameOffsetTest, ArithmeticAndTimeConversion)
{
    const FrameOffset offset(12);
    EXPECT_EQ(offset.value(), 12);
    EXPECT_TRUE(offset.isPositive());
    EXPECT_TRUE((-offset).isNegative());
    EXPECT_EQ((offset * 2).value(), 24);
    EXPECT_EQ(offset.abs().value(), 12);

    EXPECT_NEAR(offset.toTimeSeconds(FrameRate(24.0f)), 0.5, kAbsError);
}
