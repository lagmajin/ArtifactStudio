#include <gtest/gtest.h>

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

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

TEST(InOutPointsContractTest, SetAtCurrentUsesCurrentFrameForEachPoint)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPointAtCurrent(ArtifactCore::FramePosition(12));
    points.setOutPointAtCurrent(ArtifactCore::FramePosition(88));

    ASSERT_TRUE(points.inPoint().has_value());
    ASSERT_TRUE(points.outPoint().has_value());
    EXPECT_EQ(points.inPoint()->framePosition(), 12);
    EXPECT_EQ(points.outPoint()->framePosition(), 88);
    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                   ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(12, 88));
}

TEST(InOutPointsContractTest, EqualInOutPointsProduceSingleFrameRange)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(42));
    points.setOutPoint(ArtifactCore::FramePosition(42));

    EXPECT_EQ(points.effectiveRange(ArtifactCore::FramePosition(0),
                                    ArtifactCore::FramePosition(100)),
              ArtifactCore::FrameRange(42, 42));
    EXPECT_TRUE(points.isRangeLimited());
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

TEST(InOutPointsContractTest, MarkersByTypeFiltersAndSortsMultipleMatches)
{
    Artifact::ArtifactInOutPoints points;
    auto* laterChapter = points.addMarker(ArtifactCore::FramePosition(40),
                                          QStringLiteral("Later"),
                                          Artifact::MarkerType::Chapter);
    auto* comment = points.addMarker(ArtifactCore::FramePosition(20),
                                     QStringLiteral("Comment"),
                                     Artifact::MarkerType::Comment);
    auto* earlierChapter = points.addMarker(ArtifactCore::FramePosition(10),
                                            QStringLiteral("Earlier"),
                                            Artifact::MarkerType::Chapter);
    ASSERT_NE(laterChapter, nullptr);
    ASSERT_NE(comment, nullptr);
    ASSERT_NE(earlierChapter, nullptr);

    const auto chapters = points.markersByType(Artifact::MarkerType::Chapter);

    ASSERT_EQ(chapters.size(), 2u);
    EXPECT_EQ(chapters[0], earlierChapter);
    EXPECT_EQ(chapters[1], laterChapter);
}

TEST(InOutPointsContractTest, MarkersByTagFiltersCaseInsensitivelyAndSorts)
{
    Artifact::ArtifactInOutPoints points;
    auto* later = points.addMarker(ArtifactCore::FramePosition(40),
                                   QStringLiteral("Later"));
    auto* other = points.addMarker(ArtifactCore::FramePosition(20),
                                   QStringLiteral("Other"));
    auto* earlier = points.addMarker(ArtifactCore::FramePosition(10),
                                     QStringLiteral("Earlier"));
    ASSERT_NE(later, nullptr);
    ASSERT_NE(other, nullptr);
    ASSERT_NE(earlier, nullptr);
    later->addTag(QStringLiteral("Review"));
    earlier->addTag(QStringLiteral("review"));

    const auto matches = points.markersByTag(QStringLiteral(" REVIEW "));

    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0], earlier);
    EXPECT_EQ(matches[1], later);
}

TEST(InOutPointsContractTest, MovingManagedMarkerUpdatesPositionLookup)
{
    Artifact::ArtifactInOutPoints points;
    auto* marker = points.addMarker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Move me"));
    ASSERT_NE(marker, nullptr);

    marker->setPosition(ArtifactCore::FramePosition(30));

    EXPECT_EQ(marker->position().framePosition(), 30);
    EXPECT_EQ(points.getMarkerAt(ArtifactCore::FramePosition(10)), nullptr);
    EXPECT_EQ(points.getMarkerAt(ArtifactCore::FramePosition(30)), marker);
}

