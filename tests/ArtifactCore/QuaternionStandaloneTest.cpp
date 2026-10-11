#include <gtest/gtest.h>

#include <cmath>

import Math.Quaternion;

using namespace ArtifactCore;

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfSqrtTwo = 0.7071067811865475244f;
}

TEST(QuaternionStandaloneTest, DefaultAndFactoryIdentityAreUnitIdentity)
{
    const Quaternion defaultValue;
    const Quaternion identity = Quaternion::identity();
    EXPECT_FLOAT_EQ(defaultValue.x, 0.0f);
    EXPECT_FLOAT_EQ(defaultValue.y, 0.0f);
    EXPECT_FLOAT_EQ(defaultValue.z, 0.0f);
    EXPECT_FLOAT_EQ(defaultValue.w, 1.0f);
    EXPECT_FLOAT_EQ(identity.length(), 1.0f);
    EXPECT_FLOAT_EQ(identity.dot(defaultValue), 1.0f);
}

TEST(QuaternionStandaloneTest, AxisAngleConstructionUsesHalfAngle)
{
    const Quaternion value = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, kPi * 0.5f);
    EXPECT_NEAR(value.z, kHalfSqrtTwo, 1e-6f);
    EXPECT_NEAR(value.w, kHalfSqrtTwo, 1e-6f);
    EXPECT_NEAR(value.length(), 1.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, EulerZeroAndDegreeFactoriesAgreeWithIdentity)
{
    const auto radians = Quaternion::fromEuler(0.0f, 0.0f, 0.0f);
    const auto degrees = Quaternion::fromEulerDeg(0.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(radians.dot(Quaternion::identity()), 1.0f);
    EXPECT_FLOAT_EQ(degrees.dot(Quaternion::identity()), 1.0f);
}

TEST(QuaternionStandaloneTest, HamiltonProductHasIdentityOnBothSides)
{
    const Quaternion value(0.1f, -0.2f, 0.3f, 0.9f);
    const Quaternion identity = Quaternion::identity();
    const Quaternion left = identity * value;
    const Quaternion right = value * identity;
    EXPECT_FLOAT_EQ(left.x, value.x);
    EXPECT_FLOAT_EQ(left.y, value.y);
    EXPECT_FLOAT_EQ(left.z, value.z);
    EXPECT_FLOAT_EQ(left.w, value.w);
    EXPECT_FLOAT_EQ(right.x, value.x);
    EXPECT_FLOAT_EQ(right.y, value.y);
    EXPECT_FLOAT_EQ(right.z, value.z);
    EXPECT_FLOAT_EQ(right.w, value.w);
}

TEST(QuaternionStandaloneTest, HamiltonProductComposesAxisRotations)
{
    const Quaternion aroundZ = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, kPi * 0.5f);
    const Quaternion aroundX = Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, kPi * 0.5f);
    const Quaternion composed = aroundZ * aroundX;
    EXPECT_NEAR(composed.length(), 1.0f, 1e-6f);
    EXPECT_NEAR(composed.x, 0.5f, 1e-6f);
    EXPECT_NEAR(composed.y, 0.0f, 1e-6f);
    EXPECT_NEAR(composed.z, 0.5f, 1e-6f);
    EXPECT_NEAR(composed.w, 0.5f, 1e-6f);
}

TEST(QuaternionStandaloneTest, AdditionScalarMultiplyAndNegationOperateComponentwise)
{
    const Quaternion a(1.0f, 2.0f, 3.0f, 4.0f);
    const Quaternion b(-1.0f, 3.0f, 0.0f, 2.0f);
    const Quaternion sum = a + b;
    const Quaternion scaled = a * 0.5f;
    const Quaternion negated = -a;
    EXPECT_FLOAT_EQ(sum.x, 0.0f);
    EXPECT_FLOAT_EQ(sum.y, 5.0f);
    EXPECT_FLOAT_EQ(sum.z, 3.0f);
    EXPECT_FLOAT_EQ(sum.w, 6.0f);
    EXPECT_FLOAT_EQ(scaled.w, 2.0f);
    EXPECT_FLOAT_EQ(negated.z, -3.0f);
}

TEST(QuaternionStandaloneTest, DotLengthAndSquaredLengthAreConsistent)
{
    const Quaternion value(1.0f, 2.0f, 2.0f, 4.0f);
    EXPECT_FLOAT_EQ(value.dot(value), 25.0f);
    EXPECT_FLOAT_EQ(value.lengthSq(), 25.0f);
    EXPECT_FLOAT_EQ(value.length(), 5.0f);
}

