#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

import Frame.Rate;
import Serialization.Registry;
import Serialization.SchemaMigration;
import Utils.String.UniString;

using ArtifactCore::FrameRate;

TEST(FrameRateContractTest, ParsesAndPreservesExactRationalRates)
{
    const FrameRate rate(QStringLiteral("30000/1001 fps"));

    EXPECT_TRUE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 30000);
    EXPECT_EQ(rate.denominator(), 1001);
    EXPECT_NEAR(rate.exactFps(), 30000.0 / 1001.0, 1e-12);
}

TEST(FrameRateContractTest, RecognizesDropFrameOnlyForSupportedExactRates)
{
    EXPECT_TRUE(FrameRate::fromRational(30000, 1001).hasDropframe());
    EXPECT_TRUE(FrameRate::fromRational(60000, 1001).hasDropframe());
    EXPECT_FALSE(FrameRate::fromRational(25, 1).hasDropframe());
    EXPECT_FALSE(FrameRate::fromRational(30000, 1000).hasDropframe());
}

TEST(FrameRateContractTest, RecognizesEverySupportedExactDropFrameRate)
{
    for (const auto [numerator, denominator] : {
             std::pair<std::int64_t, std::int64_t>{24000, 1001},
             {30000, 1001}, {48000, 1001}, {60000, 1001}}) {
        EXPECT_TRUE(FrameRate::fromRational(numerator, denominator).hasDropframe());
    }
}

TEST(FrameRateContractTest, FloatDropFrameRecognitionUsesExpectedProximity)
{
    EXPECT_TRUE(FrameRate(23.976f).hasDropframe());
    EXPECT_TRUE(FrameRate(29.96f).hasDropframe());
    EXPECT_TRUE(FrameRate(47.95f).hasDropframe());
    EXPECT_FALSE(FrameRate(29.94f).hasDropframe());
    EXPECT_FALSE(FrameRate(59.90f).hasDropframe());
}

TEST(FrameRateContractTest, RejectsInvalidRationalUpdatesWithoutChangingCurrentRate)
{
    auto rate = FrameRate::fromRational(24000, 1001);
    rate.setRationalRate(0, 1);
    rate.setRationalRate(1, 0);
    rate.setRationalRate(-1, 2);

    EXPECT_EQ(rate.numerator(), 24000);
    EXPECT_EQ(rate.denominator(), 1001);
    EXPECT_TRUE(rate.hasExactRational());
}

TEST(FrameRateContractTest, FloatAssignmentClearsExactRationalMetadata)
{
    auto rate = FrameRate::fromRational(30000, 1001);
    rate = 25.0f;

    EXPECT_FALSE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 1);
    EXPECT_EQ(rate.denominator(), 1);
    EXPECT_FLOAT_EQ(rate.framerate(), 25.0f);
}

TEST(FrameRateContractTest, InvalidFloatRatesFallBackToThirty)
{
    for (const float value : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
        const FrameRate rate(value);
        EXPECT_FLOAT_EQ(rate.framerate(), 30.0f);
    }
}

TEST(FrameRateContractTest, SpeedChangesClampAtThirtyAndIgnoreNonFiniteDeltas)
{
    FrameRate rate(30.0f);
    rate.speedDown(100.0f);
    EXPECT_FLOAT_EQ(rate.framerate(), 30.0f);
    rate.speedUp(5.0f);
    EXPECT_FLOAT_EQ(rate.framerate(), 35.0f);
    rate.speedDown(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(rate.framerate(), 35.0f);
}

TEST(FrameRateContractTest, SpeedAdjustmentsApplyPositiveFiniteDeltasAndIgnoreNegativeOnes)
{
    FrameRate rate(30.0f);
    rate.speedUp(-2.0f);
    EXPECT_FLOAT_EQ(rate.framerate(), 30.0f);

    rate.speedDown(6.5f);
    EXPECT_FLOAT_EQ(rate.framerate(), 23.5f);
    rate.speedDown(-3.0f);
    EXPECT_FLOAT_EQ(rate.framerate(), 23.5f);
    rate.speedUp(1.25f);
    EXPECT_FLOAT_EQ(rate.framerate(), 24.75f);
}

TEST(FrameRateContractTest, SpeedAdjustmentConvertsExactRateToFloatMetadata)
{
    auto rate = FrameRate::fromRational(30000, 1001);

    rate.speedUp(1.0f);

    EXPECT_FALSE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 1);
    EXPECT_EQ(rate.denominator(), 1);
    EXPECT_NEAR(rate.framerate(), static_cast<float>(30000.0 / 1001.0) + 1.0f, 1e-5f);
}

