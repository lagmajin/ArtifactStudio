#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

import Core.Diagnostics.CrashReportParser;
import Core.Diagnostics.Recorder;
import Core.Diagnostics.Snapshot;
import Utils.Result;
import Utils.Text.Encoding;
import Utils.Text.Number;
import Utils.Text.String;

using namespace ArtifactCore;

namespace {

class RecorderReset {
public:
    RecorderReset()
        : recorder_(DiagnosticRecorder::instance()), wasEnabled_(recorder_.isEnabled())
    {
        recorder_.setEnabled(true);
        recorder_.setMaxEvents(256);
        recorder_.clear();
    }

    ~RecorderReset()
    {
        recorder_.clear();
        recorder_.setMaxEvents(256);
        recorder_.setEnabled(wasEnabled_);
    }

    RecorderReset(const RecorderReset&) = delete;
    RecorderReset& operator=(const RecorderReset&) = delete;

private:
    DiagnosticRecorder& recorder_;
    bool wasEnabled_;
};

} // namespace

TEST(CoreDiagnosticsContractTest, SnapshotSortsEventsAndFindsLatestFailure)
{
    DiagnosticSnapshot snapshot;
    DiagnosticEvent later = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Error, "test.later", "later failure",
        "Test", "later", "object-1");
    later.sequence = 2;
    DiagnosticEvent earlier = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Error, "test.earlier", "earlier failure",
        "Test", "earlier", "object-1");
    earlier.sequence = 1;
    snapshot.addEvent(later);
    snapshot.addEvent(earlier);
    snapshot.component = "Test";

    DiagnosticSnapshot unsorted;
    unsorted.recentEvents = {later, earlier};
    const std::string json = diagnosticSnapshotToJson(snapshot);

    ASSERT_TRUE(snapshot.hasErrors());
    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->code, "test.later");
    ASSERT_NE(unsorted.latestFailure(), nullptr);
    EXPECT_EQ(unsorted.latestFailure()->code, "test.later");
    EXPECT_NE(json.find("\"hasErrors\":true"), std::string::npos);
    EXPECT_NE(json.find("test.earlier"), std::string::npos);
    EXPECT_NE(json.find("latestFailureFunction"), std::string::npos);
}

TEST(CoreDiagnosticsContractTest, SnapshotCountsEachSeverityAndSelectsLatestFailure)
{
    DiagnosticSnapshot snapshot;
    const CoreDiagnosticSeverity severities[] = {
        CoreDiagnosticSeverity::Info,
        CoreDiagnosticSeverity::Warning,
        CoreDiagnosticSeverity::Error,
        CoreDiagnosticSeverity::Fatal};
    const char* codes[] = {"info", "warning", "error", "fatal"};
    for (std::size_t index = 0; index < 4; ++index) {
        auto event = makeDiagnosticEvent(
            severities[index], codes[index], codes[index], "Test", "count");
        event.sequence = index + 1;
        snapshot.addEvent(std::move(event));
    }

    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Info), 1u);
    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Warning), 1u);
    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Error), 1u);
    EXPECT_EQ(snapshot.count(CoreDiagnosticSeverity::Fatal), 1u);
    EXPECT_TRUE(snapshot.hasErrors());
    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->code, "fatal");
}

TEST(CoreDiagnosticsContractTest, SnapshotMergeSortsBoundsAndTruncatesOldestEvents)
{
    DiagnosticSnapshot primary;
    primary.component = "Primary";
    auto first = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Info, "first", "first", "Primary", "merge");
    first.sequence = 1;
    primary.recentEvents.push_back(first);

    DiagnosticSnapshot incoming;
    incoming.objectId = "object-4";
    auto third = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Error, "third", "third", "Primary", "merge");
    third.sequence = 3;
    auto second = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Warning, "second", "second", "Primary", "merge");
    second.sequence = 2;
    incoming.recentEvents = {third, second};

    primary.merge(incoming, 2);

    EXPECT_TRUE(primary.eventsTruncated);
    EXPECT_EQ(primary.component, "Primary");
    EXPECT_EQ(primary.objectId, "object-4");
    ASSERT_EQ(primary.recentEvents.size(), 2u);
    EXPECT_EQ(primary.recentEvents[0].code, "second");
    EXPECT_EQ(primary.recentEvents[1].code, "third");
    EXPECT_EQ(primary.firstSequence, 2u);
    EXPECT_EQ(primary.lastSequence, 3u);
}

