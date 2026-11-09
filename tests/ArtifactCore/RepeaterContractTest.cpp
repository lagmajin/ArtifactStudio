#include <gtest/gtest.h>

#include <cmath>

#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <vector>

import Shape.Repeater;

namespace {

using ArtifactCore::Repeater;
using ArtifactCore::ShapePath;

auto rectanglePath() -> ShapePath
{
    ShapePath path;
    path.setRectangle(0.0, 0.0, 10.0, 10.0);
    return path;
}

void expectHorizontalExtent(const ShapePath& path, double left, double right)
{
    EXPECT_DOUBLE_EQ(path.boundingRect().left(), left);
    EXPECT_DOUBLE_EQ(path.boundingRect().right(), right);
}

} // namespace

TEST(RepeaterContractTest, CreatesExpectedCopiesWithInterpolatedOpacity)
{
    const std::vector<ShapePath> input{rectanglePath()};
    Repeater repeater;
    repeater.setCopies(3);
    repeater.setPosition(QPointF(10.0, 0.0));
    repeater.setStartOpacity(1.0f);
    repeater.setEndOpacity(0.25f);

    const auto copies = repeater.process(input);
    ASSERT_EQ(copies.size(), 3u);
    expectHorizontalExtent(copies[0], 0.0, 10.0);
    expectHorizontalExtent(copies[1], 10.0, 20.0);
    expectHorizontalExtent(copies[2], 20.0, 30.0);
    EXPECT_DOUBLE_EQ(copies[0].opacity(), 1.0);
    EXPECT_DOUBLE_EQ(copies[1].opacity(), 0.625);
    EXPECT_DOUBLE_EQ(copies[2].opacity(), 0.25);
    expectHorizontalExtent(input.front(), 0.0, 10.0);
    EXPECT_DOUBLE_EQ(input.front().opacity(), 1.0);
}

TEST(RepeaterContractTest, AppliesScalePerCopyAndReversesCompositeOrderWhenRequested)
{
    const std::vector<ShapePath> input{rectanglePath()};
    Repeater repeater;
    repeater.setCopies(3);
    repeater.setPosition(QPointF(0.0, 0.0));
    repeater.setScale(QPointF(2.0, 2.0));

    const auto frontComposite = repeater.process(input);
    ASSERT_EQ(frontComposite.size(), 3u);
    expectHorizontalExtent(frontComposite[0], 0.0, 10.0);
    expectHorizontalExtent(frontComposite[1], 0.0, 20.0);
    expectHorizontalExtent(frontComposite[2], 0.0, 40.0);

    repeater.setCompositeBelow(true);
    const auto belowComposite = repeater.process(input);
    ASSERT_EQ(belowComposite.size(), 3u);
    expectHorizontalExtent(belowComposite[0], 0.0, 40.0);
    expectHorizontalExtent(belowComposite[1], 0.0, 20.0);
    expectHorizontalExtent(belowComposite[2], 0.0, 10.0);
}

TEST(RepeaterContractTest,
     RotationAccumulatesPerCopyAroundTheConfiguredAnchor)
{
    ShapePath inputPath;
    inputPath.setRectangle(0.0, 0.0, 10.0, 4.0);
    const std::vector<ShapePath> input{inputPath};
    Repeater repeater;
    repeater.setCopies(3);
    repeater.setAnchorPoint(QPointF(2.0, 1.0));
    repeater.setPosition(QPointF(0.0, 0.0));
    repeater.setScale(QPointF(1.0, 1.0));
    repeater.setRotation(90.0f);

    const auto copies = repeater.process(input);
    ASSERT_EQ(copies.size(), 3u);

    const auto firstBounds = copies[0].boundingRect();
    EXPECT_DOUBLE_EQ(firstBounds.left(), 0.0);
    EXPECT_DOUBLE_EQ(firstBounds.right(), 10.0);
    EXPECT_DOUBLE_EQ(firstBounds.top(), 0.0);
    EXPECT_DOUBLE_EQ(firstBounds.bottom(), 4.0);

    const auto secondBounds = copies[1].boundingRect();
    EXPECT_NEAR(secondBounds.left(), -1.0, 1e-12);
    EXPECT_NEAR(secondBounds.right(), 3.0, 1e-12);
    EXPECT_NEAR(secondBounds.top(), -1.0, 1e-12);
    EXPECT_NEAR(secondBounds.bottom(), 9.0, 1e-12);

    const auto thirdBounds = copies[2].boundingRect();
    EXPECT_NEAR(thirdBounds.left(), -6.0, 1e-12);
    EXPECT_NEAR(thirdBounds.right(), 4.0, 1e-12);
    EXPECT_NEAR(thirdBounds.top(), -2.0, 1e-12);
    EXPECT_NEAR(thirdBounds.bottom(), 2.0, 1e-12);
    EXPECT_EQ(input.front().boundingRect(), inputPath.boundingRect());
}

