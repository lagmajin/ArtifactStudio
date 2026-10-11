#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>

import Core.Diagnostics.Snapshot;
import Utils.Result;

using ArtifactCore::CoreDiagnosticSeverity;
using ArtifactCore::DiagnosticEvent;
using ArtifactCore::DiagnosticSnapshot;
using ArtifactCore::ErrorCode;
using ArtifactCore::ErrorContext;
using ArtifactCore::diagnosticSeverityName;
using ArtifactCore::diagnosticSnapshotToJson;
using ArtifactCore::makeDiagnosticEvent;
using ArtifactCore::sourceLocation;

namespace {

auto makeEvent(CoreDiagnosticSeverity severity, std::string code,
               std::uint64_t sequence = 0, std::uint64_t timestamp = 0) -> DiagnosticEvent
{
    auto event = makeDiagnosticEvent(severity, std::move(code), "message",
                                     "component", "operation", "object");
    event.sequence = sequence;
    event.timestampNs = timestamp;
    return event;
}

} // namespace

TEST(DiagnosticSnapshotStandaloneTest, DefaultSnapshotHasEmptyBoundsAndNoErrors)
{
    const DiagnosticSnapshot snapshot;
    EXPECT_EQ(snapshot.version, 1u);
    EXPECT_TRUE(snapshot.recentEvents.empty());
    EXPECT_EQ(snapshot.firstSequence, 0u);
    EXPECT_EQ(snapshot.lastSequence, 0u);
    EXPECT_FALSE(snapshot.hasErrors());
    EXPECT_EQ(snapshot.latestFailure(), nullptr);
}

TEST(DiagnosticSnapshotStandaloneTest, SeverityNamesAreStable)
{
    EXPECT_STREQ(diagnosticSeverityName(CoreDiagnosticSeverity::Info), "info");
    EXPECT_STREQ(diagnosticSeverityName(CoreDiagnosticSeverity::Warning), "warning");
    EXPECT_STREQ(diagnosticSeverityName(CoreDiagnosticSeverity::Error), "error");
    EXPECT_STREQ(diagnosticSeverityName(CoreDiagnosticSeverity::Fatal), "fatal");
    EXPECT_STREQ(diagnosticSeverityName(static_cast<CoreDiagnosticSeverity>(255)), "info");
}

TEST(DiagnosticSnapshotStandaloneTest, EventFailurePredicateIncludesErrorAndFatal)
{
    EXPECT_FALSE(makeEvent(CoreDiagnosticSeverity::Info, "i").isFailure());
    EXPECT_FALSE(makeEvent(CoreDiagnosticSeverity::Warning, "w").isFailure());
    EXPECT_TRUE(makeEvent(CoreDiagnosticSeverity::Error, "e").isFailure());
    EXPECT_TRUE(makeEvent(CoreDiagnosticSeverity::Fatal, "f").isFailure());
}

TEST(DiagnosticSnapshotStandaloneTest, RefreshSequenceBoundsTracksFrontAndBack)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "first", 3));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "last", 9));
    snapshot.refreshSequenceBounds();
    EXPECT_EQ(snapshot.firstSequence, 3u);
    EXPECT_EQ(snapshot.lastSequence, 9u);
}

TEST(DiagnosticSnapshotStandaloneTest, RefreshSequenceBoundsResetsForEmptyEvents)
{
    DiagnosticSnapshot snapshot;
    snapshot.firstSequence = 4;
    snapshot.lastSequence = 8;
    snapshot.refreshSequenceBounds();
    EXPECT_EQ(snapshot.firstSequence, 0u);
    EXPECT_EQ(snapshot.lastSequence, 0u);
}

