#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

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

TEST(BezierArcLengthTest, CurvedArcLengthSamplesMatchDenseIndependentReference)
{
    constexpr int referenceSubdivisions = 20000;
    constexpr int sampleCount = 13;
    const QPointF p0(0.0, 0.0);
    const QPointF p1(0.0, 80.0);
    const QPointF p2(100.0, -80.0);
    const QPointF p3(100.0, 0.0);
    const auto cubic = [&](double t) {
        const double u = 1.0 - t;
        const double a = u * u * u;
        const double b = 3.0 * u * u * t;
        const double c = 3.0 * u * t * t;
        const double d = t * t * t;
        return QPointF(a * p0.x() + b * p1.x() + c * p2.x() + d * p3.x(),
                       a * p0.y() + b * p1.y() + c * p2.y() + d * p3.y());
    };

    std::vector<QPointF> referencePoints;
    referencePoints.reserve(referenceSubdivisions + 1);
    for (int index = 0; index <= referenceSubdivisions; ++index) {
        referencePoints.push_back(cubic(
            static_cast<double>(index) / referenceSubdivisions));
    }
    std::vector<double> cumulativeLengths(referencePoints.size(), 0.0);
    for (std::size_t index = 1; index < referencePoints.size(); ++index) {
        cumulativeLengths[index] = cumulativeLengths[index - 1] +
            gap(referencePoints[index - 1], referencePoints[index]);
    }
    const double totalLength = cumulativeLengths.back();

    const QVector<BezierPoint> curve = {
        BezierPoint{p0, QPointF(), p1 - p0},
        BezierPoint{p3, p2 - p3, QPointF()},
    };
    const auto samples = BezierPathSampler::sampleByCount(curve, sampleCount, false);
    ASSERT_EQ(samples.size(), sampleCount);

    for (int sample = 0; sample < sampleCount; ++sample) {
        const double normalizedDistance =
            static_cast<double>(sample) / (sampleCount - 1);
        const double targetLength = normalizedDistance * totalLength;
        const auto upper = std::lower_bound(cumulativeLengths.begin(),
                                            cumulativeLengths.end(), targetLength);
        const std::size_t upperIndex = static_cast<std::size_t>(
            upper - cumulativeLengths.begin());
        QPointF expected = referencePoints.back();
        if (upperIndex == 0) {
            expected = referencePoints.front();
        } else if (upperIndex < referencePoints.size()) {
            const std::size_t lowerIndex = upperIndex - 1;
            const double segmentLength = cumulativeLengths[upperIndex] -
                                         cumulativeLengths[lowerIndex];
            const double fraction = segmentLength > 0.0
                ? (targetLength - cumulativeLengths[lowerIndex]) / segmentLength
                : 0.0;
            expected = referencePoints[lowerIndex] +
                (referencePoints[upperIndex] - referencePoints[lowerIndex]) * fraction;
        }

        EXPECT_LT(gap(samples[sample], expected), 0.15)
            << "sample=" << sample << " normalized distance=" << normalizedDistance;
    }
}

TEST(BezierArcLengthTest, ArcLengthTangentsMatchDenseIndependentDerivativeReference)
{
    constexpr int referenceSubdivisions = 50000;
    const QPointF p0(0.0, 0.0);
    const QPointF p1(0.0, 80.0);
    const QPointF p2(100.0, -80.0);
    const QPointF p3(100.0, 0.0);
    const auto cubic = [&](double t) {
        const double u = 1.0 - t;
        return p0 * (u * u * u) + p1 * (3.0 * u * u * t) +
               p2 * (3.0 * u * t * t) + p3 * (t * t * t);
    };
    const auto derivative = [&](double t) {
        const double u = 1.0 - t;
        return (p1 - p0) * (3.0 * u * u) +
               (p2 - p1) * (6.0 * u * t) +
               (p3 - p2) * (3.0 * t * t);
    };

    std::vector<double> cumulativeLengths(referenceSubdivisions + 1, 0.0);
    QPointF previous = cubic(0.0);
    for (int index = 1; index <= referenceSubdivisions; ++index) {
        const QPointF current = cubic(
            static_cast<double>(index) / referenceSubdivisions);
        cumulativeLengths[static_cast<std::size_t>(index)] =
            cumulativeLengths[static_cast<std::size_t>(index - 1)] +
            gap(previous, current);
        previous = current;
    }
    const double totalLength = cumulativeLengths.back();
    const QVector<BezierPoint> curve = {
        BezierPoint{p0, QPointF(), p1 - p0},
        BezierPoint{p3, p2 - p3, QPointF()},
    };

    for (const double normalizedDistance : {0.08, 0.23, 0.51, 0.77, 0.94}) {
        const double targetLength = normalizedDistance * totalLength;
        const auto upper = std::lower_bound(cumulativeLengths.begin(),
                                            cumulativeLengths.end(), targetLength);
        const std::size_t upperIndex = static_cast<std::size_t>(
            upper - cumulativeLengths.begin());
        const std::size_t lowerIndex = upperIndex - 1;
        const double segmentLength = cumulativeLengths[upperIndex] -
                                     cumulativeLengths[lowerIndex];
        const double fraction = (targetLength - cumulativeLengths[lowerIndex]) /
                                segmentLength;
        const double t = (static_cast<double>(lowerIndex) + fraction) /
                         referenceSubdivisions;
        const QPointF referenceDerivative = derivative(t);
        const double derivativeLength = std::hypot(referenceDerivative.x(),
                                                   referenceDerivative.y());
        ASSERT_GT(derivativeLength, 0.0);
        const QPointF expected = referenceDerivative / derivativeLength;

        const QPointF actual = BezierPathSampler::tangentAtArcLength(
            curve, static_cast<float>(normalizedDistance), false);
        EXPECT_NEAR(actual.x(), expected.x(), 0.02)
            << "normalized arc length=" << normalizedDistance;
        EXPECT_NEAR(actual.y(), expected.y(), 0.02)
            << "normalized arc length=" << normalizedDistance;
        EXPECT_NEAR(std::hypot(actual.x(), actual.y()), 1.0, 1.0e-5);
    }
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
