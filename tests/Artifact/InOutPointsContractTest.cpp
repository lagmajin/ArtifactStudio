#include <gtest/gtest.h>

import Artifact.Composition.InOutPoints;
import Frame.Position;
import Frame.Range;

TEST(InOutPointsContractTest, EffectiveRangeUsesCompositionBoundsWhenPointsAreUnset)
{
    Artifact::ArtifactInOutPoints points;
    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(-10),
                                    ArtifactCore::FramePosition(120)),
              ArtifactCore::FrameRange(-10, 120));
    EXPECT_FALSE(points.isRangeLimited());
}

TEST(InOutPointsContractTest, EffectiveRangeCombinesBothPointsAndCompositionBounds)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(15));
    points.setOutPoint(ArtifactCore::FramePosition(90));

    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                    ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(15, 90));
    EXPECT_TRUE(points.isRangeLimited());
}

TEST(InOutPointsContractTest, ClearingOnePointRestoresOnlyThatCompositionBoundary)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(15));
    points.setOutPoint(ArtifactCore::FramePosition(90));
    points.clearInPoint();

    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                    ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(0, 90));
    EXPECT_TRUE(points.isRangeLimited());

    points.clearOutPoint();
    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                    ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(0, 100));
    EXPECT_FALSE(points.isRangeLimited());
}

TEST(InOutPointsContractTest, ReversedPointsAreOrderedByEffectiveRange)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(90));
    points.setOutPoint(ArtifactCore::FramePosition(15));

    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                    ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(15, 90));
}

TEST(InOutPointsContractTest, ExplicitPointsOutsideCompositionRemainExplicit)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(-20));
    points.setOutPoint(ArtifactCore::FramePosition(140));

    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                    ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(-20, 140));
}