TEST(InOutPointsContractTest, EmptyTagQueryReturnsNoMarkers)
{
    Artifact::ArtifactInOutPoints points;
    auto* marker = points.addMarker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Tagged"));
    ASSERT_NE(marker, nullptr);
    marker->addTag(QStringLiteral("existing"));

    EXPECT_TRUE(points.markersByTag(QString()).empty());
    EXPECT_TRUE(points.markersByTag(QStringLiteral(" \t ")).empty());
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

TEST(InOutPointsContractTest, SuccessfulPartialJsonImportClearsOmittedFields)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(5));
    points.setOutPoint(ArtifactCore::FramePosition(75));
    ASSERT_NE(points.addMarker(ArtifactCore::FramePosition(20),
                               QStringLiteral("Old marker")), nullptr);

    ASSERT_TRUE(points.fromJson(QJsonObject{
        {QStringLiteral("inPoint"), 30}}));

    ASSERT_TRUE(points.inPoint().has_value());
    EXPECT_EQ(points.inPoint()->framePosition(), 30);
    EXPECT_FALSE(points.outPoint().has_value());
    EXPECT_EQ(points.markerCount(), 0u);
}

TEST(InOutPointsContractTest, FractionalFrameJsonIsRejectedWithoutChangingState)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(5));
    points.setOutPoint(ArtifactCore::FramePosition(75));
    ASSERT_NE(points.addMarker(ArtifactCore::FramePosition(20),
                               QStringLiteral("Keep me")), nullptr);
    const auto original = points.toJson();

    EXPECT_FALSE(points.fromJson(QJsonObject{
        {QStringLiteral("inPoint"), 10.5},
        {QStringLiteral("markers"), QJsonArray{QJsonObject{
            {QStringLiteral("position"), 30.25},
            {QStringLiteral("comment"), QStringLiteral("Fractional")}}}}}));
    EXPECT_EQ(points.toJson(), original);
}

TEST(InOutPointsContractTest, OutOfRangeFrameJsonIsRejectedWithoutChangingState)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(5));
    points.setOutPoint(ArtifactCore::FramePosition(75));
    ASSERT_NE(points.addMarker(ArtifactCore::FramePosition(20),
                               QStringLiteral("Keep me")), nullptr);
    const auto original = points.toJson();

    EXPECT_FALSE(points.fromJson(QJsonObject{
        {QStringLiteral("outPoint"), 1.0e30}}));
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

TEST(InOutPointsContractTest, ReplacingMarkerAtSamePositionPreservesOtherMetadata)
{
    Artifact::ArtifactInOutPoints points;
    auto* marker = points.addMarker(ArtifactCore::FramePosition(40),
                                    QStringLiteral("Original"),
                                    Artifact::MarkerType::Comment);
    ASSERT_NE(marker, nullptr);
    marker->setColor(QColor(12, 34, 56, 78));
    marker->setWebLink(QStringLiteral("https://example.test/original"));
    marker->setTags(QStringList{QStringLiteral("keep"), QStringLiteral("also keep")});

    auto* updated = points.addMarker(ArtifactCore::FramePosition(40),
                                     QStringLiteral("Updated"),
                                     Artifact::MarkerType::Chapter);

    ASSERT_EQ(updated, marker);
    EXPECT_EQ(updated->comment(), QStringLiteral("Updated"));
    EXPECT_EQ(updated->type(), Artifact::MarkerType::Chapter);
    EXPECT_EQ(updated->color(), QColor(12, 34, 56, 78));
    EXPECT_EQ(updated->webLink(), QStringLiteral("https://example.test/original"));
    EXPECT_EQ(updated->tags(), (QStringList{QStringLiteral("keep"),
                                           QStringLiteral("also keep")}));
}

TEST(InOutPointsContractTest, MarkerTagRemovalIsCaseInsensitiveAndPreservesOtherTags)
{
    Artifact::ArtifactMarker marker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Tagged"));
    marker.setTags(QStringList{QStringLiteral("Review"),
                               QStringLiteral("Keep"),
                               QStringLiteral("review")});

    marker.removeTag(QStringLiteral(" REVIEW "));
    EXPECT_EQ(marker.tags(), (QStringList{QStringLiteral("Keep")}));

    marker.removeTag(QStringLiteral("missing"));
    EXPECT_EQ(marker.tags(), (QStringList{QStringLiteral("Keep")}));
}

TEST(InOutPointsContractTest, SettingMarkerTagsTrimsValuesAndDropsEmptyEntries)
{
    Artifact::ArtifactMarker marker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Tagged"));

    marker.setTags(QStringList{QStringLiteral(" first "),
                               QStringLiteral(""),
                               QStringLiteral(" \t "),
                               QStringLiteral("second ")});

    EXPECT_EQ(marker.tags(), (QStringList{QStringLiteral("first"),
                                          QStringLiteral("second")}));
}

