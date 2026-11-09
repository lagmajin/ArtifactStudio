#include <gtest/gtest.h>

#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <cmath>
#include <limits>
#include <vector>

import Shape.TrimPaths;

namespace {

using ArtifactCore::ShapePath;
using ArtifactCore::TrimMode;
using ArtifactCore::TrimPaths;

auto linePath(double start, double end) -> ShapePath
{
    ShapePath path;
    path.moveTo(start, 0.0);
    path.lineTo(end, 0.0);
    return path;
}

void expectHorizontalExtent(const ShapePath& path, double left, double right)
{
    EXPECT_DOUBLE_EQ(path.boundingRect().left(), left);
    EXPECT_DOUBLE_EQ(path.boundingRect().right(), right);
}

void expectPointNear(const QPointF& actual, const QPointF& expected,
                     double tolerance = 1.0e-9)
{
    EXPECT_NEAR(actual.x(), expected.x(), tolerance);
    EXPECT_NEAR(actual.y(), expected.y(), tolerance);
}

} // namespace

TEST(TrimPathsContractTest, TrimsLineByPercentAndWrapsAcrossThePathOrigin)
{
    const std::vector<ShapePath> input{linePath(0.0, 100.0)};
    TrimPaths trim;
    trim.setStart(25.0f);
    trim.setEnd(75.0f);

    const auto middle = trim.process(input);
    ASSERT_EQ(middle.size(), 1u);
    expectHorizontalExtent(middle.front(), 25.0, 75.0);
    EXPECT_FALSE(middle.front().isClosed());

    trim.setStart(75.0f);
    trim.setEnd(25.0f);
    const auto wrapped = trim.process(input);
    ASSERT_EQ(wrapped.size(), 2u);
    expectHorizontalExtent(wrapped[0], 75.0, 100.0);
    expectHorizontalExtent(wrapped[1], 0.0, 25.0);

    trim.setStart(0.0f);
    trim.setEnd(100.0f);
    const auto full = trim.process(input);
    ASSERT_EQ(full.size(), 1u);
    expectHorizontalExtent(full.front(), 0.0, 100.0);
}

TEST(TrimPathsContractTest, OffsetShiftsTheTrimWindowAndModesChoosePathScope)
{
    const std::vector<ShapePath> input{
        linePath(0.0, 100.0), linePath(200.0, 300.0)};
    TrimPaths trim;
    trim.setStart(0.0f);
    trim.setEnd(25.0f);
    trim.setOffset(90.0f);
    const auto shifted = trim.process({input.front()});
    ASSERT_EQ(shifted.size(), 1u);
    expectHorizontalExtent(shifted.front(), 25.0, 50.0);

    trim.setOffset(0.0f);
    trim.setStart(25.0f);
    trim.setEnd(75.0f);
    trim.setTrimMode(TrimMode::Individually);
    const auto individual = trim.process(input);
    ASSERT_EQ(individual.size(), 2u);
    expectHorizontalExtent(individual[0], 25.0, 75.0);
    expectHorizontalExtent(individual[1], 225.0, 275.0);

    trim.setTrimMode(TrimMode::Simultaneously);
    const auto simultaneous = trim.process(input);
    ASSERT_EQ(simultaneous.size(), 2u);
    expectHorizontalExtent(simultaneous[0], 50.0, 100.0);
    expectHorizontalExtent(simultaneous[1], 200.0, 250.0);
}

TEST(TrimPathsContractTest, SimultaneousTrimUsesUnequalPathLengthsAndSkipsEmptyPaths)
{
    const std::vector<ShapePath> input{
        linePath(0.0, 100.0), ShapePath{}, linePath(200.0, 500.0)};
    TrimPaths trim;
    trim.setTrimMode(TrimMode::Simultaneously);
    trim.setStart(12.5f);
    trim.setEnd(62.5f);

    const auto result = trim.process(input);

    ASSERT_EQ(result.size(), 2u);
    expectHorizontalExtent(result[0], 50.0, 100.0);
    expectHorizontalExtent(result[1], 200.0, 350.0);
    EXPECT_FALSE(result[0].isClosed());
    EXPECT_FALSE(result[1].isClosed());
}

