#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <QPointF>
#include <QVector>

import Math.Bezier;
import Math.Bezier.Sampler;

using namespace ArtifactCore;

namespace {
BezierPoint linePoint(double x, double y)
{
    return {QPointF(x, y), QPointF(), QPointF()};
}

QVector<BezierPoint> straightLine()
{
    return {linePoint(0.0, 0.0), linePoint(12.0, 0.0)};
}

double pointDistance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}
}

TEST(BezierPathSamplerStandaloneTest, EmptyPathProducesEmptyStandardSamples)
{
    EXPECT_TRUE(BezierPathSampler::sampleStandard({}, 4).isEmpty());
    EXPECT_TRUE(BezierPathSampler::sampleArcLength({}, 4).isEmpty());
    EXPECT_FLOAT_EQ(BezierPathSampler::calculatePathLength({}), 0.0f);
}

TEST(BezierPathSamplerStandaloneTest, SinglePointEquidistantPathReturnsThatPoint)
{
    const QVector<BezierPoint> path{linePoint(3.0, -2.0)};
    const auto samples = BezierPathSampler::sampleEquidistant(path, 1.0f);
    ASSERT_EQ(samples.size(), 1);
    EXPECT_EQ(samples.front(), QPointF(3.0, -2.0));
}

TEST(BezierPathSamplerStandaloneTest, StandardSamplingIncludesBothOpenEndpoints)
{
    const auto samples = BezierPathSampler::sampleStandard(straightLine(), 3);
    ASSERT_EQ(samples.size(), 4);
    EXPECT_EQ(samples.front(), QPointF(0.0, 0.0));
    EXPECT_EQ(samples.back(), QPointF(12.0, 0.0));
    EXPECT_NEAR(samples[1].x(), 4.0, 1e-6);
    EXPECT_NEAR(samples[2].x(), 8.0, 1e-6);
}

TEST(BezierPathSamplerStandaloneTest, ClosedStandardSamplingDoesNotAppendDuplicateStart)
{
    const QVector<BezierPoint> loop{
        linePoint(0.0, 0.0), linePoint(4.0, 0.0), linePoint(4.0, 3.0)};
    const auto samples = BezierPathSampler::sampleStandard(loop, 1, true);
    ASSERT_EQ(samples.size(), 3);
    EXPECT_EQ(samples[0], QPointF(0.0, 0.0));
    EXPECT_EQ(samples[1], QPointF(4.0, 0.0));
    EXPECT_EQ(samples[2], QPointF(4.0, 3.0));
}

TEST(BezierPathSamplerStandaloneTest, StraightOpenPathLengthMatchesEndpointDistance)
{
    EXPECT_NEAR(BezierPathSampler::calculatePathLength(straightLine()), 12.0, 1e-4);
}

TEST(BezierPathSamplerStandaloneTest, ClosedPathLengthIncludesClosingSegment)
{
    const QVector<BezierPoint> triangle{
        linePoint(0.0, 0.0), linePoint(4.0, 0.0), linePoint(4.0, 3.0)};
    EXPECT_NEAR(BezierPathSampler::calculatePathLength(triangle, true), 12.0, 1e-4);
}

TEST(BezierPathSamplerStandaloneTest, EquidistantSamplingRejectsInvalidSpacing)
{
    const auto path = straightLine();
    EXPECT_TRUE(BezierPathSampler::sampleEquidistant(path, 0.0f).isEmpty());
    EXPECT_TRUE(BezierPathSampler::sampleEquidistant(path, -1.0f).isEmpty());
    EXPECT_TRUE(BezierPathSampler::sampleEquidistant(
        path, std::numeric_limits<float>::infinity()).isEmpty());
}

TEST(BezierPathSamplerStandaloneTest, ArcLengthSamplingRejectsCountsBelowTwo)
{
    EXPECT_TRUE(BezierPathSampler::sampleByCount(straightLine(), 0).isEmpty());
    EXPECT_TRUE(BezierPathSampler::sampleArcLength(straightLine(), 1).isEmpty());
}

TEST(BezierPathSamplerStandaloneTest, ArcLengthSamplingIncludesOpenEndpoints)
{
    const auto samples = BezierPathSampler::sampleByCount(straightLine(), 5);
    ASSERT_EQ(samples.size(), 5);
    EXPECT_EQ(samples.front(), QPointF(0.0, 0.0));
    EXPECT_EQ(samples.back(), QPointF(12.0, 0.0));
    for (int i = 1; i < samples.size(); ++i) {
        EXPECT_NEAR(pointDistance(samples[i - 1], samples[i]), 3.0, 2e-2);
    }
}

