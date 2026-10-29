#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QPointF>
#include <QJsonObject>

#include <vector>

import Artifact.Layer.Shape;

namespace {

QCoreApplication* ensureApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactShapeLayerAnimationPersistenceContractTest";
    static char* argv[] = {appName, nullptr};
    static QCoreApplication app(argc, argv);
    return &app;
}

std::vector<Artifact::CustomPathVertex> pathAt(double x, double y)
{
    return {
        {QPointF(x, y), QPointF(0.0, -x), QPointF(4.0 + x, 2.0), x > 0.0},
        {QPointF(x + 20.0, y), QPointF(-4.0 - x, 0.0),
         QPointF(0.0, y), false},
    };
}

} // namespace

TEST(ShapeLayerAnimationPersistenceContractTest,
     PathKeyframesSurviveLayerJsonRoundTripAndEvaluateAtRequestedFrames)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer source;
    source.setPathKeyframe(0, pathAt(0.0, 0.0));
    source.setPathKeyframe(10, pathAt(20.0, 10.0));

    const auto restored = Artifact::ArtifactShapeLayer::fromJson(source.toJson());
    ASSERT_TRUE(restored);
    ASSERT_TRUE(restored->hasPathKeyframes());

    const auto first = restored->evaluatePathAt(0);
    const auto before = restored->evaluatePathAt(-1);
    const auto quarter = restored->evaluatePathAt(2);
    const auto midpoint = restored->evaluatePathAt(5);
    const auto after = restored->evaluatePathAt(11);
    const auto last = restored->evaluatePathAt(10);
    ASSERT_EQ(first.size(), 2u);
    ASSERT_EQ(before.size(), 2u);
    ASSERT_EQ(quarter.size(), 2u);
    ASSERT_EQ(midpoint.size(), 2u);
    ASSERT_EQ(after.size(), 2u);
    ASSERT_EQ(last.size(), 2u);

    EXPECT_DOUBLE_EQ(first.front().pos.x(), 0.0);
    EXPECT_DOUBLE_EQ(first.front().pos.y(), 0.0);
    EXPECT_DOUBLE_EQ(before.front().pos.x(), 0.0);
    EXPECT_DOUBLE_EQ(before.front().pos.y(), 0.0);
    EXPECT_DOUBLE_EQ(quarter.front().pos.x(), 4.0);
    EXPECT_DOUBLE_EQ(quarter.front().pos.y(), 2.0);
    EXPECT_DOUBLE_EQ(quarter.front().inTangent.y(), -4.0);
    EXPECT_DOUBLE_EQ(quarter.front().outTangent.x(), 8.0);
    EXPECT_FALSE(quarter.front().smooth);
    EXPECT_DOUBLE_EQ(midpoint.front().pos.x(), 10.0);
    EXPECT_DOUBLE_EQ(midpoint.front().pos.y(), 5.0);
    EXPECT_DOUBLE_EQ(midpoint.front().inTangent.y(), -10.0);
    EXPECT_DOUBLE_EQ(midpoint.front().outTangent.x(), 14.0);
    EXPECT_TRUE(midpoint.front().smooth);
    EXPECT_DOUBLE_EQ(midpoint.back().inTangent.x(), -14.0);
    EXPECT_DOUBLE_EQ(midpoint.back().outTangent.y(), 5.0);
    EXPECT_DOUBLE_EQ(last.front().pos.x(), 20.0);
    EXPECT_DOUBLE_EQ(last.front().pos.y(), 10.0);
    EXPECT_DOUBLE_EQ(after.front().pos.x(), 20.0);
    EXPECT_DOUBLE_EQ(after.front().pos.y(), 10.0);
}