TEST(CoreDiagnosticsContractTest, SnapshotAddEventEnforcesCapacityAndZeroLimit)
{
    DiagnosticSnapshot bounded;
    auto first = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Info, "first", "first", "Test", "add");
    first.sequence = 1;
    auto second = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Info, "second", "second", "Test", "add");
    second.sequence = 2;
    auto third = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Info, "third", "third", "Test", "add");
    third.sequence = 3;
    bounded.addEvent(first, 2);
    bounded.addEvent(second, 2);
    bounded.addEvent(third, 2);

    EXPECT_TRUE(bounded.eventsTruncated);
    ASSERT_EQ(bounded.recentEvents.size(), 2u);
    EXPECT_EQ(bounded.recentEvents.front().code, "second");
    EXPECT_EQ(bounded.firstSequence, 2u);
    EXPECT_EQ(bounded.lastSequence, 3u);

    DiagnosticSnapshot disabled;
    disabled.addEvent(first, 0);
    EXPECT_TRUE(disabled.eventsTruncated);
    EXPECT_TRUE(disabled.recentEvents.empty());
    EXPECT_EQ(disabled.firstSequence, 0u);
    EXPECT_EQ(disabled.lastSequence, 0u);
}

TEST(CoreDiagnosticsContractTest, LatestFailurePrefersSequencedEventOverTimestampOnly)
{
    DiagnosticSnapshot snapshot;
    auto timestampOnly = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Error, "timestamp", "timestamp", "Test", "run");
    timestampOnly.timestampNs = 999;
    auto sequenced = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Fatal, "sequence", "sequence", "Test", "run");
    sequenced.sequence = 1;
    snapshot.recentEvents = {timestampOnly, sequenced};

    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->code, "sequence");
}

TEST(CoreDiagnosticsContractTest, CrashReportParserBuildsFatalSnapshot)
{
    constexpr std::string_view report =
        "=== Artifact Crash Report ===\n"
        "Timestamp: 2026-07-11T12:00:00\n"
        "--- Exception ---\n"
        "Code: 0xC0000005\n"
        "Type: Access Violation\n"
        "Operation: Read\n"
        "Address: 0x1234\n"
        "Exception Address: 0x5678\n"
        "--- Stack Trace ---\n"
        "frame 0\n"
        "--- System Info ---\n"
        "OS: Windows\n";

    const auto summary = parseCrashReport(report);
    const auto snapshot = crashReportToSnapshot(summary);

    EXPECT_TRUE(summary.parsed);
    ASSERT_TRUE(snapshot.hasErrors());
    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->severity, CoreDiagnosticSeverity::Fatal);
    EXPECT_EQ(snapshot.latestFailure()->code, "crash.0xC0000005");
}

TEST(CoreDiagnosticsContractTest, CrashReportParserNormalizesCrLfStackAndMissingCode)
{
    constexpr std::string_view report =
        "=== Artifact Crash Report ===\r\n"
        "--- Exception ---\r\n"
        "Type: Access Violation\r\n"
        "Operation: Write\r\n"
        "Exception Address: 0xBEEF\r\n"
        "--- Stack Trace ---\r\n"
        "frame one\r\n"
        "frame two\r\n";

    const auto summary = parseCrashReport(report);
    const auto snapshot = crashReportToSnapshot(summary);

    EXPECT_TRUE(summary.parsed);
    EXPECT_TRUE(summary.exceptionCode.isEmpty());
    EXPECT_EQ(summary.stackTrace, "frame one\nframe two");
    EXPECT_EQ(snapshot.state, "Access Violation");
    EXPECT_EQ(snapshot.lastOperation, "Write");
    EXPECT_EQ(snapshot.objectId, "0xBEEF");
    ASSERT_NE(snapshot.latestFailure(), nullptr);
    EXPECT_EQ(snapshot.latestFailure()->code, "crash.unknown");
}

TEST(CoreDiagnosticsContractTest, CrashReportParserRejectsUnknownFormatMarker)
{
    const auto summary = parseCrashReport("not an Artifact crash report\n");
    EXPECT_FALSE(summary.parsed);
}