TEST(DiagnosticSnapshotStandaloneTest, NormalizeOrderSortsBySequenceWhenAvailable)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "late", 9, 1));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "early", 2, 99));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "middle", 5, 2));

    snapshot.normalizeOrder();
    ASSERT_EQ(snapshot.recentEvents.size(), 3u);
    EXPECT_EQ(snapshot.recentEvents[0].code, "early");
    EXPECT_EQ(snapshot.recentEvents[1].code, "middle");
    EXPECT_EQ(snapshot.recentEvents[2].code, "late");
    EXPECT_EQ(snapshot.firstSequence, 2u);
    EXPECT_EQ(snapshot.lastSequence, 9u);
}

TEST(DiagnosticSnapshotStandaloneTest, NormalizeOrderPlacesSequencedEventsBeforeUnsequenced)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "unsequenced", 0, 1));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "sequenced", 1, 99));
    snapshot.normalizeOrder();
    EXPECT_EQ(snapshot.recentEvents[0].code, "sequenced");
    EXPECT_EQ(snapshot.recentEvents[1].code, "unsequenced");
}

TEST(DiagnosticSnapshotStandaloneTest, NormalizeOrderUsesTimestampWhenSequencesAreZero)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "later", 0, 12));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "earlier", 0, 4));
    snapshot.normalizeOrder();
    EXPECT_EQ(snapshot.recentEvents[0].code, "earlier");
    EXPECT_EQ(snapshot.recentEvents[1].code, "later");
}

TEST(DiagnosticSnapshotStandaloneTest, NormalizeOrderIsStableForEqualKeys)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "first", 5));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Info, "second", 5));
    snapshot.normalizeOrder();
    EXPECT_EQ(snapshot.recentEvents[0].code, "first");
    EXPECT_EQ(snapshot.recentEvents[1].code, "second");
}

TEST(DiagnosticSnapshotStandaloneTest, CountReturnsSeveritySpecificTotals)
{
    DiagnosticSnapshot snapshot;
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "i"));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Warning, "w"));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Error, "e"));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Fatal, "f"));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Error, "e2"));

    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Info), 1u);
    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Warning), 1u);
    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Error), 2u);
    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Fatal), 1u);
}

TEST(DiagnosticSnapshotStandaloneTest, HasErrorsIncludesFatalButNotWarnings)
{
    DiagnosticSnapshot warningOnly;
    warningOnly.addEvent(makeEvent(CoreDiagnosticSeverity::Warning, "warning"));
    EXPECT_FALSE(warningOnly.hasErrors());

    DiagnosticSnapshot fatal;
    fatal.addEvent(makeEvent(CoreDiagnosticSeverity::Fatal, "fatal"));
    EXPECT_TRUE(fatal.hasErrors());
}

TEST(DiagnosticSnapshotStandaloneTest, LatestFailureUsesGreatestSequence)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Error, "seq-2", 2, 100));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Fatal, "seq-7", 7, 1));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Error, "warning", 99, 999));
    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->code, "seq-7");
}

TEST(DiagnosticSnapshotStandaloneTest, LatestFailureUsesTimestampWhenSequencesAreAbsent)
{
    DiagnosticSnapshot snapshot;
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Error, "old", 0, 5));
    snapshot.recentEvents.push_back(makeEvent(CoreDiagnosticSeverity::Fatal, "new", 0, 10));
    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->code, "new");
}

TEST(DiagnosticSnapshotStandaloneTest, LatestFailureIsNullWhenSnapshotHasOnlyNonFailures)
{
    DiagnosticSnapshot snapshot;
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "info"));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Warning, "warning"));
    EXPECT_EQ(snapshot.latestFailure(), nullptr);
}

TEST(DiagnosticSnapshotStandaloneTest, AddEventTrimsOldestAndRefreshesBounds)
{
    DiagnosticSnapshot snapshot;
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "one", 1), 2);
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "two", 2), 2);
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "three", 3), 2);

    ASSERT_EQ(snapshot.recentEvents.size(), 2u);
    EXPECT_EQ(snapshot.recentEvents.front().code, "two");
    EXPECT_TRUE(snapshot.eventsTruncated);
    EXPECT_EQ(snapshot.firstSequence, 2u);
    EXPECT_EQ(snapshot.lastSequence, 3u);
}