TEST(InOutPointsContractTest, SettingMarkerTagsDeduplicatesAfterTrimming)
{
    Artifact::ArtifactMarker marker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Tagged"));

    marker.setTags(QStringList{QStringLiteral("topic"),
                               QStringLiteral(" topic ")});

    EXPECT_EQ(marker.tags(), (QStringList{QStringLiteral("topic")}));
}

TEST(InOutPointsContractTest, SettingEmptyMarkerTagsClearsExistingTags)
{
    Artifact::ArtifactMarker marker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Tagged"));
    marker.setTags(QStringList{QStringLiteral("review"),
                               QStringLiteral("approved")});

    marker.setTags({});

    EXPECT_TRUE(marker.tags().isEmpty());
}

TEST(InOutPointsContractTest, MarkerComparisonUsesFramePosition)
{
    const Artifact::ArtifactMarker earlier(
        ArtifactCore::FramePosition(10), QStringLiteral("Earlier"));
    const Artifact::ArtifactMarker samePositionDifferentMetadata(
        ArtifactCore::FramePosition(10), QStringLiteral("Different"),
        Artifact::MarkerType::Chapter);
    const Artifact::ArtifactMarker later(
        ArtifactCore::FramePosition(20), QStringLiteral("Later"));

    EXPECT_TRUE(earlier == samePositionDifferentMetadata);
    EXPECT_FALSE(earlier < samePositionDifferentMetadata);
    EXPECT_TRUE(earlier < later);
    EXPECT_FALSE(later < earlier);
}

TEST(InOutPointsContractTest, AddingBlankMarkerTagsLeavesExistingTagsUnchanged)
{
    Artifact::ArtifactMarker marker(ArtifactCore::FramePosition(10),
                                    QStringLiteral("Tagged"));
    marker.setTags(QStringList{QStringLiteral("keep")});

    marker.addTag(QString());
    marker.addTag(QStringLiteral(" \t "));

    EXPECT_EQ(marker.tags(), (QStringList{QStringLiteral("keep")}));
}

TEST(InOutPointsContractTest, EmptySearchReturnsAllMarkersInFrameOrder)
{
    Artifact::ArtifactInOutPoints points;
    auto* later = points.addMarker(ArtifactCore::FramePosition(40),
                                   QStringLiteral("Later"));
    auto* earlier = points.addMarker(ArtifactCore::FramePosition(10),
                                     QStringLiteral("Earlier"));
    ASSERT_NE(later, nullptr);
    ASSERT_NE(earlier, nullptr);

    const auto emptySearch = points.searchMarkers(QString());
    ASSERT_EQ(emptySearch.size(), 2u);
    EXPECT_EQ(emptySearch[0], earlier);
    EXPECT_EQ(emptySearch[1], later);

    const auto whitespaceSearch = points.searchMarkers(QStringLiteral(" \t "));
    ASSERT_EQ(whitespaceSearch.size(), 2u);
    EXPECT_EQ(whitespaceSearch[0], earlier);
    EXPECT_EQ(whitespaceSearch[1], later);
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

TEST(InOutPointsContractTest, JsonRoundTripPreservesFramePositionsAboveInt32Range)
{
    constexpr qint64 largeFrame = 3'000'000'000LL;
    Artifact::ArtifactInOutPoints source;
    source.setInPoint(ArtifactCore::FramePosition(largeFrame));
    source.setOutPoint(ArtifactCore::FramePosition(largeFrame + 10));
    ASSERT_NE(source.addMarker(ArtifactCore::FramePosition(largeFrame + 5),
                               QStringLiteral("Large frame")), nullptr);

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.fromJson(source.toJson()));

    ASSERT_TRUE(restored.inPoint().has_value());
    ASSERT_TRUE(restored.outPoint().has_value());
    EXPECT_EQ(restored.inPoint()->framePosition(), largeFrame);
    EXPECT_EQ(restored.outPoint()->framePosition(), largeFrame + 10);
    const auto* marker = restored.getMarkerAt(
        ArtifactCore::FramePosition(largeFrame + 5));
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(marker->comment(), QStringLiteral("Large frame"));
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

TEST(InOutPointsContractTest, ClearingMarkersPreservesInOutPoints)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(5));
    points.setOutPoint(ArtifactCore::FramePosition(75));
    ASSERT_NE(points.addMarker(ArtifactCore::FramePosition(20),
                               QStringLiteral("Comment")), nullptr);
    ASSERT_NE(points.addMarker(ArtifactCore::FramePosition(40),
                               QStringLiteral("Chapter"),
                               Artifact::MarkerType::Chapter), nullptr);

    points.clearAllMarkers();

    EXPECT_EQ(points.markerCount(), 0u);
    EXPECT_TRUE(points.chapterMarkers().empty());
    ASSERT_TRUE(points.inPoint().has_value());
    ASSERT_TRUE(points.outPoint().has_value());
    EXPECT_EQ(points.inPoint()->framePosition(), 5);
    EXPECT_EQ(points.outPoint()->framePosition(), 75);
}