TEST(TrimPathsContractTest,
     SimultaneousTrimStartingAtPathBoundarySkipsEmptyAndZeroLengthPaths)
{
    ShapePath zeroLength;
    zeroLength.moveTo(100.0, 0.0);
    zeroLength.lineTo(100.0, 0.0);
    const std::vector<ShapePath> input{
        linePath(0.0, 10.0), ShapePath{}, zeroLength, linePath(200.0, 230.0)};
    TrimPaths trim;
    trim.setTrimMode(TrimMode::Simultaneously);
    trim.setStart(25.0f);
    trim.setEnd(50.0f);

    const auto result = trim.process(input);

    // The positive path lengths total 40. The exact 25% start is the join
    // after the first 10-unit path, so only [200, 210] from the last path
    // belongs to the requested [25%, 50%] interval.
    ASSERT_EQ(result.size(), 1u);
    expectHorizontalExtent(result.front(), 200.0, 210.0);
    EXPECT_FALSE(result.front().isClosed());
}

TEST(TrimPathsContractTest, CubicTrimKeepsCurveAndUsesTheArcLengthBoundaryPoint)
{
    ShapePath path;
    path.moveTo(0.0, 0.0);
    path.cubicTo(0.0, 100.0, 100.0, 100.0, 100.0, 0.0);
    TrimPaths trim;
    trim.setStart(0.0f);
    trim.setEnd(50.0f);

    const auto result = trim.process({path});

    ASSERT_EQ(result.size(), 1u);
    const auto segments = result.front().toSegments();
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_DOUBLE_EQ(segments.front().p0.x(), 0.0);
    EXPECT_DOUBLE_EQ(segments.front().p0.y(), 0.0);
    EXPECT_NEAR(segments.front().p1.x(), 50.0, 1.0e-6);
    EXPECT_NEAR(segments.front().p1.y(), 75.0, 1.0e-6);
    EXPECT_NE(segments.front().cp1, segments.front().p0);
    EXPECT_NE(segments.front().cp2, segments.front().p1);
    EXPECT_FALSE(result.front().isClosed());
}

TEST(TrimPathsContractTest, PartialTrimPreservesGapsBetweenSubpaths)
{
    ShapePath path;
    path.moveTo(0.0, 0.0);
    path.lineTo(10.0, 0.0);
    path.moveTo(100.0, 0.0);
    path.lineTo(110.0, 0.0);
    TrimPaths trim;
    trim.setStart(25.0f);
    trim.setEnd(75.0f);

    const auto result = trim.process({path});

    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result.front().length(), 10.0)
        << "the 90-unit move between subpaths is not part of the trimmed geometry";
    const auto segments = result.front().toSegments();
    ASSERT_EQ(segments.size(), 2u);
    expectPointNear(segments[0].p0, QPointF(5.0, 0.0));
    expectPointNear(segments[0].p1, QPointF(10.0, 0.0));
    expectPointNear(segments[1].p0, QPointF(100.0, 0.0));
    expectPointNear(segments[1].p1, QPointF(105.0, 0.0));
}

TEST(ShapePathContractTest, LengthSamplingSkipsSubpathGapsAndClampsEndpoints)
{
    ShapePath path;
    path.moveTo(0.0, 0.0);
    path.lineTo(3.0, 4.0);
    path.moveTo(100.0, 0.0);
    path.lineTo(100.0, 12.0);

    EXPECT_DOUBLE_EQ(path.length(), 17.0);
    expectPointNear(path.pointAtLength(-5.0), QPointF(0.0, 0.0));
    expectPointNear(path.pointAtLength(2.5), QPointF(1.5, 2.0));
    expectPointNear(path.pointAtLength(5.0), QPointF(3.0, 4.0));
    expectPointNear(path.pointAtLength(11.0), QPointF(100.0, 6.0));
    expectPointNear(path.pointAtLength(100.0), QPointF(100.0, 12.0));
    expectPointNear(path.pointAtPercent(0.5), QPointF(100.0, 3.5));
    expectPointNear(path.pointAtPercent(2.0), QPointF(100.0, 12.0));
}

TEST(ShapePathContractTest, CubicPercentSamplingTracksAsymmetricArcLengthMidpoint)
{
    ShapePath path;
    path.moveTo(0.0, 0.0);
    path.cubicTo(0.0, 160.0, 15.0, -20.0, 100.0, 0.0);

    // Independent high-precision integration of the cubic derivative places
    // the half-length point near (22.789982, 45.251455).
    expectPointNear(path.pointAtPercent(0.5), QPointF(22.789982, 45.251455), 0.5);
}

