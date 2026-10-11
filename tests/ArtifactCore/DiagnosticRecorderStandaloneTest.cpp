#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <utility>

import Core.Diagnostics.Recorder;
import Core.Diagnostics.Snapshot;
import Utils.Result;

using ArtifactCore::CoreDiagnosticSeverity;
using ArtifactCore::DiagnosticEvent;
using ArtifactCore::DiagnosticRecorder;
using ArtifactCore::DiagnosticScope;
using ArtifactCore::ErrorCode;
using ArtifactCore::ErrorContext;
using ArtifactCore::Result;
using ArtifactCore::Status;
using ArtifactCore::makeDiagnosticEvent;
using ArtifactCore::sourceLocation;

class DiagnosticRecorderStandaloneTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        recorder().setEnabled(true);
        recorder().setMaxEvents(256);
        recorder().clear();
    }

    void TearDown() override
    {
        recorder().setEnabled(true);
        recorder().setMaxEvents(256);
        recorder().clear();
    }

    static auto recorder() -> DiagnosticRecorder& { return DiagnosticRecorder::instance(); }

    static auto event(CoreDiagnosticSeverity severity = CoreDiagnosticSeverity::Info,
                      std::string component = "render",
                      std::string objectId = "layer-1",
                      std::string operation = "draw") -> DiagnosticEvent
    {
        return makeDiagnosticEvent(severity, "test.event", "test message",
                                   std::move(component), std::move(operation),
                                   std::move(objectId));
    }
};

TEST_F(DiagnosticRecorderStandaloneTest, StartsWithNoEvents)
{
    EXPECT_TRUE(recorder().isEnabled());
    EXPECT_EQ(recorder().size(), 0u);
    EXPECT_EQ(recorder().latestSequence(), 0u);
    EXPECT_EQ(recorder().oldestSequence(), 0u);
    EXPECT_FALSE(recorder().latest().has_value());
}