TEST(InOutPointsContractTest, ClearingInOutPointsPreservesMarkers)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(5));
    points.setOutPoint(ArtifactCore::FramePosition(75));
    auto* marker = points.addMarker(ArtifactCore::FramePosition(20),
                                    QStringLiteral("Keep marker"),
                                    Artifact::MarkerType::Chapter);
    ASSERT_NE(marker, nullptr);

    points.clearAllPoints();

    EXPECT_FALSE(points.inPoint().has_value());
    EXPECT_FALSE(points.outPoint().has_value());
    EXPECT_FALSE(points.isRangeLimited());
    EXPECT_EQ(points.markerCount(), 1u);
    EXPECT_EQ(points.getMarkerAt(ArtifactCore::FramePosition(20)), marker);
    ASSERT_EQ(points.chapterMarkers().size(), 1u);
    EXPECT_EQ(points.chapterMarkers().front(), marker);
}

TEST(InOutPointsContractTest, JsonImportSkipsMalformedMarkersAndDefaultsUnknownTypes)
{
    QJsonArray markers;
    markers.append(QStringLiteral("not a marker object"));
    markers.append(QJsonObject{{QStringLiteral("comment"), QStringLiteral("No position")}});
    markers.append(QJsonObject{
        {QStringLiteral("position"), 7},
        {QStringLiteral("comment"), QStringLiteral("Legacy marker")},
        {QStringLiteral("type"), 999},
        {QStringLiteral("tags"), QJsonArray{
            QStringLiteral(" legacy "), 42, QStringLiteral("")}}
    });
    markers.append(QJsonObject{
        {QStringLiteral("position"), 8},
        {QStringLiteral("comment"), QStringLiteral("Valid marker")},
        {QStringLiteral("type"), static_cast<int>(Artifact::MarkerType::Flash)}
    });

    Artifact::ArtifactInOutPoints points;
    ASSERT_TRUE(points.fromJson(QJsonObject{{QStringLiteral("markers"), markers}}));
    ASSERT_EQ(points.markerCount(), 2u);

    const auto* legacy = points.getMarkerAt(ArtifactCore::FramePosition(7));
    ASSERT_NE(legacy, nullptr);
    EXPECT_EQ(legacy->comment(), QStringLiteral("Legacy marker"));
    EXPECT_EQ(legacy->type(), Artifact::MarkerType::Comment);
    EXPECT_EQ(legacy->color(), QColor(QStringLiteral("#FFFF00")));
    EXPECT_EQ(legacy->tags(), (QStringList{QStringLiteral("legacy")}));

    const auto* valid = points.getMarkerAt(ArtifactCore::FramePosition(8));
    ASSERT_NE(valid, nullptr);
    EXPECT_EQ(valid->comment(), QStringLiteral("Valid marker"));
    EXPECT_EQ(valid->type(), Artifact::MarkerType::Flash);
}

