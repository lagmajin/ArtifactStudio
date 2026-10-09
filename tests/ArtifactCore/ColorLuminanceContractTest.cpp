#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>

import Color.Luminance;

using namespace ArtifactCore;

TEST(ColorLuminanceContractTest, Rec709UsesPublishedWeights)
{
    const float value = ColorLuminance::calculate(0.2f, 0.4f, 0.8f);
    EXPECT_NEAR(value, 0.2126f * 0.2f + 0.7152f * 0.4f + 0.0722f * 0.8f, 1e-7f);
}

TEST(ColorLuminanceContractTest, StandardsProduceTheirOwnWeightedLuminance)
{
    struct Case { LuminanceStandard standard; float red; float green; float blue; };
    constexpr Case cases[] = {
        {LuminanceStandard::Rec601, 0.299f, 0.587f, 0.114f},
        {LuminanceStandard::Rec709, 0.2126f, 0.7152f, 0.0722f},
        {LuminanceStandard::Rec2020, 0.2627f, 0.6780f, 0.0593f},
        {LuminanceStandard::DisplayP3, 0.2289746f, 0.6917385f, 0.0792869f},
        {LuminanceStandard::ACESAP1, 0.2722287f, 0.6740818f, 0.0536895f}
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase.standard));
        const float expected = testCase.red * 0.2f + testCase.green * 0.4f + testCase.blue * 0.8f;
        EXPECT_NEAR(ColorLuminance::calculate(0.2f, 0.4f, 0.8f, testCase.standard), expected, 1e-7f);
    }
}

TEST(ColorLuminanceContractTest, SignedAndHdrInputsRetainWeightedLuminance)
{
    struct Case {
        LuminanceStandard standard;
        float redWeight;
        float greenWeight;
        float blueWeight;
    };
    constexpr Case cases[] = {
        {LuminanceStandard::Rec601, 0.299f, 0.587f, 0.114f},
        {LuminanceStandard::Rec709, 0.2126f, 0.7152f, 0.0722f},
        {LuminanceStandard::Rec2020, 0.2627f, 0.6780f, 0.0593f},
        {LuminanceStandard::DisplayP3, 0.2289746f, 0.6917385f, 0.0792869f},
        {LuminanceStandard::ACESAP1, 0.2722287f, 0.6740818f, 0.0536895f},
    };
    constexpr float samples[][3] = {
        {-0.25f, 0.5f, 1.25f},
        {1.5f, -0.2f, 0.7f},
        {0.1f, 1.8f, -0.4f},
    };

    for (const auto& testCase : cases) {
        for (const auto& sample : samples) {
            const float expected = testCase.redWeight * sample[0] +
                testCase.greenWeight * sample[1] +
                testCase.blueWeight * sample[2];
            SCOPED_TRACE(::testing::Message()
                << "standard=" << static_cast<int>(testCase.standard)
                << " rgb=(" << sample[0] << ", " << sample[1] << ", "
                << sample[2] << ")");

            const float luminance = ColorLuminance::calculate(
                sample[0], sample[1], sample[2], testCase.standard);
            EXPECT_NEAR(luminance, expected, 2e-7f);
            const auto gray = ColorLuminance::toGrayscale(
                sample[0], sample[1], sample[2], testCase.standard);
            for (const float channel : gray) {
                EXPECT_FLOAT_EQ(channel, luminance);
            }
        }
    }
}

TEST(ColorLuminanceContractTest, EveryStandardMapsBlackAndWhiteToTheirEndpoints)
{
    constexpr LuminanceStandard standards[] = {
        LuminanceStandard::Rec601,
        LuminanceStandard::Rec709,
        LuminanceStandard::Rec2020,
        LuminanceStandard::DisplayP3,
        LuminanceStandard::ACESAP1,
    };

    for (const auto standard : standards) {
        SCOPED_TRACE(static_cast<int>(standard));
        EXPECT_FLOAT_EQ(ColorLuminance::calculate(0.0f, 0.0f, 0.0f, standard), 0.0f);
        EXPECT_NEAR(ColorLuminance::calculate(1.0f, 1.0f, 1.0f, standard), 1.0f, 2e-6f);
    }
}