TEST_F(DiagnosticRecorderStandaloneTest, DisabledRecorderDropsEvents)
{
    recorder().setEnabled(false);
    recorder().record(event());
    EXPECT_FALSE(recorder().isEnabled());
    EXPECT_EQ(recorder().size(), 0u);
    EXPECT_EQ(recorder().latestSequence(), 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, RecordingFillsTimestampThreadTraceAndSequence)
{
    recorder().record(event());
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_GT(latest->timestampNs, 0u);
    EXPECT_EQ(latest->threadId,
              static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    EXPECT_GT(latest->traceId, 0u);
    EXPECT_GT(latest->sequence, 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, RecordingPreservesSuppliedMetadata)
{
    auto value = event();
    value.timestampNs = 10;
    value.threadId = 20;
    value.traceId = 30;
    recorder().record(std::move(value));

    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->timestampNs, 10u);
    EXPECT_EQ(latest->threadId, 20u);
    EXPECT_EQ(latest->traceId, 30u);
    EXPECT_GT(latest->sequence, 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, SequenceIncreasesAcrossRecordedEvents)
{
    recorder().record(event());
    const auto first = recorder().latestSequence();
    recorder().record(event());
    EXPECT_GT(recorder().latestSequence(), first);
}

TEST_F(DiagnosticRecorderStandaloneTest, ConvenienceRecordOverloadCreatesEvent)
{
    recorder().record(CoreDiagnosticSeverity::Warning, "warn.code", "slow",
                      "renderer", "composite", "layer-2");
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->severity, CoreDiagnosticSeverity::Warning);
    EXPECT_EQ(latest->code, "warn.code");
    EXPECT_EQ(latest->message, "slow");
    EXPECT_EQ(latest->component, "renderer");
    EXPECT_EQ(latest->operation, "composite");
    EXPECT_EQ(latest->objectId, "layer-2");
}

TEST_F(DiagnosticRecorderStandaloneTest, RecordErrorCapturesContextAndFrame)
{
    const ErrorContext error{
        .code = ErrorCode::NotFound,
        .message = "asset missing",
        .operation = "loadTexture",
        .objectId = "asset-4",
        .location = sourceLocation("source.cpp", "loadTexture", 42),
        .traceId = 55};
    recorder().recordError(error, "asset-loader", 120);

    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(latest->code, "core.not_found");
    EXPECT_EQ(latest->message, "asset missing");
    EXPECT_EQ(latest->component, "asset-loader");
    EXPECT_EQ(latest->operation, "loadTexture");
    EXPECT_EQ(latest->objectId, "asset-4");
    EXPECT_EQ(latest->frameIndex, 120);
    EXPECT_EQ(latest->traceId, 55u);
    EXPECT_TRUE(latest->location.hasValue());
}

TEST_F(DiagnosticRecorderStandaloneTest, RecordErrorNormalizesNoneCodeToFailed)
{
    recorder().recordError(ErrorContext{}, "component");
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->code, "core.failed");
}

TEST_F(DiagnosticRecorderStandaloneTest, SuccessfulStatusIsNotRecorded)
{
    recorder().recordStatus(Status::ok(), "component");
    EXPECT_EQ(recorder().size(), 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, FailedStatusIsRecordedWithFrameIndex)
{
    auto status = Status::fail(ErrorCode::Busy);
    recorder().recordStatus(status, "worker", 7);
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(latest->code, "core.busy");
    EXPECT_EQ(latest->frameIndex, 7);
}

TEST_F(DiagnosticRecorderStandaloneTest, RecordResultReturnsFalseForSuccess)
{
    const auto result = Result<int>::ok(12);
    EXPECT_FALSE(recorder().recordResult(result, "component"));
    EXPECT_EQ(recorder().size(), 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, RecordResultReturnsTrueAndRecordsFailure)
{
    const auto result = Result<int>::fail(ErrorCode::InvalidArgument);
    EXPECT_TRUE(recorder().recordResult(result, "parser", 3));
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->code, "core.invalid_argument");
    EXPECT_EQ(latest->component, "parser");
    EXPECT_EQ(latest->frameIndex, 3);
}

TEST_F(DiagnosticRecorderStandaloneTest, EventsForFiltersByComponentAndOptionalObject)
{
    recorder().record(event(CoreDiagnosticSeverity::Info, "render", "layer-a"));
    recorder().record(event(CoreDiagnosticSeverity::Info, "render", "layer-b"));
    recorder().record(event(CoreDiagnosticSeverity::Info, "audio", "layer-a"));

    EXPECT_EQ(recorder().eventsFor("render").size(), 2u);
    const auto filtered = recorder().eventsFor("render", "layer-b");
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].objectId, "layer-b");
}

TEST_F(DiagnosticRecorderStandaloneTest, SinceReturnsOnlyEventsAfterSequence)
{
    recorder().record(event());
    const auto firstSequence = recorder().latestSequence();
    recorder().record(event());
    recorder().record(event());

    const auto newer = recorder().since(firstSequence);
    ASSERT_EQ(newer.size(), 2u);
    EXPECT_GT(newer.front().sequence, firstSequence);
}

TEST_F(DiagnosticRecorderStandaloneTest, SnapshotSinceFiltersAndDerivesStateFromLatestMatch)
{
    recorder().record(event(CoreDiagnosticSeverity::Error, "render", "layer-a", "first"));
    const auto cursor = recorder().latestSequence();
    recorder().record(event(CoreDiagnosticSeverity::Info, "render", "layer-b", "other"));
    recorder().record(event(CoreDiagnosticSeverity::Warning, "render", "layer-a", "final"));

    const auto snapshot = recorder().snapshotSince(cursor, "render", "layer-a", 44);
    ASSERT_EQ(snapshot.recentEvents.size(), 1u);
    EXPECT_EQ(snapshot.component, "render");
    EXPECT_EQ(snapshot.objectId, "layer-a");
    EXPECT_EQ(snapshot.frameIndex, 44);
    EXPECT_EQ(snapshot.state, "warning");
    EXPECT_EQ(snapshot.lastOperation, "final");
    EXPECT_EQ(snapshot.firstSequence, snapshot.lastSequence);
}

