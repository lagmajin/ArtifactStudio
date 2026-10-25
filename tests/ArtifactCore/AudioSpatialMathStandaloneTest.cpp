#include <gtest/gtest.h>

#include <cmath>
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

TEST(AudioSpatialMathStandaloneTest, FastReciprocalSquareRootHonorsTinyInputThreshold)
{
    constexpr float threshold = 1.0e-12f;
    const float aboveThreshold = std::nextafter(
        threshold, std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ(fastRsqrt(std::nextafter(threshold, 0.0f)), 0.0f);
    EXPECT_FLOAT_EQ(fastRsqrt(threshold), 0.0f);
    const float reciprocal = fastRsqrt(aboveThreshold);
    EXPECT_TRUE(std::isfinite(reciprocal));
    EXPECT_GT(reciprocal, 0.0f);
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

TEST(AudioSpatialMathStandaloneTest, ZeroWidthDistanceRangeUsesBoundaryBranches)
{
    SpatialParams params;
    params.model = DistanceModel::Linear;
    params.minDistance = 4.0f;
    params.maxDistance = 4.0f;

    const auto sanitized = sanitizedSpatialParams(params);
    EXPECT_FLOAT_EQ(sanitized.minDistance, 4.0f);
    EXPECT_FLOAT_EQ(sanitized.maxDistance, 4.0f);
    EXPECT_FLOAT_EQ(distanceAttenuation(4.0f, sanitized), 1.0f);
    EXPECT_FLOAT_EQ(distanceAttenuation(4.001f, sanitized), 0.0f);
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

TEST(AudioSpatialMathStandaloneTest, InfiniteDistancesUseNearAndFarBoundaryBehavior)
{
    SpatialParams params;
    params.minDistance = 1.0f;
    params.maxDistance = 10.0f;
    params.rolloff = 1.0f;
    const float negativeInfinity = -std::numeric_limits<float>::infinity();
    const float positiveInfinity = std::numeric_limits<float>::infinity();

    params.model = DistanceModel::Linear;
    EXPECT_FLOAT_EQ(distanceAttenuation(negativeInfinity, params), 1.0f);
    EXPECT_FLOAT_EQ(distanceAttenuation(positiveInfinity, params), 0.0f);

    params.model = DistanceModel::Inverse;
    EXPECT_FLOAT_EQ(distanceAttenuation(negativeInfinity, params), 1.0f);
    EXPECT_NEAR(distanceAttenuation(positiveInfinity, params), 0.1f, 1e-6f);

    params.model = DistanceModel::Exponential;
    EXPECT_FLOAT_EQ(distanceAttenuation(negativeInfinity, params), 1.0f);
    EXPECT_NEAR(distanceAttenuation(positiveInfinity, params), 0.1f, 1e-6f);
}

TEST(AudioSpatialMathStandaloneTest, NaNDistanceDoesNotProduceNonFiniteAttenuation)
{
    SpatialParams params;
    params.minDistance = 1.0f;
    params.maxDistance = 10.0f;
    params.rolloff = 1.0f;
    const float nanDistance = std::numeric_limits<float>::quiet_NaN();

    for (const DistanceModel model : {DistanceModel::Linear,
                                      DistanceModel::Inverse,
                                      DistanceModel::Exponential}) {
        params.model = model;
        EXPECT_TRUE(std::isfinite(distanceAttenuation(nanDistance, params)));
    }
}

TEST(AudioSpatialMathStandaloneTest, InvalidDistanceModelUsesSanitizedInverseFallback)
{
    SpatialParams params;
    params.model = static_cast<DistanceModel>(255);
    params.minDistance = 2.0f;
    params.maxDistance = 20.0f;
    params.rolloff = 1.0f;

    const auto sanitized = sanitizedSpatialParams(params);
    EXPECT_EQ(sanitized.model, DistanceModel::Inverse);
    EXPECT_NEAR(distanceAttenuation(6.0f, sanitized), 1.0f / 3.0f, 1e-6f);
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

TEST(AudioSpatialMathStandaloneTest, UnitQuaternionRotationPreservesVectorLength)
{
    const float inverseQuaternionLength = 1.0f / std::sqrt(0.54f);
    const Quat rotation{0.2f * inverseQuaternionLength,
                        0.3f * inverseQuaternionLength,
                        0.4f * inverseQuaternionLength,
                        0.5f * inverseQuaternionLength};
    const Vec3 input{2.0f, -3.0f, 5.0f};
    const Vec3 rotated = quatRotate(rotation, input);
    const float inputLengthSquared =
        input.x * input.x + input.y * input.y + input.z * input.z;
    const float rotatedLengthSquared =
        rotated.x * rotated.x + rotated.y * rotated.y + rotated.z * rotated.z;

    EXPECT_NEAR(rotatedLengthSquared, inputLengthSquared, 1e-4f);
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

TEST(AudioSpatialMathStandaloneTest, SphericalCoordinatesAreInvariantToPositiveScale)
{
    float baseAzimuth = 0.0f;
    float baseElevation = 0.0f;
    float scaledAzimuth = 0.0f;
    float scaledElevation = 0.0f;
    toSpherical({1.0f, 2.0f, -3.0f}, baseAzimuth, baseElevation);
    toSpherical({7.0f, 14.0f, -21.0f}, scaledAzimuth, scaledElevation);

    EXPECT_NEAR(scaledAzimuth, baseAzimuth, 1e-5f);
    EXPECT_NEAR(scaledElevation, baseElevation, 1e-5f);
}