TEST(FrameRateContractTest, ParsesDecimalAndRationalStrings)
{
    const FrameRate decimal(QStringLiteral("  23.976 fps  "));
    const FrameRate rational(QStringLiteral("24000 / 1001 DF"));

    EXPECT_NEAR(decimal.framerate(), 23.976f, 1e-5f);
    EXPECT_TRUE(rational.hasExactRational());
    EXPECT_EQ(rational.numerator(), 24000);
    EXPECT_EQ(rational.denominator(), 1001);
}

TEST(FrameRateContractTest, InvalidStringAssignmentLeavesExistingValue)
{
    FrameRate rate(48.0f);
    rate.setFromString(QStringLiteral("not a rate"));

    EXPECT_FLOAT_EQ(rate.framerate(), 48.0f);
}

TEST(FrameRateContractTest, InvalidRationalStringsPreserveExistingExactRate)
{
    auto rate = FrameRate::fromRational(30000, 1001);
    const QString invalidRates[] = {QStringLiteral("24000/0"),
                                    QStringLiteral("-24/1"),
                                    QStringLiteral("24/-1")};

    for (const QString& invalid : invalidRates) {
        rate.setFromString(invalid);
        EXPECT_TRUE(rate.hasExactRational()) << invalid;
        EXPECT_EQ(rate.numerator(), 30000) << invalid;
        EXPECT_EQ(rate.denominator(), 1001) << invalid;
        EXPECT_NEAR(rate.exactFps(), 30000.0 / 1001.0, 1e-12) << invalid;
    }
}

TEST(FrameRateContractTest, CopyAndCopyAssignmentPreserveExactRate)
{
    const auto source = FrameRate::fromRational(24000, 1001);
    const FrameRate copied(source);
    FrameRate assigned(60.0f);
    assigned = source;

    for (const auto* rate : {&copied, &assigned}) {
        EXPECT_TRUE(rate->hasExactRational());
        EXPECT_EQ(rate->numerator(), 24000);
        EXPECT_EQ(rate->denominator(), 1001);
    }
}

TEST(FrameRateContractTest, MoveConstructionAndAssignmentTransferOwnership)
{
    FrameRate source = FrameRate::fromRational(30000, 1001);
    FrameRate moved(std::move(source));
    FrameRate destination(24.0f);
    destination = std::move(moved);

    EXPECT_TRUE(destination.hasExactRational());
    EXPECT_EQ(destination.numerator(), 30000);
    EXPECT_EQ(destination.denominator(), 1001);
}

TEST(FrameRateContractTest, JsonRoundTripPreservesExactAndFloatRates)
{
    const auto exact = FrameRate::fromRational(30000, 1001);
    const auto exactRestored = FrameRate::fromJsonStatic(exact.toJson());
    const FrameRate plain(25.0f);
    const auto plainRestored = FrameRate::fromJsonStatic(plain.toJson());

    EXPECT_TRUE(exactRestored.hasExactRational());
    EXPECT_EQ(exactRestored.numerator(), 30000);
    EXPECT_EQ(exactRestored.denominator(), 1001);
    EXPECT_FALSE(plainRestored.hasExactRational());
    EXPECT_FLOAT_EQ(plainRestored.framerate(), 25.0f);
}

TEST(FrameRateContractTest, JsonAcceptsLegacyFpsAndValueFields)
{
    QJsonObject legacyFps{{QStringLiteral("fps"), 48.0}};
    QJsonObject legacyValue{{QStringLiteral("value"), QStringLiteral("24000/1001")}};

    const auto fpsRate = FrameRate::fromJsonStatic(legacyFps);
    const auto valueRate = FrameRate::fromJsonStatic(legacyValue);
    EXPECT_FLOAT_EQ(fpsRate.framerate(), 48.0f);
    EXPECT_TRUE(valueRate.hasExactRational());
    EXPECT_EQ(valueRate.numerator(), 24000);
}