TEST(ColorLuminanceContractTest, UnknownStandardFallsBackToRec709Weights)
{
    constexpr auto unknown = static_cast<LuminanceStandard>(255);
    const float expected = 0.2126f * 0.2f + 0.7152f * 0.4f + 0.0722f * 0.8f;

    EXPECT_FLOAT_EQ(ColorLuminance::calculate(
        0.2f, 0.4f, 0.8f, unknown), expected);
    const auto gray = ColorLuminance::toGrayscale(
        0.2f, 0.4f, 0.8f, unknown);
    EXPECT_FLOAT_EQ(gray[0], expected);
    EXPECT_FLOAT_EQ(gray[1], expected);
    EXPECT_FLOAT_EQ(gray[2], expected);

    const auto inspected = ColorLuminance::inspectBroadcastSafe(
        0.2f, 0.4f, 0.8f, unknown, 0.3f, 0.7f, 0.1f, 0.6f);
    EXPECT_FLOAT_EQ(inspected.luminance, expected);
    EXPECT_FALSE(inspected.luminanceViolation);
    EXPECT_TRUE(inspected.gamutViolation);
    EXPECT_TRUE(inspected.hasViolation());
}

TEST(ColorLuminanceContractTest, PrimaryChannelContributionsMatchTheSelectedWeights)
{
    constexpr struct Case { LuminanceStandard standard; float red; float green; float blue; } cases[] = {
        {LuminanceStandard::Rec601, 0.299f, 0.587f, 0.114f},
        {LuminanceStandard::Rec709, 0.2126f, 0.7152f, 0.0722f},
        {LuminanceStandard::Rec2020, 0.2627f, 0.6780f, 0.0593f},
        {LuminanceStandard::DisplayP3, 0.2289746f, 0.6917385f, 0.0792869f},
        {LuminanceStandard::ACESAP1, 0.2722287f, 0.6740818f, 0.0536895f},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase.standard));
        EXPECT_FLOAT_EQ(ColorLuminance::calculate(1.0f, 0.0f, 0.0f, testCase.standard), testCase.red);
        EXPECT_FLOAT_EQ(ColorLuminance::calculate(0.0f, 1.0f, 0.0f, testCase.standard), testCase.green);
        EXPECT_FLOAT_EQ(ColorLuminance::calculate(0.0f, 0.0f, 1.0f, testCase.standard), testCase.blue);
    }
}

TEST(ColorLuminanceContractTest, WeightedLuminanceIsMonotonicInEachChannel)
{
    constexpr LuminanceStandard standards[] = {
        LuminanceStandard::Rec601,
        LuminanceStandard::Rec709,
        LuminanceStandard::Rec2020,
        LuminanceStandard::DisplayP3,
        LuminanceStandard::ACESAP1,
    };
    constexpr float values[] = {0.0f, 0.1f, 0.25f, 0.5f, 0.9f, 1.0f};

    for (const auto standard : standards) {
        for (std::size_t index = 1; index < std::size(values); ++index) {
            SCOPED_TRACE(static_cast<int>(standard));
            EXPECT_LE(ColorLuminance::calculate(values[index - 1], 0.4f, 0.3f, standard),
                      ColorLuminance::calculate(values[index], 0.4f, 0.3f, standard));
            EXPECT_LE(ColorLuminance::calculate(0.4f, values[index - 1], 0.3f, standard),
                      ColorLuminance::calculate(0.4f, values[index], 0.3f, standard));
            EXPECT_LE(ColorLuminance::calculate(0.4f, 0.3f, values[index - 1], standard),
                      ColorLuminance::calculate(0.4f, 0.3f, values[index], standard));
        }
    }
}

TEST(ColorLuminanceContractTest, PerceptualBrightnessUsesSquaredChannelWeights)
{
    const float expected = std::sqrt(0.299f * 0.2f * 0.2f + 0.587f * 0.4f * 0.4f + 0.114f * 0.8f * 0.8f);
    EXPECT_NEAR(ColorLuminance::calculatePerceptual(0.2f, 0.4f, 0.8f), expected, 1e-7f);
}

