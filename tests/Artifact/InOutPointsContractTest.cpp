#include <gtest/gtest.h>

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QStringList>

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

TEST(InOutPointsContractTest, MarkerUpdateKeepsIdentityAndSearchesCommentLinkAndTags)
{
    Artifact::ArtifactInOutPoints points;
    auto* later = points.addMarker(ArtifactCore::FramePosition(40),
                                   QStringLiteral("Review notes"),
                                   Artifact::MarkerType::Comment);
    auto* earlier = points.addMarker(ArtifactCore::FramePosition(10),
                                     QStringLiteral("Unrelated"),
                                     Artifact::MarkerType::WebLink);
    auto* samePosition = points.addMarker(ArtifactCore::FramePosition(40),
                                          QStringLiteral("Updated review"),
                                          Artifact::MarkerType::Chapter);
    ASSERT_NE(later, nullptr);
    ASSERT_NE(earlier, nullptr);
    EXPECT_EQ(samePosition, later);
    EXPECT_EQ(points.markerCount(), 2u);
    EXPECT_EQ(later->comment(), QStringLiteral("Updated review"));
    EXPECT_EQ(later->type(), Artifact::MarkerType::Chapter);

    later->setWebLink(QStringLiteral("https://example.test/review"));
    later->addTag(QStringLiteral(" Editorial "));
    later->addTag(QStringLiteral("editorial"));
    earlier->addTag(QStringLiteral("Review"));

    const auto byType = points.markersByType(Artifact::MarkerType::Chapter);
    ASSERT_EQ(byType.size(), 1u);
    EXPECT_EQ(byType.front(), later);
    const auto byTag = points.markersByTag(QStringLiteral(" EDITORIAL "));
    ASSERT_EQ(byTag.size(), 1u);
    EXPECT_EQ(byTag.front(), later);

    const auto matches = points.searchMarkers(QStringLiteral(" review "));
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0], earlier);
    EXPECT_EQ(matches[1], later);
    EXPECT_EQ(later->tags().size(), 1);
}

TEST(InOutPointsContractTest, ValidJsonRoundTripPreservesMarkerMetadata)
{
    Artifact::ArtifactInOutPoints source;
    source.setInPoint(ArtifactCore::FramePosition(-12));
    source.setOutPoint(ArtifactCore::FramePosition(144));
    auto* marker = source.addMarker(ArtifactCore::FramePosition(36),
                                    QStringLiteral("Scene transition"),
                                    Artifact::MarkerType::WebLink);
    ASSERT_NE(marker, nullptr);
    marker->setColor(QColor(12, 34, 56, 78));
    marker->setWebLink(QStringLiteral("https://example.test/scene"));
    marker->setTags(QStringList{QStringLiteral(" edit "), QStringLiteral("review")});
    const auto serialized = source.toJson();

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.fromJson(serialized));
    EXPECT_EQ(restored.toJson(), serialized);
    ASSERT_EQ(restored.markerCount(), 1u);
    const auto* restoredMarker = restored.getMarkerAt(ArtifactCore::FramePosition(36));
    ASSERT_NE(restoredMarker, nullptr);
    EXPECT_EQ(restoredMarker->comment(), QStringLiteral("Scene transition"));
    EXPECT_EQ(restoredMarker->type(), Artifact::MarkerType::WebLink);
    EXPECT_EQ(restoredMarker->color(), QColor(12, 34, 56, 78));
    EXPECT_EQ(restoredMarker->webLink(), QStringLiteral("https://example.test/scene"));
    EXPECT_EQ(restoredMarker->tags(), (QStringList{QStringLiteral("edit"),
                                                  QStringLiteral("review")}));
    ASSERT_TRUE(restored.inPoint().has_value());
    ASSERT_TRUE(restored.outPoint().has_value());
    EXPECT_EQ(restored.inPoint()->framePosition(), -12);
    EXPECT_EQ(restored.outPoint()->framePosition(), 144);
}

TEST(InOutPointsContractTest, MarkerRangeIsInclusiveAndRemovalRefreshesChapterIndex)
{
    Artifact::ArtifactInOutPoints points;
    auto* before = points.addMarker(ArtifactCore::FramePosition(9),
                                    QStringLiteral("Before"),
                                    Artifact::MarkerType::Comment);
    auto* firstChapter = points.addMarker(ArtifactCore::FramePosition(10),
                                          QStringLiteral("First"),
                                          Artifact::MarkerType::Chapter);
    auto* lastChapter = points.addMarker(ArtifactCore::FramePosition(20),
                                         QStringLiteral("Last"),
                                         Artifact::MarkerType::Chapter);
    auto* after = points.addMarker(ArtifactCore::FramePosition(21),
                                   QStringLiteral("After"),
                                   Artifact::MarkerType::Comment);
    ASSERT_NE(before, nullptr);
    ASSERT_NE(firstChapter, nullptr);
    ASSERT_NE(lastChapter, nullptr);
    ASSERT_NE(after, nullptr);

    const auto inRange = points.markersInRange(
        ArtifactCore::FrameRange(10, 20));
    ASSERT_EQ(inRange.size(), 2u);
    EXPECT_EQ(inRange[0], firstChapter);
    EXPECT_EQ(inRange[1], lastChapter);

    Artifact::ArtifactMarker foreign(ArtifactCore::FramePosition(10),
                                     QStringLiteral("Foreign"),
                                     Artifact::MarkerType::Chapter);
    points.removeMarker(&foreign);
    EXPECT_EQ(points.markerCount(), 4u);
    EXPECT_EQ(points.getMarkerAt(ArtifactCore::FramePosition(10)), firstChapter);
    EXPECT_EQ(foreign.comment(), QStringLiteral("Foreign"));

    ASSERT_EQ(points.chapterMarkers().size(), 2u);
    EXPECT_TRUE(points.removeMarker(ArtifactCore::FramePosition(10)));
    EXPECT_FALSE(points.removeMarker(ArtifactCore::FramePosition(10)));
    const auto chaptersAfterPositionRemoval = points.chapterMarkers();
    ASSERT_EQ(chaptersAfterPositionRemoval.size(), 1u);
    EXPECT_EQ(chaptersAfterPositionRemoval.front(), lastChapter);

    points.removeMarker(lastChapter);
    points.removeMarker(nullptr);
    EXPECT_TRUE(points.chapterMarkers().empty());
    EXPECT_EQ(points.markerCount(), 2u);
}