TEST(RepeaterContractTest, ZeroCopiesAndEmptyInputReturnWithoutCreatingGeometry)
{
    const std::vector<ShapePath> input{rectanglePath()};
    Repeater repeater;
    repeater.setCopies(0);
    const auto zeroCopies = repeater.process(input);
    ASSERT_EQ(zeroCopies.size(), 1u);
    expectHorizontalExtent(zeroCopies.front(), 0.0, 10.0);
    EXPECT_TRUE(repeater.process({}).empty());
}

TEST(RepeaterContractTest, SerializationCloneAndRestorationPreserveSettings)
{
    Repeater repeater;
    repeater.setCopies(5);
    repeater.setOffset(0.5f);
    repeater.setAnchorPoint(QPointF(2.0, 3.0));
    repeater.setPosition(QPointF(4.0, -6.0));
    repeater.setScale(QPointF(1.5, 0.75));
    repeater.setRotation(30.0f);
    repeater.setStartOpacity(0.2f);
    repeater.setEndOpacity(0.8f);
    repeater.setCompositeBelow(true);

    const QJsonObject saved = repeater.toJson();
    Repeater restored;
    restored.fromJson(saved);
    EXPECT_EQ(restored.toJson(), saved);

    auto clone = repeater.clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->type(), ArtifactCore::ShapeOperatorType::Repeater);
    EXPECT_EQ(clone->toJson(), saved);

    restored.fromJson(QJsonObject{
        {QStringLiteral("copies"), 10001},
        {QStringLiteral("startOpacity"), -2.0},
        {QStringLiteral("endOpacity"), 2.0},
    });
    EXPECT_EQ(restored.copies(), 10000);
    EXPECT_FLOAT_EQ(restored.startOpacity(), 0.0f);
    EXPECT_FLOAT_EQ(restored.endOpacity(), 1.0f);
}

TEST(RepeaterContractTest,
     FiniteJsonValuesOutsideFloatRangePreserveUsableTransforms)
{
    Repeater repeater;
    repeater.setCopies(2);
    repeater.setOffset(0.25f);
    repeater.setRotation(15.0f);
    repeater.setPosition(QPointF(3.0, 0.0));

    repeater.fromJson(QJsonObject{
        {QStringLiteral("offset"), 1.0e300},
        {QStringLiteral("rotation"), -1.0e300},
    });

    EXPECT_FLOAT_EQ(repeater.offset(), 0.25f);
    EXPECT_FLOAT_EQ(repeater.rotation(), 15.0f);

    const auto copies = repeater.process(std::vector<ShapePath>{rectanglePath()});
    ASSERT_EQ(copies.size(), 2u);
    for (const auto& copy : copies) {
        const auto bounds = copy.boundingRect();
        EXPECT_TRUE(std::isfinite(bounds.left()));
        EXPECT_TRUE(std::isfinite(bounds.right()));
        EXPECT_TRUE(std::isfinite(bounds.top()));
        EXPECT_TRUE(std::isfinite(bounds.bottom()));
    }
}