TEST(ColorLuminanceContractTest, PerceptualBrightnessIsSignInvariantAndHomogeneous)
{
    constexpr float red = -0.2f;
    constexpr float green = 0.4f;
    constexpr float blue = -1.6f;
    constexpr float scale = 2.5f;

    const float value = ColorLuminance::calculatePerceptual(red, green, blue);
    const float signFlipped = ColorLuminance::calculatePerceptual(-red, -green, -blue);
    const float scaled = ColorLuminance::calculatePerceptual(
        red * scale, green * scale, blue * scale);
    const float expected = std::sqrt(
        0.299f * red * red + 0.587f * green * green + 0.114f * blue * blue);

    EXPECT_TRUE(std::isfinite(value));
    EXPECT_NEAR(value, expected, 1e-7f);
    EXPECT_NEAR(signFlipped, value, 1e-7f);
    EXPECT_NEAR(scaled, scale * value, 1e-6f);
}

TEST(ColorLuminanceContractTest, GrayscaleRepeatsSelectedStandardLuminance)
{
    const auto gray = ColorLuminance::toGrayscale(0.2f, 0.4f, 0.8f, LuminanceStandard::Rec2020);
    const float expected = ColorLuminance::calculate(0.2f, 0.4f, 0.8f, LuminanceStandard::Rec2020);
    EXPECT_FLOAT_EQ(gray[0], expected);
    EXPECT_FLOAT_EQ(gray[1], expected);
    EXPECT_FLOAT_EQ(gray[2], expected);
}

TEST(ColorLuminanceContractTest, BroadcastInspectionReportsLumaAndChannelViolationsSeparately)
{
    const auto safe = ColorLuminance::inspectBroadcastSafe(0.3f, 0.4f, 0.5f);
    EXPECT_FALSE(safe.luminanceViolation);
    EXPECT_FALSE(safe.gamutViolation);
    EXPECT_FALSE(safe.hasViolation());

    const auto channelOnly = ColorLuminance::inspectBroadcastSafe(
        0.0f, 0.5f, 0.5f, LuminanceStandard::Rec709, -1.0f, 2.0f, 0.1f, 0.9f);
    EXPECT_FALSE(channelOnly.luminanceViolation);
    EXPECT_TRUE(channelOnly.gamutViolation);
    EXPECT_TRUE(channelOnly.hasViolation());

    const auto lumaOnly = ColorLuminance::inspectBroadcastSafe(
        0.5f, 0.5f, 0.5f, LuminanceStandard::Rec709, 0.6f, 0.9f, 0.0f, 1.0f);
    EXPECT_TRUE(lumaOnly.luminanceViolation);
    EXPECT_FALSE(lumaOnly.gamutViolation);
}

TEST(ColorLuminanceContractTest, BroadcastInspectionReportsBothViolationsAndWeightedLuminance)
{
    const auto belowLegalRange = ColorLuminance::inspectBroadcastSafe(
        0.0f, 0.2f, 0.2f, LuminanceStandard::Rec709,
        0.25f, 0.75f, 0.1f, 0.9f);
    const float expectedLow = 0.2126f * 0.0f + 0.7152f * 0.2f + 0.0722f * 0.2f;
    EXPECT_NEAR(belowLegalRange.luminance, expectedLow, 1e-7f);
    EXPECT_TRUE(belowLegalRange.luminanceViolation);
    EXPECT_TRUE(belowLegalRange.gamutViolation);
    EXPECT_TRUE(belowLegalRange.hasViolation());

    const auto aboveLegalRange = ColorLuminance::inspectBroadcastSafe(
        1.2f, 0.9f, 0.9f, LuminanceStandard::Rec709,
        0.25f, 0.75f, 0.1f, 0.9f);
    const float expectedHigh = 0.2126f * 1.2f + 0.7152f * 0.9f + 0.0722f * 0.9f;
    EXPECT_NEAR(aboveLegalRange.luminance, expectedHigh, 1e-7f);
    EXPECT_TRUE(aboveLegalRange.luminanceViolation);
    EXPECT_TRUE(aboveLegalRange.gamutViolation);
    EXPECT_TRUE(aboveLegalRange.hasViolation());
}

