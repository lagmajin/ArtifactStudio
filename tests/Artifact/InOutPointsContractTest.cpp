#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>

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

TEST(InOutPointsContractTest, MarkerNavigationAndChapterQueriesUseStrictOrderedPositions)
{
    Artifact::ArtifactInOutPoints points;
    auto* first = points.addMarker(ArtifactCore::FramePosition(10),
                                   QStringLiteral("Opening"),
                                   Artifact::MarkerType::Chapter);
    auto* middle = points.addMarker(ArtifactCore::FramePosition(20),
                                    QStringLiteral("Comment"),
                                    Artifact::MarkerType::Comment);
    auto* second = points.addMarker(ArtifactCore::FramePosition(40),
                                    QStringLiteral("Act 2"),
                                    Artifact::MarkerType::Chapter);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(middle, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(points.markerCount(), 3u);
    EXPECT_EQ(points.getMarkerAt(ArtifactCore::FramePosition(20)), middle);

    const auto nextMarker = points.nextMarker(ArtifactCore::FramePosition(10));
    ASSERT_TRUE(nextMarker.has_value());
    EXPECT_EQ(nextMarker->framePosition(), 20);
    const auto previousMarker = points.previousMarker(ArtifactCore::FramePosition(20));
    ASSERT_TRUE(previousMarker.has_value());
    EXPECT_EQ(previousMarker->framePosition(), 10);
    EXPECT_FALSE(points.nextMarker(ArtifactCore::FramePosition(40)).has_value());
    EXPECT_FALSE(points.previousMarker(ArtifactCore::FramePosition(10)).has_value());

    const auto chapters = points.chapterMarkers();
    ASSERT_EQ(chapters.size(), 2u);
    EXPECT_EQ(chapters[0], first);
    EXPECT_EQ(chapters[1], second);
    const auto nextChapter = points.nextChapter(ArtifactCore::FramePosition(10));
    ASSERT_TRUE(nextChapter.has_value());
    EXPECT_EQ(nextChapter->framePosition(), 40);
    const auto previousChapter = points.previousChapter(ArtifactCore::FramePosition(40));
    ASSERT_TRUE(previousChapter.has_value());
    EXPECT_EQ(previousChapter->framePosition(), 10);
    EXPECT_FALSE(points.nextChapter(ArtifactCore::FramePosition(40)).has_value());
    EXPECT_FALSE(points.previousChapter(ArtifactCore::FramePosition(10)).has_value());
}

TEST(InOutPointsContractTest, InvalidJsonImportPreservesExistingRangeAndMarkers)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(5));
    points.setOutPoint(ArtifactCore::FramePosition(75));
    points.addMarker(ArtifactCore::FramePosition(20), QStringLiteral("Keep me"),
                     Artifact::MarkerType::Chapter);
    const auto original = points.toJson();

    auto invalidMarkers = original;
    invalidMarkers.insert(QStringLiteral("markers"), QStringLiteral("not an array"));
    EXPECT_FALSE(points.fromJson(invalidMarkers));
    EXPECT_EQ(points.toJson(), original);

    auto invalidInPoint = original;
    invalidInPoint.insert(QStringLiteral("inPoint"), QStringLiteral("not a frame"));
    EXPECT_FALSE(points.fromJson(invalidInPoint));
    EXPECT_EQ(points.toJson(), original);
}