TEST(BezierPathSamplerStandaloneTest, PointAtClampsParameterToPathEndpoints)
{
    const auto path = straightLine();
    EXPECT_EQ(BezierPathSampler::pointAt(path, -1.0f), QPointF(0.0, 0.0));
    EXPECT_EQ(BezierPathSampler::pointAt(path, 2.0f), QPointF(12.0, 0.0));
}

TEST(BezierPathSamplerStandaloneTest, ArcLengthPositionClampsNormalizedDistance)
{
    const auto path = straightLine();
    EXPECT_EQ(BezierPathSampler::pointAtArcLength(path, -1.0f), QPointF(0.0, 0.0));
    EXPECT_EQ(BezierPathSampler::pointAtArcLength(path, 2.0f), QPointF(12.0, 0.0));
}

TEST(BezierPathSamplerStandaloneTest, StraightPathTangentIsNormalizedPositiveX)
{
    const QPointF tangent = BezierPathSampler::tangentAt(straightLine(), 0.4f);
    EXPECT_NEAR(tangent.x(), 1.0, 1e-6);
    EXPECT_NEAR(tangent.y(), 0.0, 1e-6);
    EXPECT_NEAR(pointDistance(tangent, QPointF()), 1.0, 1e-6);
}

TEST(BezierPathSamplerStandaloneTest, DegenerateTangentUsesFinitePositiveXFallback)
{
    const QVector<BezierPoint> path{linePoint(2.0, 5.0), linePoint(2.0, 5.0)};
    const QPointF tangent = BezierPathSampler::tangentAtArcLength(path, 0.5f);
    EXPECT_TRUE(std::isfinite(tangent.x()));
    EXPECT_TRUE(std::isfinite(tangent.y()));
    EXPECT_FLOAT_EQ(tangent.x(), 1.0f);
    EXPECT_FLOAT_EQ(tangent.y(), 0.0f);
}

TEST(BezierPathSamplerStandaloneTest, TangentSamplingReturnsRequestedCountAndUnitDirections)
{
    const auto samples = BezierPathSampler::sampleWithTangents(straightLine(), 4);
    ASSERT_EQ(samples.size(), 4);
    for (const auto& sample : samples) {
        EXPECT_TRUE(std::isfinite(sample.position.x()));
        EXPECT_TRUE(std::isfinite(sample.position.y()));
        EXPECT_NEAR(pointDistance(sample.tangent, QPointF()), 1.0, 1e-6);
    }
}

TEST(BezierPathSamplerStandaloneTest, TangentSamplingRejectsCountBelowTwo)
{
    EXPECT_TRUE(BezierPathSampler::sampleWithTangents(straightLine(), 1).isEmpty());
    EXPECT_TRUE(BezierPathSampler::sampleWithTangents(straightLine(), 0).isEmpty());
}

TEST(BezierPathSamplerStandaloneTest, AdaptiveSamplingProducesFinitePoints)
{
    const QVector<BezierPoint> curve{
        {QPointF(0.0, 0.0), QPointF(), QPointF(0.0, 10.0)},
        {QPointF(10.0, 0.0), QPointF(0.0, 10.0), QPointF()}};
    const auto samples = BezierPathSampler::sampleAdaptive(curve, 0.1f);
    ASSERT_GE(samples.size(), 2);
    for (const auto& point : samples) {
        EXPECT_TRUE(std::isfinite(point.x()));
        EXPECT_TRUE(std::isfinite(point.y()));
    }
}

TEST(BezierPathSamplerStandaloneTest, QuadraticEvaluationReturnsBothEndpoints)
{
    const QPointF p0(1.0, 2.0);
    const QPointF p1(5.0, 10.0);
    const QPointF p2(9.0, -2.0);
    EXPECT_EQ(BezierCalculator::evaluateQuadratic(p0, p1, p2, 0.0f), p0);
    EXPECT_EQ(BezierCalculator::evaluateQuadratic(p0, p1, p2, 1.0f), p2);
}