TEST(FrameRateContractTest, JsonPrefersCompleteRationalFields)
{
    QJsonObject data{{QStringLiteral("frameRate"), 25.0},
                     {QStringLiteral("numerator"), 24000.0},
                     {QStringLiteral("denominator"), 1001.0}};
    const auto rate = FrameRate::fromJsonStatic(data);

    EXPECT_EQ(rate.numerator(), 24000);
    EXPECT_EQ(rate.denominator(), 1001);
    EXPECT_TRUE(rate.hasExactRational());
}

TEST(FrameRateContractTest, JsonWriterEmitsDropFrameAndExactFields)
{
    const auto rate = FrameRate::fromRational(30000, 1001);
    const auto json = rate.toJson();

    EXPECT_TRUE(json.value(QStringLiteral("dropFrame")).toBool());
    EXPECT_EQ(json.value(QStringLiteral("numerator")).toInt(), 30000);
    EXPECT_EQ(json.value(QStringLiteral("denominator")).toInt(), 1001);
}

TEST(FrameRateContractTest, UpdatesJsonDestinationWithoutDiscardingUnrelatedKeys)
{
    const FrameRate rate(24.0f);
    QJsonObject json{{QStringLiteral("custom"), QStringLiteral("keep")}};
    rate.writeToJson(json);

    EXPECT_EQ(json.value(QStringLiteral("custom")).toString(), QStringLiteral("keep"));
    EXPECT_FLOAT_EQ(static_cast<float>(json.value(QStringLiteral("frameRate")).toDouble()), 24.0f);
    EXPECT_FALSE(json.value(QStringLiteral("dropFrame")).toBool());
}

TEST(FrameRateContractTest, RegistersJsonAdapterAndSchemaMigration)
{
    EXPECT_TRUE(ArtifactCore::Serialization::SerializationRegistry::instance()
                    .isRegistered(QStringLiteral("FrameRate")));
    EXPECT_TRUE(ArtifactCore::Serialization::SchemaMigrationRegistry::instance()
                    .hasMigrationPath(QStringLiteral("FrameRate"), 0, 1));
}

TEST(FrameRateContractTest, SwapExchangesRatesAndOffsetsRemainValueTypes)
{
    auto left = FrameRate::fromRational(24000, 1001);
    FrameRate right(60.0f);
    left.swap(right);

    EXPECT_FLOAT_EQ(left.framerate(), 60.0f);
    EXPECT_TRUE(right.hasExactRational());
    EXPECT_EQ(right.numerator(), 24000);
    const ArtifactCore::FrameRateOffset low(10), high(11);
    EXPECT_LE(low, high);
    EXPECT_NE(low, high);
}

TEST(FrameRateContractTest, FormatsPlainAndDropFrameDisplayStrings)
{
    const FrameRate plain(24.0f);
    const auto ntsc = FrameRate::fromRational(30000, 1001);

    EXPECT_EQ(plain.toString().toQString(), QStringLiteral("24.000000"));
    EXPECT_EQ(plain.toDisplayString().toQString(), QStringLiteral("24.000 fps"));
    EXPECT_EQ(ntsc.toDisplayString().toQString(), QStringLiteral("29.97 DF"));
    EXPECT_EQ(ntsc.toDisplayString(false).toQString(), QStringLiteral("29.970 fps"));
}

TEST(FrameRateContractTest, EqualityUsesStoredFloatRateValue)
{
    const auto exact = FrameRate::fromRational(30000, 1001);
    const FrameRate sameFloat(exact.framerate());
    const FrameRate different(30.0f);

    EXPECT_EQ(exact, sameFloat);
    EXPECT_NE(exact, different);
}

TEST(FrameRateContractTest, JsonFrameRateFieldWinsOverLegacyAliases)
{
    QJsonObject data{{QStringLiteral("frameRate"), 24.0},
                     {QStringLiteral("fps"), 60.0},
                     {QStringLiteral("value"), QStringLiteral("48")}};
    const auto rate = FrameRate::fromJsonStatic(data);

    EXPECT_FLOAT_EQ(rate.framerate(), 24.0f);
    EXPECT_FALSE(rate.hasExactRational());
}