TEST(CoreDiagnosticsContractTest, CrashReportFileRejectsEmptyPathWithContext)
{
    const auto result = parseCrashReportFile("");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.errorContext().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(result.errorContext().operation, "parseCrashReportFile");
}

TEST(CoreDiagnosticsContractTest, SnapshotJsonEscapesQuotesSlashesAndControlBytes)
{
    DiagnosticSnapshot snapshot;
    auto event = makeDiagnosticEvent(
        CoreDiagnosticSeverity::Error, "test.json", "", "Test", "serialize");
    event.message = "quote\" backslash\\ newline\n tab\t return\r";
    event.message.push_back('\x01');
    snapshot.addEvent(event);

    const std::string json = diagnosticSnapshotToJson(snapshot);
    const std::string escaped =
        "\"message\":\"quote\\\" backslash\\\\ newline\\n tab\\t return\\r\\u0001\"";

    EXPECT_NE(json.find(escaped), std::string::npos);
    EXPECT_NE(json.find("\"latestFailureMessage\":\"quote\\\""),
              std::string::npos);
}

TEST(CoreDiagnosticsContractTest, RecorderAssignsIncreasingSequenceNumbers)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.record(CoreDiagnosticSeverity::Info,
                    "test.first", "first", "Test", "sequence");
    recorder.record(CoreDiagnosticSeverity::Warning,
                    "test.second", "second", "Test", "sequence");

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_LT(events[0].sequence, events[1].sequence);
}

TEST(CoreDiagnosticsContractTest, DisabledRecorderDoesNotStoreOrConsumeSequenceNumbers)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.record(CoreDiagnosticSeverity::Info,
                    "before-disabled", "before", "Toggle", "run");
    recorder.setEnabled(false);
    recorder.record(CoreDiagnosticSeverity::Error,
                    "disabled", "discarded", "Toggle", "run");
    recorder.setEnabled(true);
    recorder.record(CoreDiagnosticSeverity::Info,
                    "after-disabled", "after", "Toggle", "run");

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].code, "before-disabled");
    EXPECT_EQ(events[1].code, "after-disabled");
    EXPECT_EQ(events[1].sequence, events[0].sequence + 1u);
}

TEST(CoreDiagnosticsContractTest, RecorderRetainsConcurrentEventsWithUniqueSequences)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    constexpr std::size_t ThreadCount = 4;
    constexpr std::size_t EventsPerThread = 32;
    constexpr std::size_t EventCount = ThreadCount * EventsPerThread;
    std::array<std::thread, ThreadCount> workers;

    for (std::size_t workerIndex = 0; workerIndex < ThreadCount; ++workerIndex) {
        workers[workerIndex] = std::thread([&recorder, workerIndex] {
            for (std::size_t eventIndex = 0; eventIndex < EventsPerThread; ++eventIndex) {
                recorder.record(CoreDiagnosticSeverity::Info, "concurrent", "recorded",
                                "Concurrent", "write",
                                std::to_string(workerIndex));
            }
        });
    }
    for (auto& worker : workers) worker.join();

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), EventCount);
    std::array<std::uint64_t, EventCount> sequences{};
    for (std::size_t index = 0; index < EventCount; ++index) {
        sequences[index] = events[index].sequence;
        EXPECT_NE(sequences[index], 0u);
    }
    std::sort(sequences.begin(), sequences.end());
    EXPECT_EQ(std::adjacent_find(sequences.begin(), sequences.end()), sequences.end());
}

TEST(CoreDiagnosticsContractTest, RecorderResultPreservesErrorContext)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    const auto failure = Result<int>::fail(ErrorContext{
        .code = ErrorCode::NotFound,
        .message = "asset missing",
        .operation = "asset.open",
        .objectId = "asset-7"});

    EXPECT_TRUE(recorder.recordResult(failure, "AssetLoader", 12));
    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().code, "core.not_found");
    EXPECT_EQ(events.front().component, "AssetLoader");
    EXPECT_EQ(events.front().frameIndex, 12);
    EXPECT_EQ(events.front().objectId, "asset-7");
}

TEST(CoreDiagnosticsContractTest, RecorderDoesNotEmitSuccessfulResultOrStatus)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    const auto success = Result<int>::ok(42);

    EXPECT_FALSE(recorder.recordResult(success, "AssetLoader", 13));
    recorder.recordStatus(Status{true, ErrorCode::None, {}}, "LegacyQueue");
    EXPECT_EQ(recorder.size(), 0u);
}

