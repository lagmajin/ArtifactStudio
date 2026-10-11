#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

import Core.Diagnostics.CrashReportParser;

using ArtifactCore::CoreDiagnosticSeverity;
using ArtifactCore::CrashReportSummary;
using ArtifactCore::ErrorCode;
using ArtifactCore::parseCrashReport;
using ArtifactCore::parseCrashReportFile;
using ArtifactCore::crashReportToSnapshot;
using ArtifactCore::loadCrashReportSnapshot;
using ArtifactCore::toStdString;

namespace {

constexpr std::string_view kReportHeader = "=== Artifact Crash Report ===";

auto fixturePath() -> std::filesystem::path
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("artifact-crash-report-" + std::to_string(stamp) + ".txt");
}

class TemporaryReport {
public:
    explicit TemporaryReport(std::string_view contents)
        : path_(fixturePath())
    {
        std::ofstream file(path_, std::ios::binary);
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        written_ = file.good();
    }

    ~TemporaryReport()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    auto path() const -> std::string { return path_.string(); }
    auto written() const -> bool { return written_; }

private:
    std::filesystem::path path_;
    bool written_ = false;
};

auto fullReport(std::string_view exceptionFields =
                    "Code: 0xC0000005\nType: AccessViolation\nOperation: renderFrame\n"
                    "Address: 0x1111\nException Address: 0x2222\n",
                std::string_view stack = "frame 0\nframe 1\n",
                std::string_view system = "OS: TestOS\nCPU: TestCPU\n") -> std::string
{
    std::string report(kReportHeader);
    report += "\nTimestamp: 2026-10-11T10:20:30Z\n--- Exception ---\n";
    report += exceptionFields;
    report += "--- Stack Trace ---\n";
    report += stack;
    report += "--- System Info ---\n";
    report += system;
    return report;
}

} // namespace

TEST(CrashReportParserStandaloneTest, EmptyReportReturnsDefaultUnparsedSummary)
{
    const auto report = parseCrashReport({});
    EXPECT_FALSE(report.parsed);
    EXPECT_TRUE(report.timestamp.isEmpty());
    EXPECT_TRUE(report.exceptionCode.isEmpty());
    EXPECT_TRUE(report.stackTrace.isEmpty());
}

TEST(CrashReportParserStandaloneTest, RecognizedHeaderMarksReportParsed)
{
    const auto report = parseCrashReport(kReportHeader);
    EXPECT_TRUE(report.parsed);
}

TEST(CrashReportParserStandaloneTest, HeaderMayAppearAfterOtherText)
{
    const auto report = parseCrashReport("prefix\n" + std::string(kReportHeader) + "\nsuffix");
    EXPECT_TRUE(report.parsed);
}

TEST(CrashReportParserStandaloneTest, ParsesTimestampFromHeaderSection)
{
    const auto report = parseCrashReport(fullReport());
    EXPECT_EQ(toStdString(report.timestamp), "2026-10-11T10:20:30Z");
}

TEST(CrashReportParserStandaloneTest, ParsesExceptionFieldsOnlyInExceptionSection)
{
    const auto report = parseCrashReport(fullReport());
    EXPECT_EQ(toStdString(report.exceptionCode), "0xC0000005");
    EXPECT_EQ(toStdString(report.exceptionType), "AccessViolation");
    EXPECT_EQ(toStdString(report.operation), "renderFrame");
    EXPECT_EQ(toStdString(report.address), "0x1111");
    EXPECT_EQ(toStdString(report.exceptionAddress), "0x2222");
}

TEST(CrashReportParserStandaloneTest, ExceptionAddressIsKeptSeparateFromAddress)
{
    const auto report = parseCrashReport(fullReport(
        "Address: instruction\nException Address: fault\n", "", ""));
    EXPECT_EQ(toStdString(report.address), "instruction");
    EXPECT_EQ(toStdString(report.exceptionAddress), "fault");
}

TEST(CrashReportParserStandaloneTest, RemovesLeadingWhitespaceAfterFieldKey)
{
    const auto report = parseCrashReport(fullReport("Code:\t 0x1234\n", "", ""));
    EXPECT_EQ(toStdString(report.exceptionCode), "0x1234");
}

TEST(CrashReportParserStandaloneTest, PreservesStackLinesAndJoinsThemWithNewlines)
{
    const auto report = parseCrashReport(fullReport("", "first frame\nsecond frame\n", ""));
    EXPECT_EQ(toStdString(report.stackTrace), "first frame\nsecond frame");
}

TEST(CrashReportParserStandaloneTest, PreservesSystemInfoLinesAndJoinsThemWithNewlines)
{
    const auto report = parseCrashReport(fullReport("", "", "OS: Example\nCPU: ExampleCPU\n"));
    EXPECT_EQ(toStdString(report.systemInfo), "OS: Example\nCPU: ExampleCPU");
}

TEST(CrashReportParserStandaloneTest, IgnoresEmptyLinesInsideMultilineSections)
{
    const auto report = parseCrashReport(fullReport("", "frame 0\n\nframe 1\n", ""));
    EXPECT_EQ(toStdString(report.stackTrace), "frame 0\nframe 1");
}