TEST(ColorLuminanceContractTest, BroadcastClampBoundsChannelsAndMapsNonFiniteValuesToMinimum)
{
    const auto safe = ColorLuminance::clampBroadcastSafe(
        -0.2f, std::numeric_limits<float>::quiet_NaN(), 1.4f, 0.1f, 0.9f);
    EXPECT_FLOAT_EQ(safe[0], 0.1f);
    EXPECT_FLOAT_EQ(safe[1], 0.1f);
    EXPECT_FLOAT_EQ(safe[2], 0.9f);
}

TEST(ColorLuminanceContractTest, BroadcastInspectionIncludesLegalRangeEndpoints)
{
    const auto blackEndpoint = ColorLuminance::inspectBroadcastSafe(
        0.25f, 0.25f, 0.25f, LuminanceStandard::Rec709,
        0.25f, 0.75f, 0.25f, 0.75f);
    const auto whiteEndpoint = ColorLuminance::inspectBroadcastSafe(
        0.75f, 0.75f, 0.75f, LuminanceStandard::Rec709,
        0.25f, 0.75f, 0.25f, 0.75f);

    EXPECT_FALSE(blackEndpoint.hasViolation());
    EXPECT_FALSE(whiteEndpoint.hasViolation());

    for (int mask = 0; mask < 8; ++mask) {
        const float red = (mask & 1) != 0 ? 0.75f : 0.25f;
        const float green = (mask & 2) != 0 ? 0.75f : 0.25f;
        const float blue = (mask & 4) != 0 ? 0.75f : 0.25f;
        const auto mixedEndpoints = ColorLuminance::inspectBroadcastSafe(
            red, green, blue, LuminanceStandard::Rec709,
            0.25f, 0.75f, 0.25f, 0.75f);
        SCOPED_TRACE(::testing::Message()
            << "mask=" << mask << " rgb=(" << red << ", " << green
            << ", " << blue << ")");
        EXPECT_FALSE(mixedEndpoints.luminanceViolation);
        EXPECT_FALSE(mixedEndpoints.gamutViolation);
        EXPECT_FALSE(mixedEndpoints.hasViolation());
        EXPECT_GE(mixedEndpoints.luminance, 0.25f);
        EXPECT_LE(mixedEndpoints.luminance, 0.75f);
    }
}

TEST(ColorLuminanceContractTest, BroadcastInspectionMatchesIndependentRgbBoundaryGrid)
{
    struct WeightCase {
        LuminanceStandard standard;
        float red;
        float green;
        float blue;
    };
    constexpr WeightCase standards[] = {
        {LuminanceStandard::Rec601, 0.299f, 0.587f, 0.114f},
        {LuminanceStandard::Rec709, 0.2126f, 0.7152f, 0.0722f},
        {LuminanceStandard::Rec2020, 0.2627f, 0.6780f, 0.0593f},
        {LuminanceStandard::DisplayP3, 0.2289746f, 0.6917385f, 0.0792869f},
        {LuminanceStandard::ACESAP1, 0.2722287f, 0.6740818f, 0.0536895f},
    };
    constexpr float samples[] = {-0.1f, 0.1f, 0.5f, 0.9f, 1.1f};
    constexpr float legalBlack = 0.1f;
    constexpr float legalWhite = 0.9f;
    constexpr float channelMin = 0.1f;
    constexpr float channelMax = 0.9f;

    for (const WeightCase& testCase : standards) {
        for (const float red : samples) {
            for (const float green : samples) {
                for (const float blue : samples) {
                    const float expectedLuminance = testCase.red * red
                        + testCase.green * green + testCase.blue * blue;
                    const bool expectedLumaViolation = expectedLuminance < legalBlack
                        || expectedLuminance > legalWhite;
                    const bool expectedGamutViolation = red < channelMin
                        || red > channelMax || green < channelMin
                        || green > channelMax || blue < channelMin
                        || blue > channelMax;

                    const auto actual = ColorLuminance::inspectBroadcastSafe(
                        red, green, blue, testCase.standard, legalBlack, legalWhite,
                        channelMin, channelMax);
                    SCOPED_TRACE(::testing::Message()
                        << "standard=" << static_cast<int>(testCase.standard)
                        << " rgb=" << red << "," << green << "," << blue);
                    EXPECT_NEAR(actual.luminance, expectedLuminance, 2.0e-7f);
                    EXPECT_EQ(actual.luminanceViolation, expectedLumaViolation);
                    EXPECT_EQ(actual.gamutViolation, expectedGamutViolation);
                    EXPECT_EQ(actual.hasViolation(),
                              expectedLumaViolation || expectedGamutViolation);
                }
            }
        }
    }
}