TEST(InOutPointsContractTest, JsonImportMergesDuplicateMarkerPositionsInArrayOrder)
{
    const QJsonArray markers{
        QJsonObject{
            {QStringLiteral("position"), 12},
            {QStringLiteral("comment"), QStringLiteral("First")},
            {QStringLiteral("type"), static_cast<int>(Artifact::MarkerType::Comment)}},
        QJsonObject{
            {QStringLiteral("position"), 12},
            {QStringLiteral("comment"), QStringLiteral("Last")},
            {QStringLiteral("type"), static_cast<int>(Artifact::MarkerType::Chapter)}}
    };

    Artifact::ArtifactInOutPoints points;
    ASSERT_TRUE(points.fromJson(QJsonObject{{QStringLiteral("markers"), markers}}));

    ASSERT_EQ(points.markerCount(), 1u);
    const auto* marker = points.getMarkerAt(ArtifactCore::FramePosition(12));
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(marker->comment(), QStringLiteral("Last"));
    EXPECT_EQ(marker->type(), Artifact::MarkerType::Chapter);
}

TEST(InOutPointsContractTest, JsonImportDefaultsMalformedOptionalMarkerMetadata)
{
    const QJsonObject malformedMarker{
        {QStringLiteral("position"), 16},
        {QStringLiteral("comment"), 42},
        {QStringLiteral("type"), QStringLiteral("Chapter")},
        {QStringLiteral("color"), 123},
        {QStringLiteral("webLink"), false},
        {QStringLiteral("tags"), QStringLiteral("not an array")}
    };
    Artifact::ArtifactInOutPoints points;

    ASSERT_TRUE(points.fromJson(QJsonObject{
        {QStringLiteral("markers"), QJsonArray{malformedMarker}}}));

    ASSERT_EQ(points.markerCount(), 1u);
    const auto* marker = points.getMarkerAt(ArtifactCore::FramePosition(16));
    ASSERT_NE(marker, nullptr);
    EXPECT_TRUE(marker->comment().isEmpty());
    EXPECT_EQ(marker->type(), Artifact::MarkerType::Comment);
    EXPECT_EQ(marker->color(), QColor(QStringLiteral("#FFFF00")));
    EXPECT_TRUE(marker->webLink().isEmpty());
    EXPECT_TRUE(marker->tags().isEmpty());
}

TEST(InOutPointsContractTest, XmlRoundTripPreservesRangeAndSupportedMarkerFields)
{
    Artifact::ArtifactInOutPoints source;
    source.setInPoint(ArtifactCore::FramePosition(-18));
    source.setOutPoint(ArtifactCore::FramePosition(240));
    auto* chapter = source.addMarker(ArtifactCore::FramePosition(24),
                                     QStringLiteral("Intro & <title>"),
                                     Artifact::MarkerType::Chapter);
    auto* flash = source.addMarker(ArtifactCore::FramePosition(96),
                                   QStringLiteral("Flash frame"),
                                   Artifact::MarkerType::Flash);
    ASSERT_NE(chapter, nullptr);
    ASSERT_NE(flash, nullptr);
    chapter->setTags(QStringList{QStringLiteral("opening"),
                                 QStringLiteral("review")});

    const QString xml = source.exportToXML();
    ASSERT_FALSE(xml.isEmpty());
    EXPECT_NE(xml.indexOf(QStringLiteral("&amp;")), -1);
    EXPECT_NE(xml.indexOf(QStringLiteral("&lt;title&gt;")), -1);

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.importFromXML(xml));
    EXPECT_EQ(restored.toJson(), source.toJson());
    ASSERT_EQ(restored.markerCount(), 2u);
    ASSERT_TRUE(restored.inPoint().has_value());
    ASSERT_TRUE(restored.outPoint().has_value());
    EXPECT_EQ(restored.inPoint()->framePosition(), -18);
    EXPECT_EQ(restored.outPoint()->framePosition(), 240);
    const auto* restoredChapter =
        restored.getMarkerAt(ArtifactCore::FramePosition(24));
    ASSERT_NE(restoredChapter, nullptr);
    EXPECT_EQ(restoredChapter->comment(), QStringLiteral("Intro & <title>"));
    EXPECT_EQ(restoredChapter->type(), Artifact::MarkerType::Chapter);
    EXPECT_EQ(restoredChapter->tags(), (QStringList{QStringLiteral("opening"),
                                                   QStringLiteral("review")}));
}