TEST(CoreDiagnosticsContractTest, RecorderMapsLegacyStatusToCoreError)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.recordStatus(Status{false, ErrorCode::Busy, {}}, "LegacyQueue");

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().code, "core.busy");
    EXPECT_EQ(events.front().message, "busy");
}

TEST(CoreDiagnosticsContractTest, EmptyErrorContextUsesFailedCode)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.recordError(ErrorContext{}, "UnknownSource");

    const auto event = recorder.latest();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->code, "core.failed");
    EXPECT_EQ(event->component, "UnknownSource");
}

TEST(CoreDiagnosticsContractTest, DisabledRecorderSuppressesEvents)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.setEnabled(false);
    recorder.record(CoreDiagnosticSeverity::Info,
                    "disabled", "ignored", "Test", "run");
    EXPECT_EQ(recorder.size(), 0u);

    recorder.setEnabled(true);
    recorder.record(CoreDiagnosticSeverity::Info,
                    "enabled", "kept", "Test", "run");
    EXPECT_EQ(recorder.size(), 1u);
}

TEST(CoreDiagnosticsContractTest, ErrorContextFactoryIncludesOperationAndLocation)
{
    const auto failure = Result<int>::fail(
        ErrorCode::Busy, "queue is busy", "queue.push", "queue-1",
        sourceLocation(__FILE__, __func__, __LINE__));

    EXPECT_FALSE(failure);
    EXPECT_EQ(failure.errorContext().code, ErrorCode::Busy);
    EXPECT_EQ(failure.errorContext().operation, "queue.push");
    EXPECT_EQ(failure.errorContext().objectId, "queue-1");
    EXPECT_TRUE(failure.errorContext().location.hasValue());
}

TEST(CoreDiagnosticsContractTest, ScopeCompletionRecordsTraceId)
{
    RecorderReset reset;
    {
        DiagnosticScope scope("Decoder", "decode", "frame-1");
        scope.finish(true);
    }

    const auto events = DiagnosticRecorder::instance().snapshot();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().code, "scope.completed");
    EXPECT_EQ(events.front().component, "Decoder");
    EXPECT_NE(events.front().traceId, 0u);
}

TEST(CoreDiagnosticsContractTest, ScopeExplicitFailureRecordsErrorAndMessage)
{
    RecorderReset reset;
    {
        DiagnosticScope scope("Decoder", "decode", "frame-2");
        scope.finish(false, "decoder rejected frame");
    }

    const auto events = DiagnosticRecorder::instance().snapshot();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(events.front().code, "scope.failed");
    EXPECT_EQ(events.front().message, "decoder rejected frame");
    EXPECT_EQ(events.front().objectId, "frame-2");
    EXPECT_NE(events.front().traceId, 0u);
}

TEST(CoreDiagnosticsContractTest, ScopeWithoutExplicitFinishRecordsFailureOnDestruction)
{
    RecorderReset reset;
    {
        DiagnosticScope scope("Decoder", "decode", "frame-3");
    }

    const auto events = DiagnosticRecorder::instance().snapshot();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().severity, CoreDiagnosticSeverity::Error);
    EXPECT_EQ(events.front().code, "scope.failed");
    EXPECT_EQ(events.front().message, "scope exited without explicit completion");
    EXPECT_EQ(events.front().objectId, "frame-3");
    EXPECT_NE(events.front().traceId, 0u);
}

TEST(CoreDiagnosticsContractTest, RecorderDeltaReportsTruncationAndFilters)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.setMaxEvents(3);
    recorder.record(CoreDiagnosticSeverity::Info, "one", "1", "Delta", "run");
    const auto cursor = recorder.latestSequence();
    recorder.record(CoreDiagnosticSeverity::Info, "two", "2", "Delta", "run");
    recorder.record(CoreDiagnosticSeverity::Info, "other", "x", "Other", "run");
    recorder.record(CoreDiagnosticSeverity::Info, "three", "3", "Delta", "run");

    const auto delta = recorder.snapshotSince(cursor - 1, "Delta");

    EXPECT_TRUE(delta.eventsTruncated);
    ASSERT_EQ(delta.recentEvents.size(), 2u);
    EXPECT_LT(delta.firstSequence, delta.lastSequence);
    EXPECT_EQ(delta.lastOperation, "run");
    EXPECT_EQ(delta.recentEvents.front().component, "Delta");
    EXPECT_EQ(delta.recentEvents.back().component, "Delta");
}