TEST(BezierPathSamplerStandaloneTest, QuadraticEvaluationUsesBernsteinWeights)
{
    const QPointF value = BezierCalculator::evaluateQuadratic(
        QPointF(0.0, 0.0), QPointF(5.0, 10.0), QPointF(10.0, 0.0), 0.5f);
    EXPECT_NEAR(value.x(), 5.0, 1e-6);
    EXPECT_NEAR(value.y(), 5.0, 1e-6);
}

TEST(BezierPathSamplerStandaloneTest, CubicEvaluationReturnsBothEndpoints)
{
    const QPointF p0(1.0, 2.0);
    const QPointF p1(3.0, 8.0);
    const QPointF p2(7.0, -4.0);
    const QPointF p3(9.0, 1.0);
    EXPECT_EQ(BezierCalculator::evaluateCubic(p0, p1, p2, p3, 0.0f), p0);
    EXPECT_EQ(BezierCalculator::evaluateCubic(p0, p1, p2, p3, 1.0f), p3);
}

TEST(BezierPathSamplerStandaloneTest, CubicEvaluationMatchesSymmetricCurveMidpoint)
{
    const QPointF value = BezierCalculator::evaluateCubic(
        QPointF(0.0, 0.0), QPointF(0.0, 10.0),
        QPointF(10.0, 10.0), QPointF(10.0, 0.0), 0.5f);
    EXPECT_NEAR(value.x(), 5.0, 1e-6);
    EXPECT_NEAR(value.y(), 7.5, 1e-6);
}

TEST(BezierPathSamplerStandaloneTest, PathEvaluationSelectsSegmentByGlobalParameter)
{
    const QVector<BezierPoint> path{
        linePoint(0.0, 0.0), linePoint(10.0, 0.0), linePoint(20.0, 0.0)};
    EXPECT_NEAR(BezierCalculator::evaluatePath(path, 0.25f).x(), 5.0, 1e-6);
    EXPECT_EQ(BezierCalculator::evaluatePath(path, 0.5f), QPointF(10.0, 0.0));
    EXPECT_NEAR(BezierCalculator::evaluatePath(path, 0.75f).x(), 15.0, 1e-6);
}

TEST(BezierPathSamplerStandaloneTest, ClosedPathEvaluationEndsAtItsStartingPoint)
{
    const QVector<BezierPoint> loop{
        linePoint(0.0, 0.0), linePoint(10.0, 0.0), linePoint(10.0, 10.0)};
    EXPECT_EQ(BezierCalculator::evaluatePath(loop, 0.0f, true), QPointF(0.0, 0.0));
    EXPECT_EQ(BezierCalculator::evaluatePath(loop, 1.0f, true), QPointF(0.0, 0.0));
}

TEST(BezierPathSamplerStandaloneTest, TangentAngleUsesDegrees)
{
    const float angle = BezierCalculator::getTangentAngle(
        QPointF(0.0, 0.0), QPointF(0.0, 1.0),
        QPointF(0.0, 2.0), QPointF(0.0, 3.0), 0.5f);
    EXPECT_NEAR(angle, 90.0f, 1e-5f);
}

TEST(BezierPathSamplerStandaloneTest, AnalyticTangentIsNormalizedAndClampsParameter)
{
    const QPointF p0(0.0, 0.0);
    const QPointF p1(0.0, 10.0);
    const QPointF p2(10.0, 10.0);
    const QPointF p3(10.0, 0.0);
    const QPointF start = BezierCalculator::evaluateTangent(p0, p1, p2, p3, -1.0f);
    const QPointF end = BezierCalculator::evaluateTangent(p0, p1, p2, p3, 2.0f);
    EXPECT_NEAR(pointDistance(start, QPointF()), 1.0, 1e-6);
    EXPECT_NEAR(pointDistance(end, QPointF()), 1.0, 1e-6);
    EXPECT_NEAR(start.x(), 0.0, 1e-6);
    EXPECT_NEAR(start.y(), 1.0, 1e-6);
    EXPECT_NEAR(end.x(), 0.0, 1e-6);
    EXPECT_NEAR(end.y(), -1.0, 1e-6);
}

TEST(BezierPathSamplerStandaloneTest, EmptyAndSinglePointPathEvaluationIsDefined)
{
    EXPECT_EQ(BezierCalculator::evaluatePath({}, 0.5f), QPointF());
    const QVector<BezierPoint> onePoint{linePoint(7.0, -3.0)};
    EXPECT_EQ(BezierCalculator::evaluatePath(onePoint, 0.25f), QPointF(7.0, -3.0));
}
