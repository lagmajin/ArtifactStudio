#include <gtest/gtest.h>

#include <cmath>

import Math.Matrix4x4;

using namespace ArtifactCore;

namespace {
constexpr float kTolerance = 1e-6f;
}

TEST(Matrix4x4StandaloneTest, DefaultConstructionIsZeroMatrix)
{
    const Matrix4x4 value;
    for (const float element : value.m) {
        EXPECT_FLOAT_EQ(element, 0.0f);
    }
}

TEST(Matrix4x4StandaloneTest, IdentityHasUnitDiagonalAndZeroElsewhere)
{
    const Matrix4x4 value = Matrix4x4::identity();
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            EXPECT_FLOAT_EQ(value.m[row * 4 + col], row == col ? 1.0f : 0.0f);
        }
    }
}

TEST(Matrix4x4StandaloneTest, TranslationUsesLastColumn)
{
    const Matrix4x4 value = Matrix4x4::translation(2.0f, -3.0f, 4.0f);
    EXPECT_FLOAT_EQ(value.m[3], 2.0f);
    EXPECT_FLOAT_EQ(value.m[7], -3.0f);
    EXPECT_FLOAT_EQ(value.m[11], 4.0f);
    EXPECT_FLOAT_EQ(value.m[15], 1.0f);
}

TEST(Matrix4x4StandaloneTest, TranslationDefaultsZToZero)
{
    const Matrix4x4 value = Matrix4x4::translation(2.0f, 3.0f);
    EXPECT_FLOAT_EQ(value.m[11], 0.0f);
    EXPECT_FLOAT_EQ(value.m[10], 1.0f);
}

TEST(Matrix4x4StandaloneTest, ScaleUsesUnitZByDefault)
{
    const Matrix4x4 value = Matrix4x4::scale(2.0f, 3.0f);
    EXPECT_FLOAT_EQ(value.m[0], 2.0f);
    EXPECT_FLOAT_EQ(value.m[5], 3.0f);
    EXPECT_FLOAT_EQ(value.m[10], 1.0f);
    EXPECT_FLOAT_EQ(value.m[15], 1.0f);
}

TEST(Matrix4x4StandaloneTest, RotationZMapsPositiveXToPositiveY)
{
    const Matrix4x4 value = Matrix4x4::rotationZ(90.0f);
    EXPECT_NEAR(value.m[0], 0.0f, kTolerance);
    EXPECT_NEAR(value.m[1], -1.0f, kTolerance);
    EXPECT_NEAR(value.m[4], 1.0f, kTolerance);
    EXPECT_NEAR(value.m[5], 0.0f, kTolerance);
}

TEST(Matrix4x4StandaloneTest, RotationZZeroEqualsIdentity)
{
    EXPECT_EQ(Matrix4x4::rotationZ(0.0f), Matrix4x4::identity());
}

TEST(Matrix4x4StandaloneTest, MultiplicationByIdentityPreservesBothOperands)
{
    const Matrix4x4 value = Matrix4x4::translation(1.0f, 2.0f, 3.0f)
        * Matrix4x4::scale(2.0f, 3.0f, 4.0f);
    const Matrix4x4 identity = Matrix4x4::identity();
    EXPECT_EQ(identity * value, value);
    EXPECT_EQ(value * identity, value);
}

TEST(Matrix4x4StandaloneTest, MultiplicationUsesRowMajorMatrixProduct)
{
    Matrix4x4 a;
    a.m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    Matrix4x4 b;
    b.m = {16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    const Matrix4x4 result = a * b;
    EXPECT_FLOAT_EQ(result.m[0], 80.0f);
    EXPECT_FLOAT_EQ(result.m[1], 70.0f);
    EXPECT_FLOAT_EQ(result.m[4], 240.0f);
    EXPECT_FLOAT_EQ(result.m[15], 386.0f);
}

TEST(Matrix4x4StandaloneTest, CompositionOrderAppliesRightHandTransformFirst)
{
    const Matrix4x4 composed = Matrix4x4::translation(10.0f, 20.0f)
        * Matrix4x4::scale(2.0f, 3.0f);
    const float x = composed.m[0] * 1.0f + composed.m[1] * 1.0f + composed.m[3];
    const float y = composed.m[4] * 1.0f + composed.m[5] * 1.0f + composed.m[7];
    EXPECT_FLOAT_EQ(x, 12.0f);
    EXPECT_FLOAT_EQ(y, 23.0f);
}

TEST(Matrix4x4StandaloneTest, MultiplyAssignMatchesBinaryMultiplication)
{
    Matrix4x4 assigned = Matrix4x4::translation(3.0f, -2.0f, 5.0f);
    const Matrix4x4 rhs = Matrix4x4::rotationZ(30.0f);
    const Matrix4x4 expected = assigned * rhs;
    assigned *= rhs;
    EXPECT_EQ(assigned, expected);
}

TEST(Matrix4x4StandaloneTest, EqualityAndInequalityCompareAllElements)
{
    const Matrix4x4 value = Matrix4x4::identity();
    Matrix4x4 changed = value;
    changed.m[14] = 1.0f;
    EXPECT_EQ(value, Matrix4x4::identity());
    EXPECT_NE(value, changed);
}