TEST(InOutPointsContractTest, EmptyStateXmlRoundTripRemainsEmpty)
{
    Artifact::ArtifactInOutPoints source;
    const QString xml = source.exportToXML();
    ASSERT_FALSE(xml.isEmpty());

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.importFromXML(xml));

    EXPECT_FALSE(restored.inPoint().has_value());
    EXPECT_FALSE(restored.outPoint().has_value());
    EXPECT_FALSE(restored.isRangeLimited());
    EXPECT_EQ(restored.markerCount(), 0u);
    EXPECT_TRUE(restored.allMarkers().empty());
}

TEST(InOutPointsContractTest, XmlRoundTripPreservesEmptyCommentAndOmitsEmptyTags)
{
    Artifact::ArtifactInOutPoints source;
    ASSERT_NE(source.addMarker(ArtifactCore::FramePosition(18)),
              nullptr);

    const QString xml = source.exportToXML();
    EXPECT_NE(xml.indexOf(QStringLiteral("comment=\"\"")), -1);
    EXPECT_EQ(xml.indexOf(QStringLiteral("tags=")), -1);

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.importFromXML(xml));
    const auto* marker = restored.getMarkerAt(ArtifactCore::FramePosition(18));
    ASSERT_NE(marker, nullptr);
    EXPECT_TRUE(marker->comment().isEmpty());
    EXPECT_TRUE(marker->tags().isEmpty());
}

TEST(InOutPointsContractTest, XmlRoundTripEscapesSpecialCharactersInTags)
{
    Artifact::ArtifactInOutPoints source;
    auto* marker = source.addMarker(ArtifactCore::FramePosition(18),
                                    QStringLiteral("Tagged"));
    ASSERT_NE(marker, nullptr);
    marker->setTags(QStringList{QStringLiteral("review & approve"),
                                QStringLiteral("<urgent>")});

    const QString xml = source.exportToXML();
    EXPECT_NE(xml.indexOf(QStringLiteral("&amp;")), -1);
    EXPECT_NE(xml.indexOf(QStringLiteral("&lt;urgent&gt;")), -1);

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.importFromXML(xml));
    const auto* restoredMarker =
        restored.getMarkerAt(ArtifactCore::FramePosition(18));
    ASSERT_NE(restoredMarker, nullptr);
    EXPECT_EQ(restoredMarker->tags(), (QStringList{
        QStringLiteral("review & approve"), QStringLiteral("<urgent>")}));
}

TEST(InOutPointsContractTest, XmlRoundTripPreservesFramePositionsAboveInt32Range)
{
    constexpr qint64 largeFrame = 3'000'000'000LL;
    Artifact::ArtifactInOutPoints source;
    source.setInPoint(ArtifactCore::FramePosition(largeFrame));
    source.setOutPoint(ArtifactCore::FramePosition(largeFrame + 10));
    ASSERT_NE(source.addMarker(ArtifactCore::FramePosition(largeFrame + 5),
                               QStringLiteral("Large frame")), nullptr);

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.importFromXML(source.exportToXML()));

    ASSERT_TRUE(restored.inPoint().has_value());
    ASSERT_TRUE(restored.outPoint().has_value());
    EXPECT_EQ(restored.inPoint()->framePosition(), largeFrame);
    EXPECT_EQ(restored.outPoint()->framePosition(), largeFrame + 10);
    const auto* marker = restored.getMarkerAt(
        ArtifactCore::FramePosition(largeFrame + 5));
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(marker->comment(), QStringLiteral("Large frame"));
}

TEST(InOutPointsContractTest, XmlRoundTripPreservesSemicolonsInsideTags)
{
    Artifact::ArtifactInOutPoints source;
    auto* marker = source.addMarker(ArtifactCore::FramePosition(18),
                                    QStringLiteral("Tagged"));
    ASSERT_NE(marker, nullptr);
    marker->setTags(QStringList{QStringLiteral("phase;review"),
                                QStringLiteral("keep")});

    Artifact::ArtifactInOutPoints restored;
    ASSERT_TRUE(restored.importFromXML(source.exportToXML()));
    const auto* restoredMarker =
        restored.getMarkerAt(ArtifactCore::FramePosition(18));
    ASSERT_NE(restoredMarker, nullptr);
    EXPECT_EQ(restoredMarker->tags(), (QStringList{
        QStringLiteral("phase;review"), QStringLiteral("keep")}));
}