TEST(CoreDiagnosticsContractTest, RecorderRingKeepsNewestEventsAndReportsDroppedRange)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.setMaxEvents(2);
    recorder.record(CoreDiagnosticSeverity::Info, "first", "1", "Ring", "run");
    recorder.record(CoreDiagnosticSeverity::Warning, "second", "2", "Ring", "run");
    recorder.record(CoreDiagnosticSeverity::Error, "third", "3", "Ring", "run");

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].code, "second");
    EXPECT_EQ(events[1].code, "third");
    EXPECT_TRUE(recorder.missedSince(0));
    EXPECT_TRUE(recorder.snapshotSince(0).eventsTruncated);
    EXPECT_EQ(recorder.oldestSequence(), events.front().sequence);
    EXPECT_EQ(recorder.latestSequence(), events.back().sequence);
}

TEST(CoreDiagnosticsContractTest, RecorderClampsZeroCapacityToOneEvent)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.setMaxEvents(0);
    recorder.record(CoreDiagnosticSeverity::Info, "first", "1", "Ring", "run");
    recorder.record(CoreDiagnosticSeverity::Info, "second", "2", "Ring", "run");

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().code, "second");
}

TEST(CoreDiagnosticsContractTest, LoweringRecorderCapacityImmediatelyDropsOldestEvents)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.record(CoreDiagnosticSeverity::Info, "first", "1", "Resize", "run");
    recorder.record(CoreDiagnosticSeverity::Info, "second", "2", "Resize", "run");
    recorder.record(CoreDiagnosticSeverity::Warning, "third", "3", "Resize", "run");
    recorder.record(CoreDiagnosticSeverity::Error, "fourth", "4", "Resize", "run");
    const auto firstSequence = recorder.oldestSequence();

    recorder.setMaxEvents(2);

    const auto events = recorder.snapshot();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].code, "third");
    EXPECT_EQ(events[1].code, "fourth");
    EXPECT_TRUE(recorder.missedSince(firstSequence));
    EXPECT_EQ(recorder.oldestSequence(), events.front().sequence);
}

TEST(CoreDiagnosticsContractTest, RecorderQueriesFilterByComponentObjectAndSeverity)
{
    RecorderReset reset;
    auto& recorder = DiagnosticRecorder::instance();
    recorder.record(CoreDiagnosticSeverity::Info,
                    "info", "info", "Renderer", "open", "surface-1");
    recorder.record(CoreDiagnosticSeverity::Error,
                    "error", "error", "Renderer", "draw", "surface-1");
    recorder.record(CoreDiagnosticSeverity::Fatal,
                    "fatal", "fatal", "Decoder", "read", "source-2");

    const auto surfaceEvents = recorder.eventsFor("Renderer", "surface-1");
    const auto rendererErrors = recorder.errorsFor("Renderer");
    const auto surfaceErrors = recorder.errorsFor("Renderer", "surface-1");
    const auto allErrors = recorder.errorsFor();

    ASSERT_EQ(surfaceEvents.size(), 2u);
    EXPECT_EQ(surfaceEvents[0].code, "info");
    EXPECT_EQ(surfaceEvents[1].code, "error");
    ASSERT_EQ(rendererErrors.size(), 1u);
    EXPECT_EQ(rendererErrors.front().code, "error");
    ASSERT_EQ(surfaceErrors.size(), 1u);
    EXPECT_EQ(surfaceErrors.front().code, "error");
    ASSERT_EQ(allErrors.size(), 2u);
    EXPECT_EQ(allErrors.back().code, "fatal");
}

