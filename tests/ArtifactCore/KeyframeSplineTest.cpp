#include <gtest/gtest.h>

#include <cmath>

import Math.Interpolate;

using namespace ArtifactCore;

namespace {

KeyframeInterpolator<float> makeKeys(InterpolationType type) {
    KeyframeInterpolator<float> interp;
    const double values[4] = {0.0, 10.0, 20.0, 30.0};
    for (int i = 0; i < 4; ++i) {
        typename KeyframeInterpolator<float>::KeyframeEntry entry;
        entry.time = static_cast<double>(i);
        entry.value = static_cast<float>(values[i]);
        entry.type = type;
        interp.addKeyframe(entry);
    }
    return interp;
}

} // namespace

TEST(KeyframeSplineTest, HermiteZeroTangentsIsSmoothstep) {
    EXPECT_NEAR(hermiteInterpolate(0.0f, 0.0f, 1.0f, 0.0f, 0.25f), 0.15625f, 1e-6f);
    EXPECT_NEAR(hermiteInterpolate(0.0f, 0.0f, 1.0f, 0.0f, 0.5f), 0.5f, 1e-6f);
}

TEST(KeyframeSplineTest, HermiteEndpointsExact) {
    EXPECT_FLOAT_EQ(hermiteInterpolate(2.0f, 5.0f, 7.0f, -3.0f, 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(hermiteInterpolate(2.0f, 5.0f, 7.0f, -3.0f, 1.0f), 7.0f);
}

TEST(KeyframeSplineTest, CatmullRomCollinearIsLinear) {
    EXPECT_NEAR(catmullRomInterpolate(0.0f, 10.0f, 20.0f, 30.0f, 0.5f), 15.0f, 1e-5f);
    EXPECT_NEAR(catmullRomInterpolate(0.0f, 10.0f, 20.0f, 30.0f, 0.25f), 12.5f, 1e-5f);
}

TEST(KeyframeSplineTest, CatmullRomPassesThroughControls) {
    EXPECT_FLOAT_EQ(catmullRomInterpolate(0.0f, 10.0f, 20.0f, 99.0f, 0.0f), 10.0f);
    EXPECT_FLOAT_EQ(catmullRomInterpolate(0.0f, 10.0f, 20.0f, 99.0f, 1.0f), 20.0f);
}

TEST(KeyframeSplineTest, LinearRegressionUnchanged) {
    auto interp = makeKeys(InterpolationType::Linear);
    EXPECT_FLOAT_EQ(interp.evaluate(0.5), 5.0f);
    EXPECT_FLOAT_EQ(interp.evaluate(2.0), 20.0f);
}

TEST(KeyframeSplineTest, CatmullRomThroughUniformKeys) {
    auto interp = makeKeys(InterpolationType::CatmullRom);
    // Interior segment is exactly linear for uniform data.
    EXPECT_NEAR(interp.evaluate(1.5), 15.0f, 1e-4f);
    EXPECT_FLOAT_EQ(interp.evaluate(2.0), 20.0f);
    // Edge segments duplicate the endpoint key, giving ease-like shaping.
    EXPECT_NEAR(interp.evaluate(0.5), 4.375f, 1e-4f);
    EXPECT_NEAR(interp.evaluate(2.5), 25.625f, 1e-4f);
}

TEST(KeyframeSplineTest, HermitePassesThroughKeys) {
    KeyframeInterpolator<float> interp;
    const double times[4] = {0.0, 1.0, 3.0, 4.0};
    const float values[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    for (int i = 0; i < 4; ++i) {
        typename KeyframeInterpolator<float>::KeyframeEntry entry;
        entry.time = times[i];
        entry.value = values[i];
        entry.type = InterpolationType::Hermite;
        interp.addKeyframe(entry);
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(interp.evaluate(times[i]), values[i]);
    }
    // First segment rises monotonically from 0 to 1.
    const float mid = interp.evaluate(0.5);
    EXPECT_GT(mid, 0.0f);
    EXPECT_LT(mid, 1.0f);
    EXPECT_TRUE(std::isfinite(mid));
}

TEST(KeyframeSplineTest, BezierSymmetricMidpoint) {
    auto interp = makeKeys(InterpolationType::Bezier);
    EXPECT_NEAR(interp.evaluate(0.5), 5.0f, 1e-3f);
}
