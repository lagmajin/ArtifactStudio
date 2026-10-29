#include <gtest/gtest.h>

#include <QPointF>

#include <algorithm>
#include <vector>

import Core.Mask.RotoMask;

using namespace ArtifactCore;

TEST(RotoMaskContractTest, VertexIdsRemainUniqueAfterReverseAndSubsequentEdits)
{
    RotoMask mask;
    const auto first = mask.addVertex(QPointF(1.0, 2.0), QPointF(-1.0, 0.0),
                                      QPointF(1.0, 0.0));
    const auto second = mask.addVertex(QPointF(5.0, 2.0), QPointF(-1.0, 0.0),
                                       QPointF(1.0, 0.0));
    const auto third = mask.addVertex(QPointF(5.0, 6.0));

    mask.reverse();
    const auto reversedIds = mask.vertexIDs();
    ASSERT_EQ(reversedIds.size(), 3u);
    EXPECT_NE(reversedIds[0], third);
    EXPECT_NE(reversedIds[1], second);
    EXPECT_NE(reversedIds[2], first);
    EXPECT_EQ(mask.vertexPosition(reversedIds[0], 0.0), QPointF(5.0, 6.0));
    EXPECT_EQ(mask.vertexPosition(reversedIds[1], 0.0), QPointF(5.0, 2.0));
    EXPECT_EQ(mask.vertexPosition(reversedIds[2], 0.0), QPointF(1.0, 2.0));

    const auto addedAfterReverse = mask.addVertex(QPointF(9.0, 10.0));
    const auto idsAfterAdd = mask.vertexIDs();
    EXPECT_EQ(idsAfterAdd.size(), 4u);
    EXPECT_EQ(std::count(idsAfterAdd.begin(), idsAfterAdd.end(), addedAfterReverse), 1);
    EXPECT_EQ(mask.vertexPosition(first, 0.0), QPointF(1.0, 2.0));
    EXPECT_EQ(mask.vertexPosition(second, 0.0), QPointF(5.0, 2.0));
    EXPECT_EQ(mask.vertexPosition(third, 0.0), QPointF(5.0, 6.0));
}

TEST(RotoMaskContractTest, AnimationUsesSortedTimesAndClampsOutsideTheKeyframeRange)
{
    RotoMask mask;
    const auto vertex = mask.addVertex(QPointF(0.0, 0.0));
    mask.setVertexPosition(vertex, QPointF(20.0, 10.0), 10.0);
    mask.setVertexPosition(vertex, QPointF(0.0, 0.0), 0.0);
    mask.setPositionInterpolation(vertex, RotoMask::Interpolation::Linear);

    EXPECT_TRUE(mask.isAnimated());
    EXPECT_EQ(mask.startTime(), 0.0);
    EXPECT_EQ(mask.endTime(), 10.0);
    EXPECT_EQ(mask.keyframeTimes(), (std::vector<double>{0.0, 10.0}));
    EXPECT_EQ(mask.vertexPosition(vertex, -1.0), QPointF(0.0, 0.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 5.0), QPointF(10.0, 5.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 11.0), QPointF(20.0, 10.0));

    mask.removeKeyframe(10.0);
    EXPECT_FALSE(mask.isAnimated());
    EXPECT_EQ(mask.vertexPosition(vertex, 10.0), QPointF(0.0, 0.0));
}

TEST(RotoMaskContractTest, AnimatedAttributesClampAndInterpolateAtTheirOwnTimes)
{
    RotoMask mask;
    mask.setOpacity(-2.0f, 0.0);
    mask.setOpacity(2.0f, 10.0);
    mask.setFeather(-3.0f, 0.0);
    mask.setFeather(8.0f, 10.0);
    mask.setExpansion(-4.0f, 0.0);
    mask.setExpansion(6.0f, 10.0);

    EXPECT_FLOAT_EQ(mask.opacity(0.0), 0.0f);
    EXPECT_FLOAT_EQ(mask.opacity(5.0), 0.5f);
    EXPECT_FLOAT_EQ(mask.opacity(10.0), 1.0f);
    EXPECT_FLOAT_EQ(mask.feather(0.0), 0.0f);
    EXPECT_FLOAT_EQ(mask.feather(5.0), 4.0f);
    EXPECT_FLOAT_EQ(mask.expansion(5.0), 1.0f);

    mask.setMode(RotoMaskMode::Subtract);
    mask.setClosed(false);
    mask.setInverted(true);
    mask.setName(QStringLiteral("Mask A"));
    EXPECT_EQ(mask.mode(), RotoMaskMode::Subtract);
    EXPECT_FALSE(mask.isClosed());
    EXPECT_TRUE(mask.isInverted());
    EXPECT_EQ(mask.name(), QStringLiteral("Mask A"));
}

TEST(RotoMaskContractTest, CopyCloneAndShiftedKeyframeCopyAreIndependent)
{
    RotoMask source;
    const auto sourceVertex = source.addVertex(QPointF(2.0, 3.0),
                                               QPointF(-1.0, 0.0),
                                               QPointF(1.0, 0.0));
    source.setVertexPosition(sourceVertex, QPointF(12.0, 13.0), 10.0);
    source.setOpacity(0.25f, 0.0);
    source.setOpacity(0.75f, 10.0);
    source.setName(QStringLiteral("Source"));
    source.setMode(RotoMaskMode::Difference);

    RotoMask copied = source;
    RotoMask cloned = source.clone();
    RotoMask shifted;
    shifted.copyKeyframesFrom(source, 5.0);

    const auto copiedVertex = copied.vertexIDs().front();
    const auto clonedVertex = cloned.vertexIDs().front();
    const auto shiftedVertex = shifted.vertexIDs().front();
    EXPECT_EQ(copied.vertexPosition(copiedVertex, 5.0), QPointF(7.0, 8.0));
    EXPECT_EQ(cloned.vertexPosition(clonedVertex, 5.0), QPointF(7.0, 8.0));
    EXPECT_EQ(shifted.vertexPosition(shiftedVertex, 10.0), QPointF(7.0, 8.0));
    EXPECT_EQ(shifted.startTime(), 5.0);
    EXPECT_EQ(shifted.endTime(), 15.0);
    EXPECT_EQ(shifted.name(), QStringLiteral("Source"));
    EXPECT_EQ(shifted.mode(), RotoMaskMode::Difference);

    copied.setVertexPosition(copiedVertex, QPointF(99.0, 99.0), 5.0);
    EXPECT_EQ(source.vertexPosition(sourceVertex, 5.0), QPointF(7.0, 8.0));
    EXPECT_EQ(cloned.vertexPosition(clonedVertex, 5.0), QPointF(7.0, 8.0));
}
