#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <QString>

import Animation.EasingCurveUtil;

using namespace ArtifactCore;

namespace {

constexpr std::array kEasingTypes{
    EasingType::Hold, EasingType::Linear, EasingType::EaseIn,
    EasingType::EaseOut, EasingType::EaseInOut, EasingType::EaseOutIn,
    EasingType::Smooth, EasingType::BackIn, EasingType::BackOut,
    EasingType::BackInOut, EasingType::BounceIn, EasingType::BounceOut,
    EasingType::BounceInOut, EasingType::ElasticIn, EasingType::ElasticOut,
    EasingType::ElasticInOut, EasingType::Expo, EasingType::Quartic,
    EasingType::Quintic, EasingType::Sine, EasingType::Circular,
    EasingType::Bezier};

}

TEST(EasingContractTest, EveryCurveHasDefinedEndpoints)
{
    for (const auto type : kEasingTypes) {
        EXPECT_FLOAT_EQ(evaluateEasing(type, 0.0f), 0.0f)
            << easingTypeToString(type).toStdString();
        EXPECT_FLOAT_EQ(evaluateEasing(type, 1.0f), 1.0f)
            << easingTypeToString(type).toStdString();
    }
}

TEST(EasingContractTest, InputIsClampedBeforeCurveEvaluation)
{
    for (const auto type : kEasingTypes) {
        EXPECT_FLOAT_EQ(evaluateEasing(type, -2.0f), evaluateEasing(type, 0.0f));
        EXPECT_FLOAT_EQ(evaluateEasing(type, 3.0f), evaluateEasing(type, 1.0f));
        EXPECT_FLOAT_EQ(evaluateEasing(type, -std::numeric_limits<float>::infinity()), 0.0f);
        EXPECT_FLOAT_EQ(evaluateEasing(type, std::numeric_limits<float>::infinity()), 1.0f);
    }
}

TEST(EasingContractTest, NaNInputPropagatesThroughEveryCurve)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const auto type : kEasingTypes) {
        EXPECT_TRUE(std::isnan(evaluateEasing(type, nan)))
            << easingTypeToString(type).toStdString();
    }
}

TEST(EasingContractTest, HoldChangesOnlyAtTheTerminalEndpoint)
{
    EXPECT_FLOAT_EQ(evaluateEasing(EasingType::Hold, 0.999f), 0.0f);
    EXPECT_FLOAT_EQ(evaluateEasing(EasingType::Hold, 1.0f), 1.0f);
}

TEST(EasingContractTest, InOutCurvesAreComplementSymmetric)
{
    constexpr std::array types{EasingType::EaseInOut, EasingType::EaseOutIn,
                               EasingType::Smooth, EasingType::BackInOut,
                               EasingType::BounceInOut};
    for (const auto type : types) {
        for (int step = 0; step <= 100; ++step) {
            const float t = static_cast<float>(step) / 100.0f;
            EXPECT_NEAR(evaluateEasing(type, t) + evaluateEasing(type, 1.0f - t),
                        1.0f, 2e-5f)
                << easingTypeToString(type).toStdString() << " at " << t;
        }
    }
}

TEST(EasingContractTest, AllCurvesStayFiniteAcrossDenseNormalizedInputGrid)
{
    for (const auto type : kEasingTypes) {
        for (int step = 0; step <= 1000; ++step) {
            const float t = static_cast<float>(step) / 1000.0f;
            EXPECT_TRUE(std::isfinite(evaluateEasing(type, t)))
                << easingTypeToString(type).toStdString() << " at " << t;
        }
    }
}

TEST(EasingContractTest, NonOvershootCurvesStayBoundedAndMonotone)
{
    constexpr std::array monotoneTypes{
        EasingType::Linear, EasingType::EaseIn, EasingType::EaseOut,
        EasingType::EaseInOut, EasingType::EaseOutIn, EasingType::Smooth,
        EasingType::Expo, EasingType::Quartic, EasingType::Quintic,
        EasingType::Sine, EasingType::Circular, EasingType::Bezier};

    for (const auto type : monotoneTypes) {
        float previous = evaluateEasing(type, 0.0f);
        for (int step = 1; step <= 1000; ++step) {
            const float t = static_cast<float>(step) / 1000.0f;
            const float value = evaluateEasing(type, t);
            SCOPED_TRACE(::testing::Message()
                << easingTypeToString(type).toStdString() << " at " << t);
            EXPECT_GE(value, -1.0e-6f);
            EXPECT_LE(value, 1.0f + 1.0e-6f);
            EXPECT_GE(value + 1.0e-6f, previous);
            previous = value;
        }
    }
}

TEST(EasingContractTest, BackAndElasticCurvesOvershootTheirEndpoints)
{
    constexpr std::array overshootBelow{
        EasingType::BackIn, EasingType::BackInOut,
        EasingType::ElasticIn, EasingType::ElasticInOut};
    constexpr std::array overshootAbove{
        EasingType::BackOut, EasingType::BackInOut,
        EasingType::ElasticOut, EasingType::ElasticInOut};

    for (const auto type : overshootBelow) {
        float minimum = 0.0f;
        for (int step = 0; step <= 1000; ++step) {
            const float t = static_cast<float>(step) / 1000.0f;
            minimum = std::min(minimum, evaluateEasing(type, t));
        }
        EXPECT_LT(minimum, 0.0f) << easingTypeToString(type).toStdString();
    }

    for (const auto type : overshootAbove) {
        float maximum = 1.0f;
        for (int step = 0; step <= 1000; ++step) {
            const float t = static_cast<float>(step) / 1000.0f;
            maximum = std::max(maximum, evaluateEasing(type, t));
        }
        EXPECT_GT(maximum, 1.0f) << easingTypeToString(type).toStdString();
    }
}