TEST_F(DiagnosticRecorderStandaloneTest, SnapshotSinceMarksDroppedHistoryAsTruncated)
{
    const auto cursor = recorder().latestSequence();
    recorder().setMaxEvents(2);
    recorder().record(event());
    recorder().record(event());
    recorder().record(event());

    EXPECT_TRUE(recorder().snapshotSince(cursor).eventsTruncated);
}

TEST_F(DiagnosticRecorderStandaloneTest, MissedSinceDetectsEvictedSequenceRange)
{
    const auto cursor = recorder().latestSequence();
    recorder().setMaxEvents(1);
    recorder().record(event());
    const auto firstRetained = recorder().oldestSequence();
    recorder().record(event());

    EXPECT_TRUE(recorder().missedSince(cursor));
    EXPECT_FALSE(recorder().missedSince(firstRetained));
}

TEST_F(DiagnosticRecorderStandaloneTest, ErrorsForIncludesErrorAndFatalOnly)
{
    recorder().record(event(CoreDiagnosticSeverity::Info));
    recorder().record(event(CoreDiagnosticSeverity::Warning));
    recorder().record(event(CoreDiagnosticSeverity::Error));
    recorder().record(event(CoreDiagnosticSeverity::Fatal));

    const auto errors = recorder().errorsFor();
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0].severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(errors[1].severity, CoreDiagnosticSeverity::Fatal);
}

TEST_F(DiagnosticRecorderStandaloneTest, ErrorsForCanFilterByComponentAndObject)
{
    recorder().record(event(CoreDiagnosticSeverity::Error, "render", "a"));
    recorder().record(event(CoreDiagnosticSeverity::Fatal, "render", "b"));
    recorder().record(event(CoreDiagnosticSeverity::Error, "audio", "a"));

    const auto errors = recorder().errorsFor("render", "b");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].objectId, "b");
}

TEST_F(DiagnosticRecorderStandaloneTest, LatestReturnsMostRecentlyRecordedEvent)
{
    recorder().record(event(CoreDiagnosticSeverity::Info));
    recorder().record(event(CoreDiagnosticSeverity::Fatal, "crash", "process", "terminate"));
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->component, "crash");
    EXPECT_EQ(latest->operation, "terminate");
}

TEST_F(DiagnosticRecorderStandaloneTest, DrainReturnsEventsAndEmptiesRecorder)
{
    recorder().record(event());
    recorder().record(event());
    const auto drained = recorder().drain();
    EXPECT_EQ(drained.size(), 2u);
    EXPECT_EQ(recorder().size(), 0u);
    EXPECT_FALSE(recorder().latest().has_value());
}