TEST(QuaternionStandaloneTest, NormalizationProducesUnitLength)
{
    const Quaternion value = Quaternion(0.0f, 3.0f, 0.0f, 4.0f).normalized();
    EXPECT_FLOAT_EQ(value.y, 0.6f);
    EXPECT_FLOAT_EQ(value.w, 0.8f);
    EXPECT_NEAR(value.length(), 1.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, NearZeroNormalizationFallsBackToIdentity)
{
    const Quaternion value = Quaternion(1e-10f, 0.0f, 0.0f, 0.0f).normalized();
    EXPECT_FLOAT_EQ(value.x, 0.0f);
    EXPECT_FLOAT_EQ(value.y, 0.0f);
    EXPECT_FLOAT_EQ(value.z, 0.0f);
    EXPECT_FLOAT_EQ(value.w, 1.0f);
}

TEST(QuaternionStandaloneTest, ConjugateNegatesVectorPart)
{
    const Quaternion value = Quaternion(1.0f, -2.0f, 3.0f, 4.0f).conjugate();
    EXPECT_FLOAT_EQ(value.x, -1.0f);
    EXPECT_FLOAT_EQ(value.y, 2.0f);
    EXPECT_FLOAT_EQ(value.z, -3.0f);
    EXPECT_FLOAT_EQ(value.w, 4.0f);
}

TEST(QuaternionStandaloneTest, InverseMultipliesToIdentityForNonUnitQuaternion)
{
    const Quaternion value(1.0f, 2.0f, -1.0f, 3.0f);
    const Quaternion result = value * value.inverse();
    EXPECT_NEAR(result.x, 0.0f, 1e-6f);
    EXPECT_NEAR(result.y, 0.0f, 1e-6f);
    EXPECT_NEAR(result.z, 0.0f, 1e-6f);
    EXPECT_NEAR(result.w, 1.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, NearZeroInverseFallsBackToIdentity)
{
    const Quaternion value = Quaternion(0.0f, 0.0f, 0.0f, 1e-5f).inverse();
    EXPECT_FLOAT_EQ(value.dot(Quaternion::identity()), 1.0f);
}

TEST(QuaternionStandaloneTest, RotatePointAppliesUnitAxisRotation)
{
    const Quaternion rotation = Quaternion::fromAxisAngle(
        0.0f, 0.0f, 1.0f, kPi * 0.5f);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    rotation.rotatePoint(1.0f, 0.0f, 0.0f, x, y, z);
    EXPECT_NEAR(x, 0.0f, 1e-6f);
    EXPECT_NEAR(y, 1.0f, 1e-6f);
    EXPECT_NEAR(z, 0.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, EulerConversionRoundTripsAwayFromGimbalLock)
{
    const Quaternion original = Quaternion::fromEuler(0.3f, -0.4f, 0.7f);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    original.toEuler(x, y, z);
    const Quaternion restored = Quaternion::fromEuler(x, y, z);
    EXPECT_NEAR(std::abs(original.dot(restored)), 1.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, AxisAngleConversionReturnsNormalizedAxis)
{
    const Quaternion original = Quaternion::fromAxisAngle(
        0.0f, 1.0f, 0.0f, kPi / 3.0f);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float angle = 0.0f;
    original.toAxisAngle(x, y, z, angle);
    EXPECT_NEAR(x, 0.0f, 1e-6f);
    EXPECT_NEAR(y, 1.0f, 1e-6f);
    EXPECT_NEAR(z, 0.0f, 1e-6f);
    EXPECT_NEAR(angle, kPi / 3.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, IdentityAxisAngleUsesZeroAngle)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float angle = 1.0f;
    Quaternion::identity().toAxisAngle(x, y, z, angle);
    EXPECT_FLOAT_EQ(angle, 0.0f);
    EXPECT_FLOAT_EQ(x, 0.0f);
    EXPECT_FLOAT_EQ(y, 0.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);
}

TEST(QuaternionStandaloneTest, ForwardVectorUsesPositiveZIdentityConvention)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    Quaternion::identity().toForwardVector(x, y, z);
    EXPECT_FLOAT_EQ(x, 0.0f);
    EXPECT_FLOAT_EQ(y, 0.0f);
    EXPECT_FLOAT_EQ(z, 1.0f);
}

TEST(QuaternionStandaloneTest, NlerpReturnsEndpointsAndUnitIntermediate)
{
    const Quaternion a = Quaternion::identity();
    const Quaternion b = Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, kPi * 0.5f);
    EXPECT_NEAR(std::abs(nlerp(a, b, 0.0f).dot(a)), 1.0f, 1e-6f);
    EXPECT_NEAR(std::abs(nlerp(a, b, 1.0f).dot(b)), 1.0f, 1e-6f);
    EXPECT_NEAR(nlerp(a, b, 0.5f).length(), 1.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, SlerpReturnsEndpointsAndGreatCircleMidpoint)
{
    const Quaternion a = Quaternion::identity();
    const Quaternion b = Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, kPi);
    const Quaternion middle = slerp(a, b, 0.5f);
    EXPECT_NEAR(std::abs(slerp(a, b, 0.0f).dot(a)), 1.0f, 1e-6f);
    EXPECT_NEAR(std::abs(slerp(a, b, 1.0f).dot(b)), 1.0f, 1e-6f);
    EXPECT_NEAR(middle.length(), 1.0f, 1e-6f);
    EXPECT_NEAR(angleBetween(a, middle), kPi * 0.5f, 1e-5f);
}

TEST(QuaternionStandaloneTest, SlerpUsesShortestPathForNegatedEquivalentQuaternion)
{
    const Quaternion a = Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 0.8f);
    const Quaternion b = -a;
    const Quaternion middle = slerp(a, b, 0.5f);
    EXPECT_NEAR(std::abs(middle.dot(a)), 1.0f, 1e-6f);
    EXPECT_NEAR(angleBetween(a, b), 0.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, NearEqualSlerpFallsBackToNormalizedInterpolation)
{
    const Quaternion a = Quaternion::identity();
    const Quaternion b(0.0f, 1e-4f, 0.0f, 1.0f);
    EXPECT_NEAR(slerp(a, b, 0.5f).length(), 1.0f, 1e-6f);
}

TEST(QuaternionStandaloneTest, SquadWithIdenticalControlsRemainsThatOrientation)
{
    const Quaternion value = Quaternion::fromEuler(0.2f, -0.3f, 0.4f);
    const Quaternion result = squad(value, value, value, value, 0.37f);
    EXPECT_NEAR(std::abs(result.dot(value)), 1.0f, 1e-6f);
}
