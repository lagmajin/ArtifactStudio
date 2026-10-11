#include <gtest/gtest.h>

#include <limits>

import Audio.Spatial.Math;
import Audio.Spatial.Params;

using namespace ArtifactCore::Audio::Spatial;

TEST(AudioSpatialMathStandaloneTest, FastReciprocalSquareRootApproximatesPositiveValues)
{
    EXPECT_NEAR(fastRsqrt(4.0f), 0.5f, 1e-6f);
    EXPECT_NEAR(fastRsqrt(1.0f), 1.0f, 1e-6f);
}

TEST(AudioSpatialMathStandaloneTest, FastReciprocalSquareRootReturnsZeroForInvalidInputs)
{
    EXPECT_FLOAT_EQ(fastRsqrt(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(fastRsqrt(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(fastRsqrt(std::numeric_limits<float>::infinity()), 0.0f);
    EXPECT_FLOAT_EQ(fastRsqrt(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(AudioSpatialMathStandaloneTest, DistanceAtOrBelowMinimumHasNoAttenuation)
{
    SpatialParams params;
    EXPECT_FLOAT_EQ(distanceAttenuation(0.5f, params), 1.0f);
    EXPECT_FLOAT_EQ(distanceAttenuation(1.0f, params), 1.0f);
}

TEST(AudioSpatialMathStandaloneTest, LinearDistanceAttenuationInterpolatesAndClamps)
{
    SpatialParams params;
    params.model = DistanceModel::Linear;
    params.minDistance = 1.0f;
    params.maxDistance = 11.0f;
    params.rolloff = 0.8f;
    EXPECT_NEAR(distanceAttenuation(6.0f, params), 0.6f, 1e-6f);
    EXPECT_FLOAT_EQ(distanceAttenuation(11.0f, params), 0.0f);
}

TEST(AudioSpatialMathStandaloneTest, InverseDistanceAttenuationUsesMinimumAndRolloff)
{
    SpatialParams params;
    params.model = DistanceModel::Inverse;
    params.minDistance = 2.0f;
    params.maxDistance = 20.0f;
    params.rolloff = 1.0f;
    EXPECT_NEAR(distanceAttenuation(6.0f, params), 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(distanceAttenuation(20.0f, params), 2.0f / 20.0f, 1e-6f);
}

TEST(AudioSpatialMathStandaloneTest, ExponentialDistanceAttenuationUsesDistanceRatio)
{
    SpatialParams params;
    params.model = DistanceModel::Exponential;
    params.minDistance = 2.0f;
    params.maxDistance = 20.0f;
    params.rolloff = 2.0f;
    EXPECT_NEAR(distanceAttenuation(4.0f, params), 0.25f, 1e-6f);
    EXPECT_NEAR(distanceAttenuation(20.0f, params), 0.01f, 1e-6f);
}

TEST(AudioSpatialMathStandaloneTest, ConeGainHonorsOmnidirectionalAndBoundaryCases)
{
    SpatialParams params;
    params.coneInnerAngle = 360.0f;
    EXPECT_FLOAT_EQ(coneGain(180.0f, params), 1.0f);

    params.coneInnerAngle = 60.0f;
    params.coneOuterAngle = 120.0f;
    params.coneOuterGain = 0.2f;
    EXPECT_FLOAT_EQ(coneGain(30.0f, params), 1.0f);
    EXPECT_FLOAT_EQ(coneGain(60.0f, params), 0.2f);
}

TEST(AudioSpatialMathStandaloneTest, ConeGainSmoothlyInterpolatesBetweenAngles)
{
    SpatialParams params;
    params.coneInnerAngle = 60.0f;
    params.coneOuterAngle = 120.0f;
    params.coneOuterGain = 0.0f;
    EXPECT_NEAR(coneGain(45.0f, params), 0.5f, 1e-6f);
}

TEST(AudioSpatialMathStandaloneTest, VectorSubtractionIsComponentwise)
{
    const Vec3 value = vecSub({5.0f, -2.0f, 8.0f}, {1.0f, 3.0f, 4.0f});
    EXPECT_FLOAT_EQ(value.x, 4.0f);
    EXPECT_FLOAT_EQ(value.y, -5.0f);
    EXPECT_FLOAT_EQ(value.z, 4.0f);
}

TEST(AudioSpatialMathStandaloneTest, QuaternionConjugateNegatesVectorPart)
{
    const Quat value = quatConjugate({1.0f, -2.0f, 3.0f, 4.0f});
    EXPECT_FLOAT_EQ(value.x, -1.0f);
    EXPECT_FLOAT_EQ(value.y, 2.0f);
    EXPECT_FLOAT_EQ(value.z, -3.0f);
    EXPECT_FLOAT_EQ(value.w, 4.0f);
}

TEST(AudioSpatialMathStandaloneTest, QuaternionRotationPreservesVectorForIdentity)
{
    const Vec3 value = quatRotate({0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 2.0f, 3.0f});
    EXPECT_FLOAT_EQ(value.x, 1.0f);
    EXPECT_FLOAT_EQ(value.y, 2.0f);
    EXPECT_FLOAT_EQ(value.z, 3.0f);
}

TEST(AudioSpatialMathStandaloneTest, QuaternionRotationRotatesAroundYAxis)
{
    constexpr float halfSqrtTwo = 0.7071067811865475f;
    const Vec3 value = quatRotate(
        {0.0f, halfSqrtTwo, 0.0f, halfSqrtTwo}, {0.0f, 0.0f, -1.0f});
    EXPECT_NEAR(value.x, -1.0f, 1e-6f);
    EXPECT_NEAR(value.y, 0.0f, 1e-6f);
    EXPECT_NEAR(value.z, 0.0f, 1e-6f);
}

TEST(AudioSpatialMathStandaloneTest, SphericalCoordinatesMapFrontRightAndUp)
{
    float azimuth = 0.0f;
    float elevation = 0.0f;
    toSpherical({0.0f, 0.0f, -1.0f}, azimuth, elevation);
    EXPECT_NEAR(azimuth, 0.0f, 1e-5f);
    EXPECT_NEAR(elevation, 0.0f, 1e-5f);

    toSpherical({1.0f, 0.0f, 0.0f}, azimuth, elevation);
    EXPECT_NEAR(azimuth, 90.0f, 1e-5f);
    EXPECT_NEAR(elevation, 0.0f, 1e-5f);

    toSpherical({0.0f, 1.0f, 0.0f}, azimuth, elevation);
    EXPECT_NEAR(azimuth, 0.0f, 1e-5f);
    EXPECT_NEAR(elevation, 90.0f, 1e-5f);
}

TEST(AudioSpatialMathStandaloneTest, SphericalCoordinatesDefineZeroVectorAsOrigin)
{
    float azimuth = 17.0f;
    float elevation = -23.0f;
    toSpherical({0.0f, 0.0f, 0.0f}, azimuth, elevation);
    EXPECT_FLOAT_EQ(azimuth, 0.0f);
    EXPECT_FLOAT_EQ(elevation, 0.0f);
}
