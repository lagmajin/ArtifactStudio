#include <gtest/gtest.h>

#include <initializer_list>
#include <type_traits>
#include <QVector3D>

import Interpolate.Factory;

using namespace ArtifactCore;

TEST(InterpolatorFactoryStandaloneTest, LinearInterpolatorReturnsStartAndEndAtBoundaries)
{
    const LinearInterpolator<float> interpolator;
    EXPECT_FLOAT_EQ(interpolator.interpolate(2.0f, 10.0f, 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(interpolator.interpolate(2.0f, 10.0f, 1.0f), 10.0f);
}

TEST(InterpolatorFactoryStandaloneTest, LinearInterpolatorUsesAffineInterpolation)
{
    const LinearInterpolator<float> interpolator;
    EXPECT_FLOAT_EQ(interpolator.interpolate(-4.0f, 8.0f, 0.25f), -1.0f);
}

TEST(InterpolatorFactoryStandaloneTest, LinearInterpolatorSupportsExtrapolation)
{
    const LinearInterpolator<float> interpolator;
    EXPECT_FLOAT_EQ(interpolator.interpolate(2.0f, 6.0f, 1.5f), 8.0f);
    EXPECT_FLOAT_EQ(interpolator.interpolate(2.0f, 6.0f, -0.5f), 0.0f);
}

TEST(InterpolatorFactoryStandaloneTest, LinearInterpolatorHandlesDescendingValues)
{
    const LinearInterpolator<float> interpolator;
    EXPECT_FLOAT_EQ(interpolator.interpolate(10.0f, 2.0f, 0.25f), 8.0f);
}

TEST(InterpolatorFactoryStandaloneTest, CubicInterpolatorHasExactEndpoints)
{
    const CubicInterpolator<float> interpolator;
    EXPECT_FLOAT_EQ(interpolator.interpolate(3.0f, 11.0f, 0.0f), 3.0f);
    EXPECT_FLOAT_EQ(interpolator.interpolate(3.0f, 11.0f, 1.0f), 11.0f);
}

TEST(InterpolatorFactoryStandaloneTest, CubicInterpolatorUsesSmoothstepMidpoint)
{
    const CubicInterpolator<float> interpolator;
    EXPECT_FLOAT_EQ(interpolator.interpolate(-2.0f, 6.0f, 0.5f), 2.0f);
}

TEST(InterpolatorFactoryStandaloneTest, CubicInterpolatorHasZeroEndpointSlope)
{
    const CubicInterpolator<float> interpolator;
    const float start = interpolator.interpolate(0.0f, 1.0f, 0.01f);
    const float end = interpolator.interpolate(0.0f, 1.0f, 0.99f);
    EXPECT_LT(start, 0.001f);
    EXPECT_GT(end, 0.999f);
}

TEST(InterpolatorFactoryStandaloneTest, CubicInterpolatorIsComplementSymmetric)
{
    const CubicInterpolator<float> interpolator;
    for (const float t : {0.1f, 0.25f, 0.4f}) {
        EXPECT_NEAR(interpolator.interpolate(0.0f, 1.0f, t) +
                        interpolator.interpolate(0.0f, 1.0f, 1.0f - t),
                    1.0f, 1e-6f);
    }
}

TEST(InterpolatorFactoryStandaloneTest, LinearInterpolatorSupportsQVector3D)
{
    const LinearInterpolator<QVector3D> interpolator;
    const QVector3D result = interpolator.interpolate(
        QVector3D(1.0f, -2.0f, 3.0f), QVector3D(5.0f, 6.0f, -1.0f), 0.25f);
    EXPECT_FLOAT_EQ(result.x(), 2.0f);
    EXPECT_FLOAT_EQ(result.y(), 0.0f);
    EXPECT_FLOAT_EQ(result.z(), 2.0f);
}

TEST(InterpolatorFactoryStandaloneTest, CubicInterpolatorSupportsQVector3D)
{
    const CubicInterpolator<QVector3D> interpolator;
    const QVector3D result = interpolator.interpolate(
        QVector3D(0.0f, 2.0f, -4.0f), QVector3D(8.0f, 10.0f, 4.0f), 0.5f);
    EXPECT_FLOAT_EQ(result.x(), 4.0f);
    EXPECT_FLOAT_EQ(result.y(), 6.0f);
    EXPECT_FLOAT_EQ(result.z(), 0.0f);
}

TEST(InterpolatorFactoryStandaloneTest, InterpolatorBaseIsPolymorphic)
{
    const LinearInterpolator<float> linear;
    const Interpolator<float>& interface = linear;
    EXPECT_FLOAT_EQ(interface.interpolate(4.0f, 12.0f, 0.5f), 8.0f);
}

TEST(InterpolatorFactoryStandaloneTest, InterpolatorBaseHasVirtualDestructor)
{
    EXPECT_TRUE(std::has_virtual_destructor_v<Interpolator<float>>);
}