TEST(ColorLuminanceContractTest, BroadcastInspectionFlagsNonFiniteChannelsAsBothViolations)
{
    const auto result = ColorLuminance::inspectBroadcastSafe(
        std::numeric_limits<float>::quiet_NaN(), 0.5f, 0.5f);

    EXPECT_TRUE(result.luminanceViolation);
    EXPECT_TRUE(result.gamutViolation);
    EXPECT_TRUE(result.hasViolation());
}

TEST(ColorLuminanceContractTest, BroadcastInspectionFlagsPositiveAndNegativeInfinity)
{
    const float infinity = std::numeric_limits<float>::infinity();
    const auto positive = ColorLuminance::inspectBroadcastSafe(
        infinity, 0.5f, 0.5f);
    const auto negative = ColorLuminance::inspectBroadcastSafe(
        0.5f, 0.5f, -infinity);

    EXPECT_TRUE(positive.luminanceViolation);
    EXPECT_TRUE(positive.gamutViolation);
    EXPECT_TRUE(positive.hasViolation());
    EXPECT_TRUE(negative.luminanceViolation);
    EXPECT_TRUE(negative.gamutViolation);
    EXPECT_TRUE(negative.hasViolation());
}

TEST(ColorLuminanceContractTest, NeutralInputsRemainNeutralAcrossLuminanceStandards)
{
    constexpr LuminanceStandard standards[] = {
        LuminanceStandard::Rec601,
        LuminanceStandard::Rec709,
        LuminanceStandard::Rec2020,
        LuminanceStandard::DisplayP3,
        LuminanceStandard::ACESAP1,
    };
    constexpr float values[] = {0.0f, 0.125f, 0.5f, 0.875f, 1.0f};

    for (const auto standard : standards) {
        for (const float value : values) {
            SCOPED_TRACE(::testing::Message()
                << "standard=" << static_cast<int>(standard) << " value=" << value);
            const auto gray = ColorLuminance::toGrayscale(value, value, value, standard);
            for (const float channel : gray) {
                EXPECT_NEAR(channel, value, 2.0e-6f);
            }
        }
    }

    for (const float value : values) {
        EXPECT_NEAR(ColorLuminance::calculatePerceptual(value, value, value),
                    value, 1.0e-7f);
    }
}

TEST(ColorLuminanceContractTest, BroadcastClampIsFiniteBoundedAndIdempotent)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    constexpr float values[] = {
        -2.0f, -0.1f, 0.0f, 0.1f, 0.4f, 0.9f, 1.0f, 2.0f,
    };
    constexpr float channelMin = 0.1f;
    constexpr float channelMax = 0.9f;

    for (const float value : values) {
        const auto clamped = ColorLuminance::clampBroadcastSafe(
            value, value, value, channelMin, channelMax);
        const auto clampedAgain = ColorLuminance::clampBroadcastSafe(
            clamped[0], clamped[1], clamped[2], channelMin, channelMax);
        for (std::size_t channel = 0; channel < clamped.size(); ++channel) {
            EXPECT_TRUE(std::isfinite(clamped[channel]));
            EXPECT_GE(clamped[channel], channelMin);
            EXPECT_LE(clamped[channel], channelMax);
            EXPECT_FLOAT_EQ(clampedAgain[channel], clamped[channel]);
        }
    }

    for (const float nonFinite : {nan, infinity, -infinity}) {
        const auto clamped = ColorLuminance::clampBroadcastSafe(
            nonFinite, nonFinite, nonFinite, channelMin, channelMax);
        EXPECT_FLOAT_EQ(clamped[0], channelMin);
        EXPECT_FLOAT_EQ(clamped[1], channelMin);
        EXPECT_FLOAT_EQ(clamped[2], channelMin);
    }
}
