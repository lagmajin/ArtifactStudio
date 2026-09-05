#include <gtest/gtest.h>

#include <cmath>

#include <QPointF>
#include <QVector>

import Math.Bezier;
import Math.Bezier.Sampler;

using namespace ArtifactCore;

namespace {

QVector<BezierPoint> makeUnequalLine() {
    // Segment lengths 100 and 1: uniform-t sampling clusters badly here.
    return {
        BezierPoint{QPointF(0.0, 0.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
        BezierPoint{QPointF(100.0, 0.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
        BezierPoint{QPointF(101.0, 0.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
    };
}

double gap(const QPointF& a, const QPointF& b) {
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

TEST(BezierArcLengthTest, UnequalSegmentsSampleEvenly) {
    const auto pts = BezierPathSampler::sampleByCount(makeUnequalLine(), 12, false);
    ASSERT_EQ(pts.size(), 12);
    double minGap = 1.0e100;
    double maxGap = 0.0;
    for (int i = 1; i < pts.size(); ++i) {
        const double g = gap(pts[i - 1], pts[i]);
        minGap = std::min(minGap, g);
        maxGap = std::max(maxGap, g);
    }
    // True length 101 over 11 gaps ~= 9.18 each.
    EXPECT_NEAR(minGap, 101.0 / 11.0, 0.5);
    EXPECT_GT(minGap, 0.0);
    EXPECT_LT(maxGap / minGap, 1.6);
}

TEST(BezierArcLengthTest, EndpointsMatch) {
    const auto line = makeUnequalLine();
    const QPointF head = BezierPathSampler::pointAtArcLength(line, 0.0f, false);
    const QPointF tail = BezierPathSampler::pointAtArcLength(line, 1.0f, false);
    EXPECT_NEAR(head.x(), 0.0, 1e-4);
    EXPECT_NEAR(head.y(), 0.0, 1e-4);
    EXPECT_NEAR(tail.x(), 101.0, 1e-4);
    EXPECT_NEAR(tail.y(), 0.0, 1e-4);
}

TEST(BezierArcLengthTest, StraightTangentIsUnitX) {
    const auto tangent = BezierPathSampler::tangentAtArcLength(makeUnequalLine(), 0.37f, false);
    EXPECT_NEAR(tangent.x(), 1.0, 1e-5);
    EXPECT_NEAR(tangent.y(), 0.0, 1e-5);
}

TEST(BezierArcLengthTest, DegeneratePathStaysFinite) {
    const QVector<BezierPoint> pts = {
        BezierPoint{QPointF(5.0, 5.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
        BezierPoint{QPointF(5.0, 5.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
    };
    const auto sampled = BezierPathSampler::sampleByCount(pts, 5, false);
    ASSERT_EQ(sampled.size(), 5);
    for (const auto& p : sampled) {
        EXPECT_TRUE(std::isfinite(p.x()));
        EXPECT_TRUE(std::isfinite(p.y()));
        EXPECT_NEAR(p.x(), 5.0, 1e-6);
        EXPECT_NEAR(p.y(), 5.0, 1e-6);
    }
    const auto tangent = BezierPathSampler::tangentAtArcLength(pts, 0.5f, false);
    EXPECT_TRUE(std::isfinite(tangent.x()));
    EXPECT_TRUE(std::isfinite(tangent.y()));
}

TEST(BezierArcLengthTest, ClosedLoopReturnsToStart) {
    const QVector<BezierPoint> square = {
        BezierPoint{QPointF(0.0, 0.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
        BezierPoint{QPointF(10.0, 0.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
        BezierPoint{QPointF(10.0, 10.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
        BezierPoint{QPointF(0.0, 10.0), QPointF(0.0, 0.0), QPointF(0.0, 0.0)},
    };
    const auto sampled = BezierPathSampler::sampleArcLength(square, 9, true);
    ASSERT_EQ(sampled.size(), 9);
    EXPECT_NEAR(sampled.first().x(), 0.0, 1e-4);
    EXPECT_NEAR(sampled.first().y(), 0.0, 1e-4);
    // 40 units over 8 gaps = 5.0 each for straight edges.
    for (int i = 1; i < sampled.size(); ++i) {
        EXPECT_NEAR(gap(sampled[i - 1], sampled[i]), 5.0, 0.6);
    }
}

TEST(BezierArcLengthTest, AnalyticTangentMatchesCurve) {
    // Cubic hump: tangent at the apex must be horizontal.
    const QPointF t = BezierCalculator::evaluateTangent(
        QPointF(0.0, 0.0), QPointF(0.0, 10.0), QPointF(10.0, 10.0), QPointF(10.0, 0.0), 0.5f);
    EXPECT_NEAR(t.x(), 1.0, 1e-5);
    EXPECT_NEAR(t.y(), 0.0, 1e-5);
    const QPointF degenerate = BezierCalculator::evaluateTangent(
        QPointF(3.0, 3.0), QPointF(3.0, 3.0), QPointF(3.0, 3.0), QPointF(3.0, 3.0), 0.5f);
    EXPECT_FLOAT_EQ(degenerate.x(), 1.0f);
    EXPECT_FLOAT_EQ(degenerate.y(), 0.0f);
}