TEST(CrashReportParserStandaloneTest, ParsesCrLfInputAndUnterminatedFinalLine)
{
    const auto report = parseCrashReport(
        "=== Artifact Crash Report ===\r\nTimestamp: today\r\n"
        "--- Exception ---\r\nCode: code\r\n--- Stack Trace ---\r\nlast frame");
    EXPECT_TRUE(report.parsed);
    EXPECT_EQ(toStdString(report.timestamp), "today");
    EXPECT_EQ(toStdString(report.exceptionCode), "code");
    EXPECT_EQ(toStdString(report.stackTrace), "last frame");
}

TEST(CrashReportParserStandaloneTest, DoesNotTreatExceptionFieldsOutsideSectionAsExceptionData)
{
    const auto report = parseCrashReport("Code: outside\n--- System Info ---\nCode: also outside\n");
    EXPECT_TRUE(report.exceptionCode.isEmpty());
}

TEST(CrashReportParserStandaloneTest, EmptyFilePathReturnsInvalidArgument)
{
    const auto result = parseCrashReportFile({});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::InvalidArgument);
    EXPECT_EQ(result.errorContext().operation, "parseCrashReportFile");
}

TEST(CrashReportParserStandaloneTest, MissingFileReturnsNotFound)
{
    const auto path = fixturePath().string();
    const auto result = parseCrashReportFile(path);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::NotFound);
    EXPECT_EQ(result.errorContext().objectId, path);
}

TEST(CrashReportParserStandaloneTest, UnrecognizedFileReturnsInvalidArgument)
{
    TemporaryReport fixture("not a crash report\nCode: ignored\n");
    ASSERT_TRUE(fixture.written());
    const auto result = parseCrashReportFile(fixture.path());
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::InvalidArgument);
}

TEST(CrashReportParserStandaloneTest, FileParserReturnsParsedSummary)
{
    TemporaryReport fixture(fullReport());
    ASSERT_TRUE(fixture.written());
    const auto result = parseCrashReportFile(fixture.path());
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().parsed);
    EXPECT_EQ(toStdString(result.value().exceptionType), "AccessViolation");
}

TEST(CrashReportParserStandaloneTest, SnapshotUsesCrashFieldsAndFatalSeverity)
{
    const auto summary = parseCrashReport(fullReport());
    const auto snapshot = crashReportToSnapshot(summary);
    ASSERT_EQ(snapshot.recentEvents.size(), 1u);
    EXPECT_EQ(snapshot.component, "CrashHandler");
    EXPECT_EQ(snapshot.objectId, "0x2222");
    EXPECT_EQ(snapshot.state, "AccessViolation");
    EXPECT_EQ(snapshot.lastOperation, "renderFrame");
    EXPECT_EQ(snapshot.recentEvents[0].severity, CoreDiagnosticSeverity::Fatal);
    EXPECT_EQ(snapshot.recentEvents[0].code, "crash.0xC0000005");
    EXPECT_EQ(snapshot.recentEvents[0].operation, "renderFrame");
    EXPECT_EQ(snapshot.recentEvents[0].objectId, "0x2222");
}

TEST(CrashReportParserStandaloneTest, SnapshotAppendsStackTraceToEventMessage)
{
    const auto summary = parseCrashReport(fullReport({}, "frame one\nframe two\n", ""));
    const auto snapshot = crashReportToSnapshot(summary);
    ASSERT_EQ(snapshot.recentEvents.size(), 1u);
    EXPECT_EQ(snapshot.recentEvents[0].message,
              "AccessViolation\nframe one\nframe two");
}

TEST(CrashReportParserStandaloneTest, SnapshotUsesFallbacksForMissingExceptionFields)
{
    CrashReportSummary summary;
    const auto snapshot = crashReportToSnapshot(summary);
    ASSERT_EQ(snapshot.recentEvents.size(), 1u);
    EXPECT_TRUE(snapshot.objectId.empty());
    EXPECT_TRUE(snapshot.state.empty());
    EXPECT_TRUE(snapshot.lastOperation.empty());
    EXPECT_EQ(snapshot.recentEvents[0].code, "crash.unknown");
    EXPECT_EQ(snapshot.recentEvents[0].message, "Unhandled exception");
    EXPECT_EQ(snapshot.recentEvents[0].operation, "unhandledException");
}

TEST(CrashReportParserStandaloneTest, LoadSnapshotPropagatesFileErrors)
{
    const auto result = loadCrashReportSnapshot({});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::InvalidArgument);
    EXPECT_EQ(result.errorContext().operation, "parseCrashReportFile");
}

TEST(CrashReportParserStandaloneTest, LoadSnapshotBuildsFatalEventFromFile)
{
    TemporaryReport fixture(fullReport());
    ASSERT_TRUE(fixture.written());
    const auto result = loadCrashReportSnapshot(fixture.path());
    ASSERT_TRUE(result);
    ASSERT_EQ(result.value().recentEvents.size(), 1u);
    EXPECT_EQ(result.value().recentEvents[0].severity, CoreDiagnosticSeverity::Fatal);
    EXPECT_EQ(result.value().recentEvents[0].code, "crash.0xC0000005");
}