TEST(CoreDiagnosticsContractTest, Utf8ValidationReportsInvalidInputContext)
{
    constexpr std::string_view valid = "\xE3\x81\x82\xF0\x9F\x98\x80";
    constexpr std::string_view invalidOverlong = "\xC0\xAF";
    const auto codepoints = toUtf32Checked(valid);
    const auto count = utf8CodepointCount(valid);
    const auto invalid = fromUtf8Checked(invalidOverlong);
    const auto invalidBomAware = fromUtf8BomAware("\xEF\xBB\xBF\xC0\xAF");
    const auto bomAware = fromUtf8BomAware("\xEF\xBB\xBFhello");

    EXPECT_TRUE(isValidUtf8(valid));
    EXPECT_FALSE(isValidUtf8(invalidOverlong));
    ASSERT_TRUE(codepoints);
    EXPECT_EQ(codepoints.value().size(), 2u);
    ASSERT_TRUE(count);
    EXPECT_EQ(count.value(), 2u);
    ASSERT_TRUE(bomAware);
    EXPECT_EQ(bomAware.value(), "hello");
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.errorContext().objectId, "0");
    ASSERT_FALSE(invalidBomAware);
    EXPECT_EQ(invalidBomAware.errorContext().operation, "encoding.fromUtf8BomAware");
    EXPECT_EQ(invalidBomAware.errorContext().objectId, "0");
    EXPECT_TRUE(invalidBomAware.errorContext().location.hasValue());
}

TEST(CoreDiagnosticsContractTest, Utf8ValidatorAcceptsUnicodeBoundaryCodePoints)
{
    constexpr std::string_view lastOneByte("\x7f", 1);
    constexpr std::string_view firstTwoByte("\xc2\x80", 2);
    constexpr std::string_view lastTwoByte("\xdf\xbf", 2);
    constexpr std::string_view firstThreeByte("\xe0\xa0\x80", 3);
    constexpr std::string_view lastBeforeSurrogates("\xed\x9f\xbf", 3);
    constexpr std::string_view firstAfterSurrogates("\xee\x80\x80", 3);
    constexpr std::string_view firstFourByte("\xf0\x90\x80\x80", 4);
    constexpr std::string_view lastUnicodeCodePoint("\xf4\x8f\xbf\xbf", 4);

    EXPECT_TRUE(isValidUtf8(lastOneByte));
    EXPECT_TRUE(isValidUtf8(firstTwoByte));
    EXPECT_TRUE(isValidUtf8(lastTwoByte));
    EXPECT_TRUE(isValidUtf8(firstThreeByte));
    EXPECT_TRUE(isValidUtf8(lastBeforeSurrogates));
    EXPECT_TRUE(isValidUtf8(firstAfterSurrogates));
    EXPECT_TRUE(isValidUtf8(firstFourByte));
    EXPECT_TRUE(isValidUtf8(lastUnicodeCodePoint));
}

TEST(CoreDiagnosticsContractTest, Utf8ValidatorRejectsTruncatedSurrogateAndOutOfRange)
{
    EXPECT_FALSE(isValidUtf8(std::string_view("\xc2", 1)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xe2\x82", 2)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xf0\x9f\x92", 3)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xed\xa0\x80", 3)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xf4\x90\x80\x80", 4)));

    const auto invalidOffset = fromUtf8Checked(std::string_view("a\x80", 2));
    ASSERT_FALSE(invalidOffset);
    EXPECT_EQ(invalidOffset.errorContext().objectId, "1");
}

TEST(CoreDiagnosticsContractTest, DetectsAndStripsByteOrderMarks)
{
    constexpr std::string_view utf8Bom = "\xEF\xBB\xBFtext";
    constexpr std::string_view utf16LeBom = "\xFF\xFEtext";
    constexpr std::string_view utf16BeBom = "\xFE\xFFtext";
    constexpr std::string_view utf32LeBom("\xFF\xFE\x00\x00text", 8);
    constexpr std::string_view utf32BeBom("\x00\x00\xFE\xFFtext", 8);
    constexpr std::string_view partialUtf8Bom("\xEF\xBB", 2);

    EXPECT_EQ(detectBom(utf8Bom), ByteOrderMark::Utf8);
    EXPECT_EQ(detectBom(utf16LeBom), ByteOrderMark::Utf16LittleEndian);
    EXPECT_EQ(detectBom(utf16BeBom), ByteOrderMark::Utf16BigEndian);
    EXPECT_EQ(detectBom(utf32LeBom), ByteOrderMark::Utf32LittleEndian);
    EXPECT_EQ(detectBom(utf32BeBom), ByteOrderMark::Utf32BigEndian);
    EXPECT_EQ(detectBom(partialUtf8Bom), ByteOrderMark::None);
    EXPECT_EQ(detectBom("plain"), ByteOrderMark::None);
    EXPECT_EQ(stripBom(utf8Bom), "text");
    EXPECT_EQ(stripBom(utf16LeBom), "text");
    EXPECT_EQ(stripBom(utf16BeBom), "text");
    EXPECT_EQ(stripBom(utf32LeBom), "text");
    EXPECT_EQ(stripBom(utf32BeBom), "text");
    EXPECT_EQ(stripBom("plain"), "plain");
}

