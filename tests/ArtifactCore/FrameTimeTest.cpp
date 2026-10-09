#include <gtest/gtest.h>
#include <QJsonObject>
#include <QString>
#include <cstdint>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

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

TEST(FrameRateTest, RecognizesClassicFortyEightAndSixtyNominalDropFrameRates)
{
    EXPECT_TRUE(FrameRate::fromRational(48000, 1001).hasDropframe());
    EXPECT_TRUE(FrameRate::fromRational(60000, 1001).hasDropframe());
    EXPECT_FALSE(FrameRate::fromRational(60000, 1000).hasDropframe());
    EXPECT_FALSE(FrameRate(60.0f).hasDropframe());
}

TEST(FrameRateTest, InvalidRationalUpdatesPreserveExistingExactRate)
{
    FrameRate rate = FrameRate::fromRational(24000, 1001);

    rate.setRationalRate(0, 1001);
    EXPECT_TRUE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 24000);
    EXPECT_EQ(rate.denominator(), 1001);
    rate.setRationalRate(-1, 1001);
    EXPECT_EQ(rate.numerator(), 24000);
    rate.setRationalRate(24000, 0);
    EXPECT_EQ(rate.denominator(), 1001);
    rate.setRationalRate(24000, -1);

    EXPECT_TRUE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 24000);
    EXPECT_EQ(rate.denominator(), 1001);
    EXPECT_NEAR(rate.exactFps(), 24000.0 / 1001.0, kAbsError);
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

TEST(FrameRateTest, CopiesPreserveExactRationalUntilFloatAssignment)
{
    const FrameRate original = FrameRate::fromRational(24000, 1001);
    FrameRate copied(original);
    FrameRate assigned;
    assigned = original;

    for (const FrameRate* rate : {&copied, &assigned}) {
        EXPECT_TRUE(rate->hasExactRational());
        EXPECT_EQ(rate->numerator(), 24000);
        EXPECT_EQ(rate->denominator(), 1001);
        EXPECT_TRUE(rate->hasDropframe());
    }

    assigned = 24.0f;
    EXPECT_FALSE(assigned.hasExactRational());
    EXPECT_FALSE(assigned.hasDropframe());
    EXPECT_TRUE(original.hasExactRational());
}

TEST(FrameRateTest, ParsesDecoratedRationalRateWithoutLosingExactness)
{
    const FrameRate parsed(QStringLiteral(" 24000 / 1001 fps DF "));

    EXPECT_TRUE(parsed.hasExactRational());
    EXPECT_EQ(parsed.numerator(), 24000);
    EXPECT_EQ(parsed.denominator(), 1001);
    EXPECT_NEAR(parsed.framerate(), 23.976f, 0.001f);
    EXPECT_TRUE(parsed.hasDropframe());
}

TEST(FrameRateTest, InvalidTextUpdatesPreserveExistingExactRate)
{
    FrameRate rate = FrameRate::fromRational(30000, 1001);
    const QString invalidValues[] = {
        QStringLiteral("not a rate"), QStringLiteral("30000/0"),
        QStringLiteral("-24/1"), QStringLiteral("0/1"),
        QStringLiteral("nan"), QStringLiteral("inf"),
    };

    for (const QString& value : invalidValues) {
        rate.setFromString(value);
        EXPECT_TRUE(rate.hasExactRational()) << value;
        EXPECT_EQ(rate.numerator(), 30000) << value;
        EXPECT_EQ(rate.denominator(), 1001) << value;
    }
}

TEST(FrameRateTest, ReadsLegacyJsonFpsAndStringValueFields)
{
    QJsonObject legacyFps;
    legacyFps.insert(QStringLiteral("fps"), 24.0);
    const FrameRate fromFps = FrameRate::fromJsonStatic(legacyFps);
    EXPECT_FLOAT_EQ(fromFps.framerate(), 24.0f);
    EXPECT_FALSE(fromFps.hasExactRational());

    QJsonObject legacyValue;
    legacyValue.insert(QStringLiteral("value"), QStringLiteral("25"));
    const FrameRate fromValue = FrameRate::fromJsonStatic(legacyValue);
    EXPECT_FLOAT_EQ(fromValue.framerate(), 25.0f);
    EXPECT_FALSE(fromValue.hasExactRational());
}