TEST(EasingContractTest, BounceCurvesStayWithinTheNormalizedRange)
{
    constexpr std::array bounceTypes{
        EasingType::BounceIn, EasingType::BounceOut, EasingType::BounceInOut};
    for (const auto type : bounceTypes) {
        for (int step = 0; step <= 1000; ++step) {
            const float t = static_cast<float>(step) / 1000.0f;
            const float value = evaluateEasing(type, t);
            EXPECT_GE(value, 0.0f) << easingTypeToString(type).toStdString();
            EXPECT_LE(value, 1.0f) << easingTypeToString(type).toStdString();
        }
    }
}

TEST(EasingContractTest, BounceOutMatchesStandardSegmentLandmarks)
{
    EXPECT_NEAR(evaluateEasing(EasingType::BounceOut, 1.5f / 2.75f), 0.75f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::BounceOut, 2.25f / 2.75f), 0.9375f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::BounceOut, 2.625f / 2.75f), 0.984375f, 1e-6f);
    EXPECT_FLOAT_EQ(evaluateEasing(EasingType::BounceOut, 1.0f), 1.0f);
}

TEST(EasingContractTest, BounceInIsTheTimeReversalOfBounceOut)
{
    for (int step = 0; step <= 100; ++step) {
        const float t = static_cast<float>(step) / 100.0f;
        EXPECT_NEAR(evaluateEasing(EasingType::BounceIn, t),
                    1.0f - evaluateEasing(EasingType::BounceOut, 1.0f - t), 1e-6f)
            << "at " << t;
    }
}

TEST(EasingContractTest, StandardEaseCurvesMatchReferenceSamples)
{
    EXPECT_NEAR(evaluateEasing(EasingType::EaseIn, 0.25f), 0.0625f, 1e-7f);
    EXPECT_NEAR(evaluateEasing(EasingType::EaseOut, 0.25f), 0.4375f, 1e-7f);
    EXPECT_NEAR(evaluateEasing(EasingType::EaseInOut, 0.25f), 0.125f, 1e-7f);
    EXPECT_NEAR(evaluateEasing(EasingType::EaseOutIn, 0.25f), 0.375f, 1e-7f);
    EXPECT_NEAR(evaluateEasing(EasingType::Quartic, 0.5f), 0.9375f, 1e-7f);
    EXPECT_NEAR(evaluateEasing(EasingType::Quintic, 0.5f), 0.96875f, 1e-7f);
}

TEST(EasingContractTest, DefaultBezierPreviewIsComplementSymmetric)
{
    EXPECT_FLOAT_EQ(evaluateEasing(EasingType::Bezier, 0.5f), 0.5f);
    for (int step = 0; step <= 100; ++step) {
        const float t = static_cast<float>(step) / 100.0f;
        EXPECT_NEAR(evaluateEasing(EasingType::Bezier, t) +
                        evaluateEasing(EasingType::Bezier, 1.0f - t),
                    1.0f, 2e-5f)
            << "at " << t;
    }
}

TEST(EasingContractTest, UnknownEasingTypeFallsBackToClampedLinear)
{
    constexpr auto unknown = static_cast<EasingType>(999);
    EXPECT_FLOAT_EQ(evaluateEasing(unknown, 0.37f), 0.37f);
    EXPECT_FLOAT_EQ(evaluateEasing(unknown, -1.0f), 0.0f);
    EXPECT_FLOAT_EQ(evaluateEasing(unknown, 2.0f), 1.0f);
}

TEST(EasingContractTest, EasingNamesAreNonEmptyAndUnique)
{
    for (std::size_t i = 0; i < kEasingTypes.size(); ++i) {
        const auto name = easingTypeToString(kEasingTypes[i]);
        ASSERT_FALSE(name.trimmed().isEmpty());
        for (std::size_t j = i + 1; j < kEasingTypes.size(); ++j) {
            EXPECT_NE(name, easingTypeToString(kEasingTypes[j]));
        }
    }
    EXPECT_EQ(easingTypeToString(static_cast<EasingType>(999)), QStringLiteral("Linear"));
}

TEST(EasingContractTest, CandidateListCoversEachCurveExactlyOnce)
{
    const auto candidates = defaultEasingCandidates();
    ASSERT_EQ(candidates.size(), kEasingTypes.size());
    for (const auto type : kEasingTypes) {
        EXPECT_EQ(std::count_if(candidates.begin(), candidates.end(),
                                [type](const EasingCandidate& candidate) {
                                    return candidate.type == type;
                                }), 1);
    }
}

TEST(EasingContractTest, MappingPinsEveryCurveToItsStoredInterpolationOrdinal)
{
    constexpr std::array expected{0, 1, 3, 4, 5, 6, 2, 22, 23, 24, 16,
                                  17, 18, 19, 20, 21, 11, 9, 10, 13, 14, 25};
    for (std::size_t i = 0; i < kEasingTypes.size(); ++i) {
        EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(kEasingTypes[i])), expected[i])
            << easingTypeToString(kEasingTypes[i]).toStdString();
    }
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(static_cast<EasingType>(999))), 0);
}