TEST(TrimPathsContractTest, AsymmetricCubicHalfTrimUsesTheArcLengthMidpoint)
{
    ShapePath path;
    path.moveTo(0.0, 0.0);
    path.cubicTo(0.0, 160.0, 15.0, -20.0, 100.0, 0.0);
    TrimPaths trim;
    trim.setStart(0.0f);
    trim.setEnd(50.0f);

    const auto result = trim.process({path});

    ASSERT_EQ(result.size(), 1u);
    const auto segments = result.front().toSegments();
    ASSERT_EQ(segments.size(), 1u);
    // Independent high-precision integration places half the curve length at
    // t ~= 0.55027 for this asymmetric cubic, rather than at t = 0.5.
    EXPECT_NEAR(segments.front().p1.x(), 22.789982, 0.02);
    EXPECT_NEAR(segments.front().p1.y(), 45.251455, 0.02);
}

TEST(TrimPathsContractTest, CubicTrimUsesArcLengthRatherThanBezierParameterPercent)
{
    ShapePath path;
    path.moveTo(0.0, 0.0);
    path.cubicTo(0.0, 0.0, 0.0, 0.0, 100.0, 0.0);
    TrimPaths trim;
    trim.setStart(10.0f);
    trim.setEnd(90.0f);

    const auto result = trim.process({path});

    ASSERT_EQ(result.size(), 1u);
    const auto segments = result.front().toSegments();
    ASSERT_EQ(segments.size(), 1u);

    // This cubic is x(t) = 100t^3, so its arc-length fractions 10% and 90%
    // occur at cube-root parameters. Derive the sliced controls independently
    // from the cubic derivative and require the geometric endpoints to match
    // the requested arc-length positions.
    const double t0 = std::cbrt(0.1);
    const double t1 = std::cbrt(0.9);
    const double expectedCp1 = 100.0 * t0 * t0 * t1;
    const double expectedCp2 = 100.0 * t0 * t1 * t1;
    EXPECT_NEAR(segments.front().p0.x(), 10.0, 1.0e-6);
    EXPECT_NEAR(segments.front().cp1.x(), expectedCp1, 1.0e-6);
    EXPECT_NEAR(segments.front().cp2.x(), expectedCp2, 1.0e-6);
    EXPECT_NEAR(segments.front().p1.x(), 90.0, 1.0e-6);
    EXPECT_DOUBLE_EQ(segments.front().p0.y(), 0.0);
    EXPECT_DOUBLE_EQ(segments.front().cp1.y(), 0.0);
    EXPECT_DOUBLE_EQ(segments.front().cp2.y(), 0.0);
    EXPECT_DOUBLE_EQ(segments.front().p1.y(), 0.0);
}

TEST(TrimPathsContractTest, ClampsInvalidParametersAndRoundTripsOperatorState)
{
    TrimPaths trim;
    trim.setStart(-10.0f);
    trim.setEnd(120.0f);
    trim.setOffset(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(trim.start(), 0.0f);
    EXPECT_FLOAT_EQ(trim.end(), 100.0f);
    EXPECT_FLOAT_EQ(trim.offset(), 0.0f);

    trim.setStart(std::numeric_limits<float>::quiet_NaN());
    trim.setEnd(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(trim.start(), 0.0f);
    EXPECT_FLOAT_EQ(trim.end(), 100.0f);
    trim.setTrimMode(static_cast<TrimMode>(99));
    EXPECT_EQ(trim.trimMode(), TrimMode::Individually);

    const QJsonObject saved{
        {QStringLiteral("start"), 20.0},
        {QStringLiteral("end"), 80.0},
        {QStringLiteral("offset"), 45.0},
        {QStringLiteral("trimMode"), static_cast<int>(TrimMode::Simultaneously)},
    };
    trim.fromJson(saved);
    EXPECT_FLOAT_EQ(trim.start(), 20.0f);
    EXPECT_FLOAT_EQ(trim.end(), 80.0f);
    EXPECT_FLOAT_EQ(trim.offset(), 45.0f);
    EXPECT_EQ(trim.trimMode(), TrimMode::Simultaneously);
    EXPECT_TRUE(trim.process({}).empty());

    auto clone = trim.clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->type(), ArtifactCore::ShapeOperatorType::TrimPaths);
    EXPECT_EQ(clone->toJson(), trim.toJson());
}