TEST(FrameRateTest, JsonPrefersExactRationalAndFrameRateOverLegacyAliases)
{
    QJsonObject exactAndLegacy;
    exactAndLegacy.insert(QStringLiteral("numerator"), 24000);
    exactAndLegacy.insert(QStringLiteral("denominator"), 1001);
    exactAndLegacy.insert(QStringLiteral("frameRate"), 60.0);
    exactAndLegacy.insert(QStringLiteral("fps"), 48.0);

    const FrameRate exact = FrameRate::fromJsonStatic(exactAndLegacy);
    ASSERT_TRUE(exact.hasExactRational());
    EXPECT_EQ(exact.numerator(), 24000);
    EXPECT_EQ(exact.denominator(), 1001);

    QJsonObject legacyAliases;
    legacyAliases.insert(QStringLiteral("frameRate"), 30.0);
    legacyAliases.insert(QStringLiteral("fps"), 24.0);

    const FrameRate fromFrameRate = FrameRate::fromJsonStatic(legacyAliases);
    EXPECT_FLOAT_EQ(fromFrameRate.framerate(), 30.0f);
    EXPECT_FALSE(fromFrameRate.hasExactRational());
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

TEST(FramePositionTest, RoundsHalfFrameTimesAwayFromZeroForExactAndNominalRates)
{
    const FrameRate exactRate = FrameRate::fromRational(30000, 1001);
    EXPECT_EQ(FramePosition::fromRationalTime(RationalTime(1001, 60000), exactRate)
                  .framePosition(),
              1);
    EXPECT_EQ(FramePosition::fromRationalTime(RationalTime(-1001, 60000), exactRate)
                  .framePosition(),
              -1);

    const FrameRate nominalRate(24.0f);
    EXPECT_EQ(FramePosition::fromRationalTime(RationalTime(1, 48), nominalRate)
                  .framePosition(),
              1);
    EXPECT_EQ(FramePosition::fromRationalTime(RationalTime(-1, 48), nominalRate)
                  .framePosition(),
              -1);
}

TEST(FramePositionTest, HashMatchesEqualPositions)
{
    const FramePosition a(5);
    const FramePosition b(5);
    const FramePosition c(7);
    EXPECT_EQ(qHash(a), qHash(b));
    EXPECT_NE(qHash(a), qHash(c));
}

TEST(FrameTimeTest, CopyAndMovePreserveValueAndLeaveMovedFromUsable)
{
    FrameTime original(-17);
    FrameTime copied(original);
    EXPECT_EQ(copied.frame(), -17);

    FrameTime moved(std::move(copied));
    EXPECT_EQ(moved.frame(), -17);
    EXPECT_EQ(copied.frame(), 0);
    copied.setFrame(9);
    EXPECT_EQ(copied.frame(), 9);

    FrameTime assigned(2);
    assigned = moved;
    EXPECT_EQ(assigned.frame(), -17);
    assigned = FrameTime(31);
    EXPECT_EQ(assigned.frame(), 31);
}

TEST(FrameTimeTest, ConvertsNegativeFramesAndRejectsInvalidFrameRates)
{
    const FrameTime frame(-12);
    const auto time = frame.toTime(24.0);
    EXPECT_EQ(time.value(), -12);
    EXPECT_EQ(time.scale(), 24);

    EXPECT_EQ(frame.toTime(0.0), RationalTime());
    EXPECT_EQ(frame.toTime(-24.0), RationalTime());
    EXPECT_EQ(frame.toTime(std::numeric_limits<double>::infinity()), RationalTime());
    EXPECT_EQ(frame.toTime(std::numeric_limits<double>::quiet_NaN()), RationalTime());
}

TEST(FrameTimeTest, ArithmeticAndComparisonsUseFrameValues)
{
    const FrameTime origin(10);
    EXPECT_EQ((origin + 3).frame(), 13);
    EXPECT_EQ((origin - 4).frame(), 6);
    EXPECT_EQ(origin.frame(), 10);

    FrameTime adjusted(10);
    adjusted += 5;
    EXPECT_EQ(adjusted.frame(), 15);
    adjusted -= 8;
    EXPECT_EQ(adjusted.frame(), 7);

    EXPECT_TRUE(FrameTime(7) == adjusted);
    EXPECT_FALSE(FrameTime(8) == adjusted);
    EXPECT_TRUE(adjusted < origin);
    EXPECT_FALSE(origin < adjusted);
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

TEST(TimeCodeTest, TrailingDotMeansZeroFrameField)
{
    TimeCode parsed(0, 30.0);
    parsed.setFromQString(QStringLiteral("00:00:01."));

    EXPECT_EQ(parsed.frame(), 30);
    EXPECT_EQ(parsed.toString(), QStringLiteral("00:00:01:00"));
}

TEST(TimeCodeTest, DropFrameTenMinuteBoundaryRoundTrips)
{
    TimeCode tenMinuteBoundary(17982, 29.97);
    tenMinuteBoundary.setDropFrame(true);
    EXPECT_EQ(tenMinuteBoundary.toString(), QStringLiteral("00:10:00;00"));

    TimeCode parsed(0, 29.97);
    parsed.setDropFrame(true);
    parsed.setFromQString(QStringLiteral("00:10:00;00"));
    EXPECT_EQ(parsed.frame(), 17982);
}

TEST(TimeCodeTest, SixtyNominalDropFrameUsesFourFrameTenMinuteCorrection)
{
    TimeCode boundary(35964, 59.94);
    boundary.setDropFrame(true);
    EXPECT_EQ(boundary.toString(), QStringLiteral("00:10:00;00"));

    TimeCode parsed(0, 59.94);
    parsed.setDropFrame(true);
    parsed.setFromQString(QStringLiteral("00:10:00;00"));
    EXPECT_EQ(parsed.frame(), 35964);
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

TEST(FrameRangeTest, InclusiveOverlapAndAdjacentTouchHaveDistinctIntersections)
{
    const FrameRange range(10, 20);
    const FrameRange sharedEndpoint(20, 30);
    const FrameRange adjacent(21, 30);

    EXPECT_TRUE(range.contains(10));
    EXPECT_TRUE(range.contains(20));
    EXPECT_TRUE(range.overlaps(sharedEndpoint));
    EXPECT_TRUE(range.touches(sharedEndpoint));
    EXPECT_EQ(range.intersected(sharedEndpoint), FrameRange(20, 20));

    EXPECT_FALSE(range.overlaps(adjacent));
    EXPECT_TRUE(range.touches(adjacent));
    EXPECT_FALSE(range.intersected(adjacent).isValid());
}

TEST(FrameRangeTest, NonMutatingShiftExpandAndShrinkKeepSourceRange)
{
    const FrameRange range(10, 20);

    EXPECT_EQ(range.shifted(-5), FrameRange(5, 15));
    EXPECT_EQ(range.expanded(2), FrameRange(8, 22));
    EXPECT_EQ(range.shrinked(2), FrameRange(12, 18));
    EXPECT_EQ(range, FrameRange(10, 20));
    EXPECT_EQ(range.clampFrame(3), 10);
    EXPECT_EQ(range.clampFrame(27), 20);
}

TEST(FrameRangeTest, JsonAndStringRoundTripsPreserveSignedEndpoints)
{
    const FrameRange original(-12, 345);

    const FrameRange fromJson = FrameRange::fromJson(original.toJson());
    EXPECT_EQ(fromJson, original);
    EXPECT_EQ(fromJson.start(), -12);
    EXPECT_EQ(fromJson.end(), 345);

    const FrameRange fromString = FrameRange::fromString(original.toString());
    EXPECT_EQ(fromString, original);
    EXPECT_EQ(FrameRange::fromString(QStringLiteral("  -12, 345  ")), original);
}

TEST(FrameRangeTest, StringParserRejectsMalformedEndpoints)
{
    EXPECT_FALSE(FrameRange::fromString(QStringLiteral("not a range")).isValid());
    EXPECT_FALSE(FrameRange::fromString(QStringLiteral("[1, two]")).isValid());
}

TEST(FrameRangeTest, NormalizationOrdersEndpointsWithoutChangingNonMutatingSource)
{
    const FrameRange reversed(9, 3);
    EXPECT_FALSE(reversed.isValid());
    EXPECT_EQ(reversed.normalized(), FrameRange(3, 9));
    EXPECT_EQ(reversed, FrameRange(9, 3));

    FrameRange inPlace(9, 3);
    inPlace.normalize();
    EXPECT_EQ(inPlace, FrameRange(3, 9));
}

TEST(FrameRangeTest, FrameEnumerationAndUniformSamplesUseInclusiveRange)
{
    const FrameRange range(-2, 4);
    const std::vector<std::int64_t> expectedFrames{-2, -1, 0, 1, 2, 3, 4};
    EXPECT_EQ(range.frameCount(), 7);
    EXPECT_EQ(range.frames(), expectedFrames);
    EXPECT_EQ(range.uniformSample(4),
              (std::vector<std::int64_t>{-2, -1, 1, 2}));
    EXPECT_TRUE(range.uniformSample(0).empty());
}

TEST(FrameRangeTest, EnumeratesSmallRangesNearSignedFrameLimits)
{
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const FrameRange nearMinimum(minimum, minimum + 1);
    const FrameRange nearMaximum(maximum - 2, maximum - 1);

    EXPECT_EQ(nearMinimum.frames(),
              (std::vector<std::int64_t>{minimum, minimum + 1}));
    EXPECT_EQ(nearMaximum.frames(),
              (std::vector<std::int64_t>{maximum - 2, maximum - 1}));
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

TEST(RationalTimeTest, NonpositiveScalesUseSafeFallbacks)
{
    EXPECT_EQ(RationalTime(7, 0).scale(), 1);
    EXPECT_EQ(RationalTime(7, -3).scale(), 1);
    EXPECT_EQ(RationalTime::fromFrameCount(24, 0), RationalTime(24, 1));
    EXPECT_EQ(RationalTime(3, 2).rescaledTo(0), 0);
    EXPECT_EQ(RationalTime(3, 2).toFrameCount(-24), 0);
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

TEST(RationalTimeTest, ComparesEquivalentAndCrossScaleSignedFractions)
{
    const RationalTime half(1, 2);
    const RationalTime equivalentHalf(2, 4);
    const RationalTime twoThirds(2, 3);
    const RationalTime negativeThreeQuarters(-3, 4);
    const RationalTime negativeTwoThirds(-2, 3);

    EXPECT_EQ(half, equivalentHalf);
    EXPECT_LE(half, equivalentHalf);
    EXPECT_GE(half, equivalentHalf);
    EXPECT_LT(half, twoThirds);
    EXPECT_GT(twoThirds, half);
    EXPECT_LT(negativeThreeQuarters, negativeTwoThirds);
    EXPECT_GT(negativeTwoThirds, negativeThreeQuarters);
    EXPECT_LT(negativeThreeQuarters, half);

    EXPECT_EQ(twoThirds - half, RationalTime(1, 6));
    EXPECT_EQ(half - twoThirds, RationalTime(-1, 6));
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

TEST(FrameOffsetTest, DivisionByZeroReturnsZeroAndCompoundDivisionPreservesValue)
{
    const FrameOffset offset(12);
    EXPECT_EQ((offset / 0).value(), 0);

    FrameOffset adjusted(12);
    adjusted /= 0;
    EXPECT_EQ(adjusted.value(), 12);
}