TEST(DiagnosticSnapshotStandaloneTest, AddEventWithZeroCapacityClearsAndMarksTruncated)
{
    DiagnosticSnapshot snapshot;
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Error, "discarded"), 0);
    EXPECT_TRUE(snapshot.recentEvents.empty());
    EXPECT_TRUE(snapshot.eventsTruncated);
    EXPECT_EQ(snapshot.firstSequence, 0u);
    EXPECT_EQ(snapshot.lastSequence, 0u);
}

TEST(DiagnosticSnapshotStandaloneTest, MergeFillsMissingMetadataAndAppendsEvents)
{
    DiagnosticSnapshot left;
    DiagnosticSnapshot right;
    right.component = "render";
    right.objectId = "comp-2";
    right.state = "warning";
    right.lastOperation = "compose";
    right.frameIndex = 12;
    right.threadId = 44;
    right.addEvent(makeEvent(CoreDiagnosticSeverity::Warning, "warning", 8));

    left.merge(right);
    EXPECT_EQ(left.component, "render");
    EXPECT_EQ(left.objectId, "comp-2");
    EXPECT_EQ(left.state, "warning");
    EXPECT_EQ(left.lastOperation, "compose");
    EXPECT_EQ(left.frameIndex, 12);
    EXPECT_EQ(left.threadId, 44u);
    ASSERT_EQ(left.recentEvents.size(), 1u);
    EXPECT_EQ(left.firstSequence, 8u);
}

TEST(DiagnosticSnapshotStandaloneTest, MergePreservesAlreadySetMetadata)
{
    DiagnosticSnapshot left;
    left.component = "left";
    left.frameIndex = 3;
    left.threadId = 5;
    DiagnosticSnapshot right;
    right.component = "right";
    right.frameIndex = 7;
    right.threadId = 9;

    left.merge(right);
    EXPECT_EQ(left.component, "left");
    EXPECT_EQ(left.frameIndex, 3);
    EXPECT_EQ(left.threadId, 5u);
}

TEST(DiagnosticSnapshotStandaloneTest, MergePropagatesTruncationAndEnforcesEventLimit)
{
    DiagnosticSnapshot left;
    left.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "one", 1));
    DiagnosticSnapshot right;
    right.eventsTruncated = true;
    right.addEvent(makeEvent(CoreDiagnosticSeverity::Warning, "two", 2));
    right.addEvent(makeEvent(CoreDiagnosticSeverity::Error, "three", 3));

    left.merge(right, 2);
    ASSERT_EQ(left.recentEvents.size(), 2u);
    EXPECT_EQ(left.recentEvents[0].code, "two");
    EXPECT_EQ(left.recentEvents[1].code, "three");
    EXPECT_TRUE(left.eventsTruncated);
    EXPECT_EQ(left.firstSequence, 2u);
    EXPECT_EQ(left.lastSequence, 3u);
}

TEST(DiagnosticSnapshotStandaloneTest, MergeWithZeroLimitDropsEventsAndMarksTruncated)
{
    DiagnosticSnapshot left;
    left.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "existing"));
    DiagnosticSnapshot right;
    right.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "incoming"));
    left.merge(right, 0);
    EXPECT_TRUE(left.recentEvents.empty());
    EXPECT_TRUE(left.eventsTruncated);
    EXPECT_EQ(left.firstSequence, 0u);
    EXPECT_EQ(left.lastSequence, 0u);
}