TEST(FrameRateContractTest, IncompleteRationalJsonFallsBackToLegacyFps)
{
    QJsonObject data{{QStringLiteral("numerator"), 24000.0},
                     {QStringLiteral("fps"), 48.0}};
    const auto rate = FrameRate::fromJsonStatic(data);

    EXPECT_FLOAT_EQ(rate.framerate(), 48.0f);
    EXPECT_FALSE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 1);
    EXPECT_EQ(rate.denominator(), 1);
}

TEST(FrameRateContractTest, InvalidCompleteRationalJsonLeavesDefaultRate)
{
    QJsonObject zeroDenominator{{QStringLiteral("numerator"), 24000.0},
                                {QStringLiteral("denominator"), 0.0},
                                {QStringLiteral("frameRate"), 60.0}};
    const auto rate = FrameRate::fromJsonStatic(zeroDenominator);

    EXPECT_FLOAT_EQ(rate.framerate(), 30.0f);
    EXPECT_FALSE(rate.hasExactRational());
}

TEST(FrameRateContractTest, InvalidCompleteRationalJsonPreservesExistingRate)
{
    auto rate = FrameRate::fromRational(24000, 1001);
    const QJsonObject invalid{{QStringLiteral("numerator"), 0.0},
                              {QStringLiteral("denominator"), 1001.0},
                              {QStringLiteral("frameRate"), 60.0}};

    rate.setFromJson(invalid);

    EXPECT_TRUE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 24000);
    EXPECT_EQ(rate.denominator(), 1001);
    EXPECT_NEAR(rate.exactFps(), 24000.0 / 1001.0, 1e-12);
}

TEST(FrameRateContractTest, FloatJsonUpdateClearsPreviouslyExactRateMetadata)
{
    auto rate = FrameRate::fromRational(30000, 1001);
    const QJsonObject plainRate{{QStringLiteral("frameRate"), 25.0}};

    rate.setFromJson(plainRate);

    EXPECT_FLOAT_EQ(rate.framerate(), 25.0f);
    EXPECT_FALSE(rate.hasExactRational());
    EXPECT_EQ(rate.numerator(), 1);
    EXPECT_EQ(rate.denominator(), 1);
}

TEST(FrameRateContractTest, OffsetMutationAndAllComparisonsUseFrameCounts)
{
    ArtifactCore::FrameRateOffset offset(-4);
    offset.setValue(6);
    const ArtifactCore::FrameRateOffset same(6);
    const ArtifactCore::FrameRateOffset later(7);

    EXPECT_EQ(offset.value(), 6);
    EXPECT_EQ(offset, same);
    EXPECT_LE(offset, same);
    EXPECT_LE(same, offset);
    EXPECT_FALSE(later <= offset);
    EXPECT_NE(offset, later);
}

TEST(FrameRateContractTest, StorageScaleRoundsCommonFractionalRatesToNearestInteger)
{
    EXPECT_EQ(FrameRate::storageScaleForFps(24.0), 24);
    EXPECT_EQ(FrameRate::storageScaleForFps(24000.0 / 1001.0), 24);
    EXPECT_EQ(FrameRate::storageScaleForFps(30000.0 / 1001.0), 30);
    EXPECT_EQ(FrameRate::storageScaleForFps(60000.0 / 1001.0), 60);
}

TEST(FrameRateContractTest, StorageScaleRoundsPositiveHalfValuesAwayFromZero)
{
    EXPECT_EQ(FrameRate::storageScaleForFps(0.1), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(0.49), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(0.5), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(1.49), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(1.5), 2);
}

TEST(FrameRateContractTest, StorageScaleUsesPositiveFallbackForInvalidFps)
{
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(FrameRate::storageScaleForFps(0.0, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(-24.0, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(infinity, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(nan, 48), 48);
    EXPECT_EQ(FrameRate::storageScaleForFps(nan, 0), 1);
    EXPECT_EQ(FrameRate::storageScaleForFps(infinity, -5), 1);
}

TEST(FrameRateContractTest, StorageScaleSaturatesFiniteValuesBeyondInt64Range)
{
    EXPECT_EQ(FrameRate::storageScaleForFps(std::numeric_limits<double>::max()),
              std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(FrameRate::storageScaleForFps(std::numeric_limits<double>::max(), 48),
              std::numeric_limits<std::int64_t>::max());
}