TEST(InOutPointsContractTest, XmlImportSkipsEmptyTagSegmentsAndPreservesOrder)
{
    const QString xml = QStringLiteral(
        "<InOutPoints><Marker frame=\"18\" comment=\"Tagged\" "
        "tags=\"alpha;; beta;\" type=\"Comment\"/></InOutPoints>");
    Artifact::ArtifactInOutPoints points;

    ASSERT_TRUE(points.importFromXML(xml));
    const auto* marker = points.getMarkerAt(ArtifactCore::FramePosition(18));
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(marker->tags(), (QStringList{
        QStringLiteral("alpha"), QStringLiteral("beta")}));
}

TEST(InOutPointsContractTest, MalformedXmlImportPreservesExistingState)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(-12));
    points.setOutPoint(ArtifactCore::FramePosition(144));
    auto* marker = points.addMarker(ArtifactCore::FramePosition(32),
                                    QStringLiteral("Keep this marker"),
                                    Artifact::MarkerType::Chapter);
    ASSERT_NE(marker, nullptr);
    const QJsonObject before = points.toJson();
    const QString malformedXml = QStringLiteral(
        "<InOutPoints><InPoint frame=\"100\"/>"
        "<Marker frame=\"6\" comment=\"Partial import\" type=\"Chapter\"/>"
        "<OutPoint frame=\"200\"></InOutPoints>");

    EXPECT_FALSE(points.importFromXML(malformedXml));
    EXPECT_EQ(points.toJson(), before);
}

TEST(InOutPointsContractTest, XmlImportRejectsUnexpectedRootWithoutChangingState)
{
    Artifact::ArtifactInOutPoints points;
    points.setInPoint(ArtifactCore::FramePosition(-12));
    points.setOutPoint(ArtifactCore::FramePosition(144));
    ASSERT_NE(points.addMarker(ArtifactCore::FramePosition(32),
                               QStringLiteral("Keep this marker"),
                               Artifact::MarkerType::Chapter), nullptr);
    const QJsonObject before = points.toJson();

    EXPECT_FALSE(points.importFromXML(QStringLiteral("<OtherRoot/>")));
    EXPECT_EQ(points.toJson(), before);
}

TEST(InOutPointsContractTest, XmlImportSkipsInvalidFrameAttributesAndKeepsValidRecords)
{
    const QString xml = QStringLiteral(
        "<InOutPoints>"
        "<InPoint frame=\"not-a-frame\"/>"
        "<OutPoint frame=\"24\"/>"
        "<Marker frame=\"invalid\" comment=\"Skip\" type=\"Chapter\"/>"
        "<Marker frame=\"36\" comment=\"Keep\" type=\"Comment\"/>"
        "</InOutPoints>");
    Artifact::ArtifactInOutPoints points;

    ASSERT_TRUE(points.importFromXML(xml));

    EXPECT_FALSE(points.inPoint().has_value());
    ASSERT_TRUE(points.outPoint().has_value());
    EXPECT_EQ(points.outPoint()->framePosition(), 24);
    ASSERT_EQ(points.markerCount(), 1u);
    const auto* marker = points.getMarkerAt(ArtifactCore::FramePosition(36));
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(marker->comment(), QStringLiteral("Keep"));
}

TEST(InOutPointsContractTest, XmlImportSkipsUnknownNestedElementsAndContinues)
{
    const QString xml = QStringLiteral(
        "<InOutPoints>"
        "<FutureExtension><InPoint frame=\"900\"/>"
        "<Marker frame=\"901\" comment=\"Nested\"/></FutureExtension>"
        "<InPoint frame=\"12\"/>"
        "<Marker frame=\"24\" comment=\"Current\" type=\"Comment\"/>"
        "</InOutPoints>");
    Artifact::ArtifactInOutPoints points;

    ASSERT_TRUE(points.importFromXML(xml));

    ASSERT_TRUE(points.inPoint().has_value());
    EXPECT_EQ(points.inPoint()->framePosition(), 12);
    EXPECT_EQ(points.markerCount(), 1u);
    EXPECT_EQ(points.getMarkerAt(ArtifactCore::FramePosition(901)), nullptr);
    const auto* marker = points.getMarkerAt(ArtifactCore::FramePosition(24));
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(marker->comment(), QStringLiteral("Current"));
}