TEST(DiagnosticSnapshotStandaloneTest, ErrorContextFactoryBuildsErrorEventAndRetainsTrace)
{
    const ErrorContext error{
        .code = ErrorCode::NotFound,
        .message = "asset absent",
        .operation = "open",
        .objectId = "asset-1",
        .location = sourceLocation("asset.cpp", "open", 27),
        .traceId = 88};
    const auto event = makeDiagnosticEvent(error, "asset-loader");

    EXPECT_EQ(event.severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(event.code, "core.not_found");
    EXPECT_EQ(event.message, "asset absent");
    EXPECT_EQ(event.component, "asset-loader");
    EXPECT_EQ(event.operation, "open");
    EXPECT_EQ(event.objectId, "asset-1");
    EXPECT_EQ(event.traceId, 88u);
    EXPECT_EQ(event.location.line, 27);
}

TEST(DiagnosticSnapshotStandaloneTest, ErrorContextFactoryFallsBackToCodeNameAndCustomSeverity)
{
    ErrorContext error;
    error.code = ErrorCode::Busy;
    const auto event = makeDiagnosticEvent(error, "worker", CoreDiagnosticSeverity::Fatal);
    EXPECT_EQ(event.severity, CoreDiagnosticSeverity::Fatal);
    EXPECT_EQ(event.code, "core.busy");
    EXPECT_EQ(event.message, "busy");
}

TEST(DiagnosticSnapshotStandaloneTest, JsonSerializationContainsCountsLatestFailureAndRecentEvents)
{
    DiagnosticSnapshot snapshot;
    snapshot.component = "render";
    snapshot.frameIndex = 9;
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "info", 1));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Error, "failure", 2));
    const auto json = diagnosticSnapshotToJson(snapshot);

    EXPECT_NE(json.find("\"component\":\"render\""), std::string::npos);
    EXPECT_NE(json.find("\"frameIndex\":9"), std::string::npos);
    EXPECT_NE(json.find("\"infoCount\":1"), std::string::npos);
    EXPECT_NE(json.find("\"errorCount\":1"), std::string::npos);
    EXPECT_NE(json.find("\"hasErrors\":true"), std::string::npos);
    EXPECT_NE(json.find("\"latestFailureCode\":\"failure\""), std::string::npos);
    EXPECT_NE(json.find("\"recentEvents\":["), std::string::npos);
}

TEST(DiagnosticSnapshotStandaloneTest, JsonSerializationEscapesControlCharactersAndQuotes)
{
    DiagnosticSnapshot snapshot;
    snapshot.component = "quote\" slash\\";
    auto event = makeEvent(CoreDiagnosticSeverity::Error, "code\n", 1);
    event.message = std::string("tab\treturn\r") + static_cast<char>(1);
    snapshot.addEvent(std::move(event));
    const auto json = diagnosticSnapshotToJson(snapshot);

    EXPECT_NE(json.find("quote\\\" slash\\\\"), std::string::npos);
    EXPECT_NE(json.find("code\\n"), std::string::npos);
    EXPECT_NE(json.find("tab\\treturn\\r\\u0001"), std::string::npos);
}

TEST(DiagnosticSnapshotStandaloneTest, JsonSerializationEscapesBackspaceAndFormFeed)
{
    DiagnosticSnapshot snapshot;
    auto event = makeEvent(CoreDiagnosticSeverity::Error, "controls", 1);
    event.message = std::string("back") + '\b' + "form" + '\f' + "feed";
    snapshot.addEvent(std::move(event));

    const auto json = diagnosticSnapshotToJson(snapshot);
    EXPECT_NE(json.find("back\\bform\\ffeed"), std::string::npos);
}

TEST(DiagnosticSnapshotStandaloneTest, JsonSerializationUsesEachSeverityName)
{
    DiagnosticSnapshot snapshot;
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Info, "i", 1));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Warning, "w", 2));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Error, "e", 3));
    snapshot.addEvent(makeEvent(CoreDiagnosticSeverity::Fatal, "f", 4));

    const auto json = diagnosticSnapshotToJson(snapshot);
    EXPECT_NE(json.find("\"severity\":\"info\""), std::string::npos);
    EXPECT_NE(json.find("\"severity\":\"warning\""), std::string::npos);
    EXPECT_NE(json.find("\"severity\":\"error\""), std::string::npos);
    EXPECT_NE(json.find("\"severity\":\"fatal\""), std::string::npos);
}
