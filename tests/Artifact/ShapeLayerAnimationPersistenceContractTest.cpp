#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QPointF>
#include <QJsonObject>

#include <cmath>
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

TEST(ShapeLayerAnimationPersistenceContractTest,
     SparseNegativeFramesHoldEdgesAndReplacingAFrameDoesNotMutateOtherKeys)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer layer;
    auto firstKey = pathAt(-8.0, -4.0);
    const auto originalLastKey = pathAt(12.0, 16.0);
    layer.setPathKeyframe(-10, firstKey);
    layer.setPathKeyframe(10, originalLastKey);

    // Keyframe values are snapshots; editing the caller's vector afterward
    // must not mutate the serialized key.
    firstKey.front().pos = QPointF(999.0, 999.0);

    // Reusing a frame replaces only that frame's geometry.
    layer.setPathKeyframe(10, pathAt(20.0, 30.0));

    const auto before = layer.evaluatePathAt(-11);
    const auto first = layer.evaluatePathAt(-10);
    const auto middle = layer.evaluatePathAt(0);
    const auto last = layer.evaluatePathAt(10);
    const auto after = layer.evaluatePathAt(11);
    ASSERT_EQ(before.size(), 2u);
    ASSERT_EQ(first.size(), 2u);
    ASSERT_EQ(middle.size(), 2u);
    ASSERT_EQ(last.size(), 2u);
    ASSERT_EQ(after.size(), 2u);

    EXPECT_DOUBLE_EQ(before.front().pos.x(), -8.0);
    EXPECT_DOUBLE_EQ(before.front().pos.y(), -4.0);
    EXPECT_DOUBLE_EQ(first.front().pos.x(), -8.0);
    EXPECT_DOUBLE_EQ(first.front().pos.y(), -4.0);
    EXPECT_DOUBLE_EQ(middle.front().pos.x(), 6.0);
    EXPECT_DOUBLE_EQ(middle.front().pos.y(), 13.0);
    EXPECT_DOUBLE_EQ(last.front().pos.x(), 20.0);
    EXPECT_DOUBLE_EQ(last.front().pos.y(), 30.0);
    EXPECT_DOUBLE_EQ(after.front().pos.x(), 20.0);
    EXPECT_DOUBLE_EQ(after.front().pos.y(), 30.0);
}

TEST(ShapeLayerAnimationPersistenceContractTest,
     TopologyChangesMorphBetweenKeysAndSurviveJsonRoundTrip)
{
    ensureApplication();
    Artifact::ArtifactShapeLayer source;
    const std::vector<Artifact::CustomPathVertex> triangle{
        {QPointF(-10.0, -8.0), QPointF(), QPointF(), false},
        {QPointF(10.0, -8.0), QPointF(), QPointF(), false},
        {QPointF(0.0, 12.0), QPointF(), QPointF(), false},
    };
    const std::vector<Artifact::CustomPathVertex> pentagon{
        {QPointF(0.0, -14.0), QPointF(), QPointF(), false},
        {QPointF(13.0, -4.0), QPointF(), QPointF(), false},
        {QPointF(8.0, 11.0), QPointF(), QPointF(), false},
        {QPointF(-8.0, 11.0), QPointF(), QPointF(), false},
        {QPointF(-13.0, -4.0), QPointF(), QPointF(), false},
    };
    source.setCustomPathVertices(triangle, true);
    source.setPathKeyframe(0, triangle);
    source.setPathKeyframe(12, pentagon);

    const auto restored = Artifact::ArtifactShapeLayer::fromJson(source.toJson());
    ASSERT_TRUE(restored);

    const auto first = restored->evaluatePathAt(0);
    const auto middle = restored->evaluatePathAt(6);
    const auto last = restored->evaluatePathAt(12);
    ASSERT_EQ(first.size(), triangle.size());
    ASSERT_EQ(last.size(), pentagon.size());
    ASSERT_EQ(middle.size(), 64u);

    bool movedFromFirst = false;
    bool movedFromLast = false;
    for (const auto& vertex : middle) {
        EXPECT_TRUE(std::isfinite(vertex.pos.x()));
        EXPECT_TRUE(std::isfinite(vertex.pos.y()));
        movedFromFirst = movedFromFirst || vertex.pos != triangle.front().pos;
        movedFromLast = movedFromLast || vertex.pos != pentagon.front().pos;
    }
    EXPECT_TRUE(movedFromFirst);
    EXPECT_TRUE(movedFromLast);

    const auto restoredAgain = Artifact::ArtifactShapeLayer::fromJson(
        restored->toJson());
    ASSERT_TRUE(restoredAgain);
    const auto middleAfterSecondRoundTrip = restoredAgain->evaluatePathAt(6);
    ASSERT_EQ(middleAfterSecondRoundTrip.size(), middle.size());
    for (size_t i = 0; i < middle.size(); ++i) {
        EXPECT_DOUBLE_EQ(middleAfterSecondRoundTrip[i].pos.x(),
                         middle[i].pos.x());
        EXPECT_DOUBLE_EQ(middleAfterSecondRoundTrip[i].pos.y(),
                         middle[i].pos.y());
    }
}