TEST_F(DiagnosticRecorderStandaloneTest, DrainSnapshotCarriesCallerMetadataAndBounds)
{
    recorder().record(event());
    recorder().record(event());
    const auto snapshot = recorder().drainSnapshot("renderer", "comp-2", "warning", "present", 99);

    EXPECT_EQ(snapshot.component, "renderer");
    EXPECT_EQ(snapshot.objectId, "comp-2");
    EXPECT_EQ(snapshot.state, "warning");
    EXPECT_EQ(snapshot.lastOperation, "present");
    EXPECT_EQ(snapshot.frameIndex, 99);
    ASSERT_EQ(snapshot.recentEvents.size(), 2u);
    EXPECT_EQ(snapshot.firstSequence, snapshot.recentEvents.front().sequence);
    EXPECT_EQ(snapshot.lastSequence, snapshot.recentEvents.back().sequence);
    EXPECT_EQ(recorder().size(), 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, DrainJsonSerializesSnapshotAndConsumesEvents)
{
    recorder().record(event(CoreDiagnosticSeverity::Error));
    const auto json = recorder().drainJson("render", "layer-1", "error", "draw", 5);
    EXPECT_NE(json.find("\"component\":\"render\""), std::string::npos);
    EXPECT_NE(json.find("\"objectId\":\"layer-1\""), std::string::npos);
    EXPECT_NE(json.find("test.event"), std::string::npos);
    EXPECT_EQ(recorder().size(), 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, SnapshotReturnsCopyWithoutConsumingEvents)
{
    recorder().record(event());
    const auto snapshot = recorder().snapshot("renderer", "object", "ready", "idle", 2);
    EXPECT_EQ(snapshot.component, "renderer");
    EXPECT_EQ(snapshot.state, "ready");
    EXPECT_EQ(snapshot.recentEvents.size(), 1u);
    EXPECT_EQ(recorder().size(), 1u);
}

TEST_F(DiagnosticRecorderStandaloneTest, SnapshotForFiltersAndDerivesLatestState)
{
    recorder().record(event(CoreDiagnosticSeverity::Error, "render", "layer-a", "first"));
    recorder().record(event(CoreDiagnosticSeverity::Info, "audio", "layer-a", "other"));
    recorder().record(event(CoreDiagnosticSeverity::Fatal, "render", "layer-a", "last"));
    const auto snapshot = recorder().snapshotFor("render", "layer-a", 8);

    ASSERT_EQ(snapshot.recentEvents.size(), 2u);
    EXPECT_EQ(snapshot.frameIndex, 8);
    EXPECT_EQ(snapshot.state, "fatal");
    EXPECT_EQ(snapshot.lastOperation, "last");
    EXPECT_TRUE(snapshot.hasErrors());
}

TEST_F(DiagnosticRecorderStandaloneTest, SnapshotForJsonSerializesFilteredSnapshot)
{
    recorder().record(event(CoreDiagnosticSeverity::Warning, "render", "layer-a"));
    recorder().record(event(CoreDiagnosticSeverity::Error, "audio", "layer-b"));
    const auto json = recorder().snapshotForJson("render", "layer-a", 3);
    EXPECT_NE(json.find("\"component\":\"render\""), std::string::npos);
    EXPECT_EQ(json.find("audio"), std::string::npos);
}

TEST_F(DiagnosticRecorderStandaloneTest, SnapshotJsonSerializesCallerProvidedState)
{
    recorder().record(event());
    const auto json = recorder().snapshotJson("render", "layer-1", "rendering", "draw", 14);
    EXPECT_NE(json.find("\"state\":\"rendering\""), std::string::npos);
    EXPECT_NE(json.find("\"lastOperation\":\"draw\""), std::string::npos);
    EXPECT_NE(json.find("\"frameIndex\":14"), std::string::npos);
    EXPECT_EQ(recorder().size(), 1u);
}

TEST_F(DiagnosticRecorderStandaloneTest, ClearEmptiesEventsWithoutResettingSequence)
{
    recorder().record(event());
    const auto previousSequence = recorder().latestSequence();
    recorder().clear();
    EXPECT_EQ(recorder().size(), 0u);
    EXPECT_EQ(recorder().latestSequence(), 0u);
    recorder().record(event());
    EXPECT_GT(recorder().latestSequence(), previousSequence);
}

TEST_F(DiagnosticRecorderStandaloneTest, MaxEventCountTrimsOldestAndClampsToOne)
{
    recorder().setMaxEvents(2);
    recorder().record(event(CoreDiagnosticSeverity::Info, "c", "first"));
    recorder().record(event(CoreDiagnosticSeverity::Info, "c", "second"));
    recorder().record(event(CoreDiagnosticSeverity::Info, "c", "third"));
    ASSERT_EQ(recorder().size(), 2u);
    EXPECT_EQ(recorder().snapshot().front().objectId, "second");

    recorder().setMaxEvents(0);
    recorder().record(event(CoreDiagnosticSeverity::Info, "c", "fourth"));
    ASSERT_EQ(recorder().size(), 1u);
    EXPECT_EQ(recorder().snapshot().front().objectId, "fourth");
}

TEST_F(DiagnosticRecorderStandaloneTest, AllocateTraceIdReturnsIncreasingValues)
{
    const auto first = recorder().allocateTraceId();
    const auto second = recorder().allocateTraceId();
    EXPECT_GT(first, 0u);
    EXPECT_GT(second, first);
}

TEST_F(DiagnosticRecorderStandaloneTest, ScopeFinishSuccessRecordsDurationAndTrace)
{
    {
        DiagnosticScope scope("render", "compose", "comp-1");
        scope.finish(true, "done");
    }
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->severity, CoreDiagnosticSeverity::Info);
    EXPECT_EQ(latest->code, "scope.completed");
    EXPECT_EQ(latest->message, "done");
    EXPECT_EQ(latest->component, "render");
    EXPECT_EQ(latest->operation, "compose");
    EXPECT_EQ(latest->objectId, "comp-1");
    EXPECT_GT(latest->traceId, 0u);
}

TEST_F(DiagnosticRecorderStandaloneTest, ScopeFinishFailureRecordsError)
{
    DiagnosticScope scope("render", "compose");
    scope.finish(false);
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(latest->code, "scope.failed");
    EXPECT_EQ(latest->message, "failed");
}

TEST_F(DiagnosticRecorderStandaloneTest, UnfinishedScopeRecordsFailureOnDestruction)
{
    {
        DiagnosticScope scope("writer", "flush", "file-1");
    }
    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->code, "scope.failed");
    EXPECT_EQ(latest->message, "scope exited without explicit completion");
    EXPECT_EQ(latest->objectId, "file-1");
}