TEST(CoreDiagnosticsContractTest, StringViewHelpersPreserveEmptyFields)
{
    const auto parts = splitView("a,,c", ',');
    const std::span<const std::string_view> view(parts.data(), parts.size());

    ASSERT_EQ(parts.size(), 3u);
    EXPECT_TRUE(parts[1].empty());
    EXPECT_EQ(join(view, '|'), "a||c");
    EXPECT_EQ(trimView(" \tvalue\r\n"), "value");
    EXPECT_TRUE(startsWith("artifact.core", "artifact"));
    EXPECT_TRUE(endsWith("artifact.core", "core"));
}

TEST(CoreDiagnosticsContractTest, StringViewSplitPreservesBoundaryAndEmptyInputs)
{
    const auto empty = splitView("", ',');
    const auto unsplit = splitView("asset", ',');
    const auto boundaries = splitView(",asset,", ',');
    constexpr std::string_view nonAsciiSpace("\xc2\xa0", 2);

    ASSERT_EQ(empty.size(), 1u);
    EXPECT_TRUE(empty.front().empty());
    ASSERT_EQ(unsplit.size(), 1u);
    EXPECT_EQ(unsplit.front(), "asset");
    ASSERT_EQ(boundaries.size(), 3u);
    EXPECT_TRUE(boundaries.front().empty());
    EXPECT_EQ(boundaries[1], "asset");
    EXPECT_TRUE(boundaries.back().empty());
    EXPECT_TRUE(trimView("\f\v value \f\v") == "value");
    EXPECT_EQ(trimView(nonAsciiSpace), nonAsciiSpace);
}

TEST(CoreDiagnosticsContractTest, NumericParsingPreservesOperationAndInputContext)
{
    const auto signedValue = parseInt64("-42", "frame.index");
    const auto unsignedValue = parseUInt64("184", "frame.count");
    const auto enabled = parseBool("true", "feature.enabled");
    const auto invalid = parseInt64("42ms", "timestamp");
    const auto invalidBool = parseBool("yes", "feature.enabled");

    ASSERT_TRUE(signedValue);
    EXPECT_EQ(signedValue.value(), -42);
    ASSERT_TRUE(unsignedValue);
    EXPECT_EQ(unsignedValue.value(), 184u);
    ASSERT_TRUE(enabled);
    EXPECT_TRUE(enabled.value());
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.errorContext().operation, "number.parseInt64");
    EXPECT_EQ(invalid.errorContext().objectId, "timestamp");
    EXPECT_TRUE(invalid.errorContext().location.hasValue());
    EXPECT_FALSE(invalidBool);
}

TEST(CoreDiagnosticsContractTest, NumericParsingRejectsOverflowNegativeUnsignedAndEmpty)
{
    const auto overflow = parseInt64("9223372036854775808", "frame.index");
    const auto negativeUnsigned = parseUInt64("-1", "frame.count");
    const auto emptySigned = parseInt64("", "frame.index");
    const auto emptyUnsigned = parseUInt64("", "frame.count");
    const auto emptyBool = parseBool("", "feature.enabled");

    EXPECT_FALSE(overflow);
    EXPECT_EQ(overflow.errorContext().operation, "number.parseInt64");
    EXPECT_EQ(overflow.errorContext().objectId, "frame.index");
    EXPECT_FALSE(negativeUnsigned);
    EXPECT_EQ(negativeUnsigned.errorContext().operation, "number.parseUInt64");
    EXPECT_FALSE(emptySigned);
    EXPECT_FALSE(emptyUnsigned);
    EXPECT_FALSE(emptyBool);
}

TEST(CoreDiagnosticsContractTest, BooleanParserAcceptsNumericAliases)
{
    const auto one = parseBool("1", "feature.enabled");
    const auto zero = parseBool("0", "feature.enabled");

    ASSERT_TRUE(one);
    EXPECT_TRUE(one.value());
    ASSERT_TRUE(zero);
    EXPECT_FALSE(zero.value());
}
