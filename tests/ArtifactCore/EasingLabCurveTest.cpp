#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include <QString>

import Animation.EasingCurveUtil;

using namespace ArtifactCore;

TEST(EasingLabCurveTest, NewCandidatesEvaluate) {
    EXPECT_NEAR(evaluateEasing(EasingType::Quartic, 0.5f), 0.9375f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::Quintic, 0.5f), 0.96875f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::Sine, 0.5f), 0.70710678f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::Circular, 0.5f), 0.8660254f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::Smooth, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(evaluateEasing(EasingType::EaseOutIn, 0.5f), 0.5f, 1e-6f);
    EXPECT_FLOAT_EQ(evaluateEasing(EasingType::Quartic, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(evaluateEasing(EasingType::Circular, 1.0f), 1.0f);
}

TEST(EasingLabCurveTest, BezierEasyEaseMidpoint) {
    // Guards the removed mt3 term in the Newton solver (P0=(0,0)).
    EXPECT_NEAR(evaluateEasing(EasingType::Bezier, 0.5f), 0.5f, 1e-3f);
    EXPECT_NEAR(evaluateEasing(EasingType::Bezier, 0.25f), 0.129162f, 1e-3f);
}

TEST(EasingLabCurveTest, MappingsMatchInterpolationEnum) {
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(EasingType::EaseOutIn)), 6);
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(EasingType::Smooth)), 2);
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(EasingType::Quartic)), 9);
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(EasingType::Quintic)), 10);
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(EasingType::Sine)), 13);
    EXPECT_EQ(static_cast<int>(easingTypeToInterpolation(EasingType::Circular)), 14);
}

TEST(EasingLabCurveTest, DefaultCandidatesIncludeNewTypes) {
    const auto candidates = defaultEasingCandidates();
    auto hasType = [&](EasingType type) {
        return std::any_of(candidates.begin(), candidates.end(),
                           [type](const EasingCandidate& c) { return c.type == type; });
    };
    EXPECT_TRUE(hasType(EasingType::EaseOutIn));
    EXPECT_TRUE(hasType(EasingType::Smooth));
    EXPECT_TRUE(hasType(EasingType::Quartic));
    EXPECT_TRUE(hasType(EasingType::Quintic));
    EXPECT_TRUE(hasType(EasingType::Sine));
    EXPECT_TRUE(hasType(EasingType::Circular));
    EXPECT_FALSE(easingTypeToString(EasingType::Quartic).isEmpty());
}
