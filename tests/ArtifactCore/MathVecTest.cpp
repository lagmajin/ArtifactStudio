#include <gtest/gtest.h>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <cmath>
#include <limits>

import Math.Vec;

using namespace ArtifactCore;

namespace {

constexpr float kEps = 1e-5f;

}

TEST(MathVecTest, AliasTypesAreGlm)
{
    static_assert(std::is_same_v<vec3, glm::vec3>);
    static_assert(std::is_same_v<mat4, glm::mat4>);

    vec3 v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);

    // glm API がそのまま使える
    const vec3 sum = v + vec3(1.0f);
    EXPECT_TRUE(epsilonEqual(sum, vec3(2.0f, 3.0f, 4.0f)));
}

TEST(MathVecTest, SafeNormalizeHandlesZeroVector)
{
    const vec3 zero(0.0f);
    const vec3 result = safeNormalize(zero, vec3(0.0f, 1.0f, 0.0f));
    EXPECT_TRUE(epsilonEqual(result, vec3(0.0f, 1.0f, 0.0f)));

    // 通常ベクトルは長さ 1
    const vec3 normal = safeNormalize(vec3(3.0f, 0.0f, 4.0f));
    EXPECT_NEAR(glm::length(normal), 1.0f, kEps);
    EXPECT_TRUE(std::isfinite(normal.x));
    EXPECT_TRUE(std::isfinite(normal.y));
    EXPECT_TRUE(std::isfinite(normal.z));

    // 非常に小さいベクトル (デノーマル級) も NaN にしない
    const vec3 tiny(1e-20f, 0.0f, 0.0f);
    const vec3 tinyResult = safeNormalize(tiny, vec3(1.0f, 0.0f, 0.0f));
    EXPECT_TRUE(isFinite(tinyResult));
}

TEST(MathVecTest, DistanceSqAndComponentOps)
{
    EXPECT_FLOAT_EQ(distanceSq(vec3(0.0f), vec3(3.0f, 4.0f, 0.0f)), 25.0f);

    const vec3 a(1.0f, 5.0f, -3.0f);
    const vec3 b(2.0f, -1.0f, 7.0f);
    EXPECT_TRUE(epsilonEqual(perComponentMin(a, b), vec3(1.0f, -1.0f, -3.0f)));
    EXPECT_TRUE(epsilonEqual(perComponentMax(a, b), vec3(2.0f, 5.0f, 7.0f)));
    EXPECT_TRUE(epsilonEqual(perComponentAbs(a), vec3(1.0f, 5.0f, 3.0f)));
    EXPECT_TRUE(epsilonEqual(clampComponents(vec3(10.0f, -10.0f, 0.5f), 0.0f, 1.0f),
                             vec3(1.0f, 0.0f, 0.5f)));
}

TEST(MathVecTest, IsFiniteDetectsNonFinite)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    EXPECT_TRUE(isFinite(vec3(1.0f, 2.0f, 3.0f)));
    EXPECT_FALSE(isFinite(vec3(nan, 0.0f, 0.0f)));
    EXPECT_FALSE(isFinite(vec4(0.0f, inf, 0.0f, 0.0f)));

    mat4 m(1.0f);
    EXPECT_TRUE(isFinite(m));
    m[0][0] = nan;
    EXPECT_FALSE(isFinite(m));
}

TEST(MathVecTest, EpsilonEqualTolerance)
{
    EXPECT_TRUE(epsilonEqual(vec3(1.0f, 2.0f, 3.0f),
                             vec3(1.0f + 1e-6f, 2.0f, 3.0f)));
    EXPECT_FALSE(epsilonEqual(vec3(1.0f, 2.0f, 3.0f),
                              vec3(1.1f, 2.0f, 3.0f)));
}

TEST(MathVecTest, QVectorRoundTripPreservesComponents)
{
    const vec3 original(1.5f, -2.25f, 3.75f);
    const QVector3D q = toQVector3D(original);
    EXPECT_FLOAT_EQ(q.x(), 1.5f);
    EXPECT_FLOAT_EQ(q.y(), -2.25f);
    EXPECT_FLOAT_EQ(q.z(), 3.75f);

    const vec3 back = toVec3(q);
    EXPECT_TRUE(epsilonEqual(original, back));

    const vec2 v2(0.25f, -8.0f);
    const QVector2D q2 = toQVector2D(v2);
    const vec2 back2 = toVec2(q2);
    EXPECT_TRUE(epsilonEqual(v2, back2));
}

TEST(MathVecTest, MatrixRoundTripPreservesValues)
{
    // glm::translate で非自明行列を作り、Qt 往復で数値一致を確認
    const mat4 glmMat = glm::translate(mat4(1.0f), vec3(1.0f, 2.0f, 3.0f));

    const QMatrix4x4 qtMat = toQMatrix4x4(glmMat);

    // Qt 側の同値演算と一致するか (column-major 互換の検証)
    QMatrix4x4 qtExpected;
    qtExpected.translate(1.0f, 2.0f, 3.0f);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            EXPECT_FLOAT_EQ(qtMat(r, c), qtExpected(r, c)) << "c=" << c << " r=" << r;
        }
    }

    const mat4 back = toGlmMat4(qtMat);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            EXPECT_FLOAT_EQ(back[c][r], glmMat[c][r]) << "c=" << c << " r=" << r;
        }
    }
}

TEST(MathVecTest, PointConversionUsesQReal)
{
    const QPointF p(12.5, -3.25);
    const vec2 v = toVec2(p);
    EXPECT_FLOAT_EQ(v.x, 12.5f);
    EXPECT_FLOAT_EQ(v.y, -3.25f);

    const QPointF back = toQPointF(v);
    EXPECT_DOUBLE_EQ(back.x(), 12.5);
    EXPECT_DOUBLE_EQ(back.y(), -3.25);
}