TEST_F(DiagnosticRecorderStandaloneTest, ScopeCanFinishFromErrorContext)
{
    DiagnosticScope scope("reader", "decode", "asset-1",
                          sourceLocation("reader.cpp", "decode", 18));
    const ErrorContext error{
        .code = ErrorCode::InvalidArgument,
        .message = "bad header",
        .operation = "readHeader",
        .objectId = "context-object",
        .location = sourceLocation("error.cpp", "readHeader", 27)};
    scope.finish(error);

    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->code, "core.invalid_argument");
    EXPECT_EQ(latest->message, "bad header");
    EXPECT_EQ(latest->operation, "decode");
    EXPECT_EQ(latest->objectId, "asset-1");
    EXPECT_STREQ(latest->location.file, "reader.cpp");
}

TEST_F(DiagnosticRecorderStandaloneTest, ScopeUsesErrorLocationAndObjectWhenNotOverridden)
{
    DiagnosticScope scope("reader", "", "",
                          sourceLocation("", "", 0));
    const ErrorContext error{
        .code = ErrorCode::NotFound,
        .message = "missing",
        .operation = "openFile",
        .objectId = "file-8",
        .location = sourceLocation("fallback.cpp", "openFile", 61)};
    scope.finish(error);

    const auto latest = recorder().latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->operation, "openFile");
    EXPECT_EQ(latest->objectId, "file-8");
    EXPECT_STREQ(latest->location.file, "fallback.cpp");
}

TEST_F(DiagnosticRecorderStandaloneTest, RepeatedScopeFinishRecordsOnlyOnce)
{
    DiagnosticScope scope("render", "compose");
    scope.finish();
    scope.finish(false, "second attempt");
    EXPECT_EQ(recorder().size(), 1u);
}

TEST_F(DiagnosticRecorderStandaloneTest, ScopeCreatedWhileDisabledDoesNotRecord)
{
    recorder().setEnabled(false);
    {
        DiagnosticScope scope("render", "compose");
        scope.finish(false, "failure");
    }
    EXPECT_EQ(recorder().size(), 0u);
}
