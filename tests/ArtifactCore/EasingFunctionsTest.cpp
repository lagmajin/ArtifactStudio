#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

import Math.Interpolate;

using namespace ArtifactCore;

namespace {

template <typename Easer>
void expectEndpoints(const Easer& easer) {
    EXPECT_NEAR(easer(0.0f, 100.0f, 0.0f), 0.0f, 1e-4f);
    EXPECT_NEAR(easer(0.0f, 100.0f, 1.0f), 100.0f, 1e-4f);
}

template <typename Easer>
void expectMonotonicOnUnitInterval(const Easer& easer, float start, float end) {
    constexpr float kTolerance = 1e-5f;
    float previous = easer(start, end, 0.0f);
    const float lower = std::min(start, end);
    const float upper = std::max(start, end);

    for (int step = 1; step <= 100; ++step) {
        const float alpha = static_cast<float>(step) / 100.0f;
        const float current = easer(start, end, alpha);
        SCOPED_TRACE(step);
        EXPECT_TRUE(std::isfinite(current));
        EXPECT_GE(current, lower - kTolerance);
        EXPECT_LE(current, upper + kTolerance);
        if (start <= end) {
            EXPECT_GE(current, previous - kTolerance);
        } else {
            EXPECT_LE(current, previous + kTolerance);
        }
        previous = current;
    }
}

template <typename Easer>
void expectMonotonicInBothDirections(const Easer& easer) {
    expectMonotonicOnUnitInterval(easer, -3.0f, 7.0f);
    expectMonotonicOnUnitInterval(easer, 7.0f, -3.0f);
}

} // namespace

TEST(EasingFunctionsTest, NewEasingsHitEndpoints) {
    expectEndpoints(EaseOutIn{});
    expectEndpoints(CosineEase{});
    expectEndpoints(CubicIn{});
    expectEndpoints(CubicInOut{});
    expectEndpoints(QuarticIn{});
    expectEndpoints(QuarticOut{});
    expectEndpoints(QuarticInOut{});
    expectEndpoints(QuinticIn{});
    expectEndpoints(QuinticOut{});
    expectEndpoints(QuinticInOut{});
    expectEndpoints(SineIn{});
    expectEndpoints(SineInOut{});
    expectEndpoints(CircularIn{});
    expectEndpoints(CircularOut{});
    expectEndpoints(CircularInOut{});
    expectEndpoints(ExponentialIn{});
    expectEndpoints(ExponentialOut{});
    expectEndpoints(ExponentialInOut{});
    expectEndpoints(LogarithmicEase{});
}

