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
