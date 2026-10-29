#include <gtest/gtest.h>

#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <vector>

import Shape.AeOperators;

namespace {

using ArtifactCore::MergePaths;
using ArtifactCore::OffsetPaths;
using ArtifactCore::RoundedCorners;
using ArtifactCore::ShapePath;

auto overlappingRectangles() -> std::vector<ShapePath>
{
    ShapePath left;
    left.setRectangle(0.0, 0.0, 10.0, 10.0);
    ShapePath right;
    right.setRectangle(5.0, 0.0, 10.0, 10.0);
    return {left, right};
}

} // namespace

TEST(AeShapeOperatorsContractTest, MergeModesRespectInputOrderAndOverlap)
{
    const auto inputs = overlappingRectangles();

    MergePaths add;
    const auto unionResult = add.process(inputs);
    ASSERT_EQ(unionResult.size(), 1u);
    EXPECT_TRUE(unionResult.front().contains(QPointF(2.0, 5.0)));
    EXPECT_TRUE(unionResult.front().contains(QPointF(7.0, 5.0)));
    EXPECT_TRUE(unionResult.front().contains(QPointF(13.0, 5.0)));
    EXPECT_FALSE(unionResult.front().contains(QPointF(16.0, 5.0)));

    MergePaths subtract;
    subtract.setMode(MergePaths::Subtract);
    const auto subtractionResult = subtract.process(inputs);
    ASSERT_EQ(subtractionResult.size(), 1u);
    EXPECT_TRUE(subtractionResult.front().contains(QPointF(2.0, 5.0)));
    EXPECT_FALSE(subtractionResult.front().contains(QPointF(7.0, 5.0)));
    EXPECT_FALSE(subtractionResult.front().contains(QPointF(13.0, 5.0)));

    MergePaths intersect;
    intersect.setMode(MergePaths::Intersect);
    const auto intersectionResult = intersect.process(inputs);
    ASSERT_EQ(intersectionResult.size(), 1u);
    EXPECT_FALSE(intersectionResult.front().contains(QPointF(2.0, 5.0)));
    EXPECT_TRUE(intersectionResult.front().contains(QPointF(7.0, 5.0)));
    EXPECT_FALSE(intersectionResult.front().contains(QPointF(13.0, 5.0)));

    MergePaths difference;
    difference.setMode(MergePaths::Difference);
    const auto differenceResult = difference.process(inputs);
    ASSERT_EQ(differenceResult.size(), 1u);
    EXPECT_TRUE(differenceResult.front().contains(QPointF(2.0, 5.0)));
    EXPECT_FALSE(differenceResult.front().contains(QPointF(7.0, 5.0)));
    EXPECT_TRUE(differenceResult.front().contains(QPointF(13.0, 5.0)));
}

TEST(AeShapeOperatorsContractTest, MergeModeAndSerializationKeepTheSelectedMode)
{
    MergePaths merge;
    merge.setMode(-1);
    EXPECT_EQ(merge.mode(), MergePaths::Add);
    merge.setMode(MergePaths::Merge);

    const QJsonObject saved = merge.toJson();
    EXPECT_EQ(saved.value(QStringLiteral("mode")).toInt(), MergePaths::Merge);
    EXPECT_TRUE(merge.process({}).empty());

    auto clone = merge.clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->type(), ArtifactCore::ShapeOperatorType::MergePaths);
    EXPECT_EQ(clone->toJson().value(QStringLiteral("mode")).toInt(),
              MergePaths::Merge);

    MergePaths restored;
    restored.fromJson(saved);
    EXPECT_EQ(restored.mode(), MergePaths::Merge);
}

TEST(AeShapeOperatorsContractTest, OffsetExpandsAndContractsClosedContours)
{
    ShapePath rectangle;
    rectangle.setRectangle(0.0, 0.0, 10.0, 10.0);
    const std::vector<ShapePath> input{rectangle};

    OffsetPaths expand;
    expand.setOffset(2.0f);
    const auto expanded = expand.process(input);
    ASSERT_EQ(expanded.size(), 1u);
    EXPECT_TRUE(expanded.front().contains(QPointF(-1.0, 5.0)));
    EXPECT_TRUE(expanded.front().contains(QPointF(5.0, 5.0)));
    EXPECT_FALSE(expanded.front().contains(QPointF(-3.0, 5.0)));

    OffsetPaths contract;
    contract.setOffset(-2.0f);
    const auto contracted = contract.process(input);
    ASSERT_EQ(contracted.size(), 1u);
    EXPECT_TRUE(contracted.front().contains(QPointF(5.0, 5.0)));
    EXPECT_FALSE(contracted.front().contains(QPointF(1.0, 5.0)));
}

TEST(AeShapeOperatorsContractTest, RoundedCornersPreserveRadiusAndIgnoreEmptyInput)
{
    RoundedCorners rounded;
    rounded.setRadius(4.0f);

    const QJsonObject saved = rounded.toJson();
    EXPECT_DOUBLE_EQ(saved.value(QStringLiteral("radius")).toDouble(), 4.0);
    EXPECT_TRUE(rounded.process({}).empty());

    auto clone = rounded.clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_DOUBLE_EQ(clone->toJson().value(QStringLiteral("radius")).toDouble(), 4.0);
}