TEST(EasingFunctionsTest, KnownMidpoints) {
    EXPECT_NEAR(CosineEase{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(EaseOutIn{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(CubicIn{}(0.0f, 1.0f, 0.5f), 0.125f, 1e-6f);
    EXPECT_NEAR(CubicInOut{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(QuarticIn{}(0.0f, 1.0f, 0.5f), 0.0625f, 1e-6f);
    EXPECT_NEAR(QuarticOut{}(0.0f, 1.0f, 0.5f), 0.9375f, 1e-6f);
    EXPECT_NEAR(QuarticInOut{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(QuinticIn{}(0.0f, 1.0f, 0.5f), 0.03125f, 1e-6f);
    EXPECT_NEAR(QuinticOut{}(0.0f, 1.0f, 0.5f), 0.96875f, 1e-6f);
    EXPECT_NEAR(QuinticInOut{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(SineIn{}(0.0f, 1.0f, 0.5f), 0.29289322f, 1e-6f);
    EXPECT_NEAR(SineInOut{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(CircularIn{}(0.0f, 1.0f, 0.5f), 0.1339746f, 1e-6f);
    EXPECT_NEAR(CircularOut{}(0.0f, 1.0f, 0.5f), 0.8660254f, 1e-6f);
    EXPECT_NEAR(CircularInOut{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(ExponentialIn{}(0.0f, 1.0f, 0.5f), 0.03125f, 1e-6f);
    EXPECT_NEAR(ExponentialOut{}(0.0f, 1.0f, 0.5f), 0.96875f, 1e-6f);
    EXPECT_NEAR(ExponentialInOut{}(0.0f, 1.0f, 0.5f), 0.5f, 1e-6f);
    EXPECT_NEAR(LogarithmicEase{}(0.0f, 1.0f, 0.5f), 0.62011451f, 1e-6f);
}

TEST(EasingFunctionsTest, DispatchRoutesNewTypes) {
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Quartic), 93.75f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Quintic), 96.875f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Exponential), 96.875f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Circular), 86.60254f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Smooth), 50.0f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Cosine), 50.0f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::EaseOutIn), 50.0f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Quadratic), 25.0f, 1e-3f);
    EXPECT_NEAR(interpolate(0.0f, 100.0f, 0.5f, InterpolationType::Logarithmic), 62.011451f, 1e-3f);
}

TEST(EasingFunctionsTest, KeyframeInterpolatorUsesNewTypes) {
    KeyframeInterpolator<float> interp;
    for (const double t : {0.0, 10.0}) {
        typename KeyframeInterpolator<float>::KeyframeEntry entry;
        entry.time = t;
        entry.value = static_cast<float>(t * 10.0);
        entry.type = InterpolationType::Quartic;
        interp.addKeyframe(entry);
    }
    EXPECT_NEAR(interp.evaluate(5.0), 93.75f, 1e-3f);
}

TEST(EasingFunctionsTest, OutOfRangeAlphaStaysFinite) {
    EXPECT_TRUE(std::isfinite(CircularIn{}(0.0f, 1.0f, 2.0f)));
    EXPECT_TRUE(std::isfinite(CircularOut{}(0.0f, 1.0f, -1.0f)));
    EXPECT_TRUE(std::isfinite(LogarithmicEase{}(0.0f, 1.0f, 2.0f)));
    EXPECT_TRUE(std::isfinite(ExponentialInOut{}(0.0f, 1.0f, -1.0f)));
}

TEST(EasingFunctionsTest, InfiniteAlphaClampsToTheCorrespondingEndpoint) {
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -positiveInfinity;

    EXPECT_NEAR(CircularIn{}(2.0f, 8.0f, negativeInfinity), 2.0f, 1e-6f);
    EXPECT_NEAR(CircularIn{}(2.0f, 8.0f, positiveInfinity), 8.0f, 1e-6f);
    EXPECT_NEAR(ExponentialInOut{}(-4.0f, 6.0f, negativeInfinity), -4.0f, 1e-6f);
    EXPECT_NEAR(ExponentialInOut{}(-4.0f, 6.0f, positiveInfinity), 6.0f, 1e-6f);
    EXPECT_NEAR(LogarithmicEase{}(3.0f, -5.0f, negativeInfinity), 3.0f, 1e-6f);
    EXPECT_NEAR(LogarithmicEase{}(3.0f, -5.0f, positiveInfinity), -5.0f, 1e-6f);
}

TEST(EasingFunctionsTest, MonotonicEaseCurvesStayOrderedAcrossTheInterval)
{
    expectMonotonicInBothDirections(EaseOutIn{});
    expectMonotonicInBothDirections(CosineEase{});
    expectMonotonicInBothDirections(CubicIn{});
    expectMonotonicInBothDirections(CubicOut{});
    expectMonotonicInBothDirections(CubicInOut{});
    expectMonotonicInBothDirections(QuarticIn{});
    expectMonotonicInBothDirections(QuarticOut{});
    expectMonotonicInBothDirections(QuarticInOut{});
    expectMonotonicInBothDirections(QuinticIn{});
    expectMonotonicInBothDirections(QuinticOut{});
    expectMonotonicInBothDirections(QuinticInOut{});
    expectMonotonicInBothDirections(SineIn{});
    expectMonotonicInBothDirections(SineInOut{});
    expectMonotonicInBothDirections(CircularIn{});
    expectMonotonicInBothDirections(CircularOut{});
    expectMonotonicInBothDirections(CircularInOut{});
    expectMonotonicInBothDirections(ExponentialIn{});
    expectMonotonicInBothDirections(ExponentialOut{});
    expectMonotonicInBothDirections(ExponentialInOut{});
    expectMonotonicInBothDirections(LogarithmicEase{});
}
