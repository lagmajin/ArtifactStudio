#include <gtest/gtest.h>

#include <cstdint>
#include <string>

import ArtifactCore.Utils.PerformanceProfiler;

using namespace ArtifactCore;

TEST(PerformanceProfilerContractTest, StartupProfilerStartsEmptyAndCanBeCleared)
{
    auto& startup = StartupProfiler::instance();
    startup.clear();
    EXPECT_TRUE(startup.events().empty());
    EXPECT_DOUBLE_EQ(startup.totalMs(), 0.0);
    startup.recordEvent(String("stale"), StartupPhase::Custom, 1.0);
    startup.clear();
    EXPECT_TRUE(startup.events().empty());
}

TEST(PerformanceProfilerContractTest, RecordsStartupEventMetadataAndInsertionOrder)
{
    auto& startup = StartupProfiler::instance();
    startup.clear();
    startup.recordEvent(String("device"), StartupPhase::DeviceCreation, 4.5, 2, 7);
    startup.recordEvent(String("shaders"), StartupPhase::ShaderCompilation, 8.25, 4, 19);
    const auto events = startup.events();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].name, "device");
    EXPECT_EQ(events[0].phase, StartupPhase::DeviceCreation);
    EXPECT_DOUBLE_EQ(events[0].durationMs, 4.5);
    EXPECT_EQ(events[0].threadCount, 2);
    EXPECT_EQ(events[0].itemCount, 7);
    EXPECT_EQ(events[1].name, "shaders");
    EXPECT_EQ(events[1].phase, StartupPhase::ShaderCompilation);
}

TEST(PerformanceProfilerContractTest, StartupTotalSumsEventsWithoutTotalMarker)
{
    auto& startup = StartupProfiler::instance();
    startup.clear();
    startup.recordEvent(String("one"), StartupPhase::DeviceCreation, 1.25);
    startup.recordEvent(String("two"), StartupPhase::ShaderCompilation, 2.75);
    EXPECT_DOUBLE_EQ(startup.totalMs(), 4.0);
}

TEST(PerformanceProfilerContractTest, ExplicitTotalStartupEventOverridesAccumulatedSum)
{
    auto& startup = StartupProfiler::instance();
    startup.clear();
    startup.recordEvent(String("device"), StartupPhase::DeviceCreation, 3.0);
    startup.recordEvent(String("total"), StartupPhase::TotalStartup, 20.0);
    startup.recordEvent(String("late custom"), StartupPhase::Custom, 5.0);
    EXPECT_DOUBLE_EQ(startup.totalMs(), 20.0);
}

TEST(PerformanceProfilerContractTest, StartupReportContainsMetadataAndExcludesTotalFromSum)
{
    auto& startup = StartupProfiler::instance();
    startup.clear();
    startup.recordEvent(String("compile"), StartupPhase::ShaderCompilation, 2.0, 3, 11);
    startup.recordEvent(String("overall"), StartupPhase::TotalStartup, 9.0);
    const std::string report = toStdString(startup.generateReportZero());
    EXPECT_NE(report.find("compile: 2.000000 ms"), std::string::npos);
    EXPECT_NE(report.find("threads=3"), std::string::npos);
    EXPECT_NE(report.find("items=11"), std::string::npos);
    EXPECT_NE(report.find("overall: 9.000000 ms"), std::string::npos);
    EXPECT_NE(report.find("sum (excl. total): 2.000000 ms"), std::string::npos);
}

TEST(PerformanceProfilerContractTest, ScopedStartupTimerRecordsWhenScopeEnds)
{
    auto& startup = StartupProfiler::instance();
    startup.clear();
    {
        ScopedStartupTimer timer(String("load cache"), StartupPhase::PSOCacheLoad, 2, 5);
        EXPECT_TRUE(startup.events().empty());
    }
    const auto events = startup.events();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].name, "load cache");
    EXPECT_EQ(events[0].phase, StartupPhase::PSOCacheLoad);
    EXPECT_GE(events[0].durationMs, 0.0);
    EXPECT_EQ(events[0].threadCount, 2);
    EXPECT_EQ(events[0].itemCount, 5);
}

TEST(PerformanceProfilerContractTest, PerformanceRegistryKeepsLatestAndOrderedHistory)
{
    auto& registry = PerformanceRegistry::instance();
    const String name("standalone.registry.latest");
    registry.recordSample(name, 1.25);
    registry.recordSample(name, 3.5);
    const auto latest = registry.getLatestSamples();
    ASSERT_TRUE(latest.contains("standalone.registry.latest"));
    EXPECT_DOUBLE_EQ(latest.at("standalone.registry.latest").durationMs, 3.5);
    const auto history = registry.getHistory(name);
    ASSERT_GE(history.size(), 2u);
    EXPECT_DOUBLE_EQ(history[history.size() - 2], 1.25);
    EXPECT_DOUBLE_EQ(history.back(), 3.5);
}

TEST(PerformanceProfilerContractTest, UnknownTimerHistoryAndStatsAreEmpty)
{
    const String name("standalone.registry.unknown.timer");
    const auto history = PerformanceRegistry::instance().getHistory(name);
    EXPECT_TRUE(history.empty());

    const auto stats = Profiler::instance().timerStats(name, 100);
    EXPECT_EQ(stats.samples, 0);
    EXPECT_DOUBLE_EQ(stats.avgMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.maxMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.p95Ms, 0.0);
}

TEST(PerformanceProfilerContractTest, ZeroFrameHistoryProducesEmptyFrameAndScopeStats)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7250, 320, 240, false);
    EXPECT_TRUE(profiler.beginScope("zero.history.scope", ProfileCategory::Other));
    profiler.endScope();
    profiler.endFrame();

    const auto frameStats = profiler.computeFrameStats(0);
    const auto scopeStats = profiler.computeScopeStats(String("zero.history.scope"), 0);
    EXPECT_EQ(frameStats.samples, 0);
    EXPECT_DOUBLE_EQ(frameStats.avgMs, 0.0);
    EXPECT_EQ(scopeStats.samples, 0);
    EXPECT_DOUBLE_EQ(scopeStats.avgMs, 0.0);
}

TEST(PerformanceProfilerContractTest, PerformanceRegistryHistoryIsBoundedToOneHundredSamples)
{
    auto& registry = PerformanceRegistry::instance();
    const String name("standalone.registry.bound");
    for (int sample = 0; sample < 105; ++sample) {
        registry.recordSample(name, static_cast<double>(sample));
    }
    const auto history = registry.getHistory(name);
    ASSERT_EQ(history.size(), 100u);
    EXPECT_DOUBLE_EQ(history.front(), 5.0);
    EXPECT_DOUBLE_EQ(history.back(), 104.0);
}

TEST(PerformanceProfilerContractTest, ScopedPerformanceTimerFeedsRegistryStatistics)
{
    const String name("standalone.scoped.performance.timer");
    {
        ScopedPerformanceTimer timer(name);
    }
    const auto history = PerformanceRegistry::instance().getHistory(name);
    ASSERT_FALSE(history.empty());
    EXPECT_GE(history.back(), 0.0);
    const auto stats = Profiler::instance().timerStats(name, 1);
    EXPECT_GE(stats.samples, 1);
    EXPECT_GE(stats.avgMs, 0.0);
    EXPECT_GE(stats.maxMs, 0.0);
}

TEST(PerformanceProfilerContractTest, ProfilerRecordsFrameMetadataScopesAndEventPeaks)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7101, 1920, 1080, true);
    EXPECT_TRUE(profiler.beginScope("outer", ProfileCategory::Render));
    EXPECT_TRUE(profiler.beginScope("inner", ProfileCategory::Composite));
    profiler.endScope();
    profiler.endScope();
    profiler.recordEvent("asset.changed", 2);
    profiler.recordEvent("asset.changed", 7);
    profiler.recordEvent("asset.changed", 4);
    profiler.endFrame();

    const auto frame = profiler.lastFrameSnapshot();
    EXPECT_EQ(frame.frameIndex, 7101);
    EXPECT_EQ(frame.canvasWidth, 1920);
    EXPECT_EQ(frame.canvasHeight, 1080);
    EXPECT_TRUE(frame.isPlayback);
    ASSERT_EQ(frame.scopes.size(), 2u);
    EXPECT_EQ(frame.scopes[0].name, "inner");
    EXPECT_EQ(frame.scopes[0].depth, 1);
    EXPECT_EQ(frame.scopes[1].name, "outer");
    EXPECT_EQ(frame.scopes[1].depth, 0);
    EXPECT_EQ(frame.eventCounts.at("asset.changed"), 3);
    EXPECT_EQ(frame.eventSubscriberPeak.at("asset.changed"), 7);
}

TEST(PerformanceProfilerContractTest, EndingScopeWithoutActiveScopeIsIgnored)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7150, 640, 480, false);
    profiler.endScope();
    profiler.endFrame();

    const auto frame = profiler.lastFrameSnapshot();
    EXPECT_EQ(frame.frameIndex, 7150);
    EXPECT_TRUE(frame.scopes.empty());
}

TEST(PerformanceProfilerContractTest, EventCountsIncludeZeroSubscribersAndPeakNeverGoesNegative)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7160, 640, 480, false);
    profiler.recordEvent("subscriber.edge", -3);
    profiler.recordEvent("subscriber.edge", 0);
    profiler.endFrame();

    const auto frame = profiler.lastFrameSnapshot();
    EXPECT_EQ(frame.eventCounts.at("subscriber.edge"), 2);
    EXPECT_EQ(frame.eventSubscriberPeak.at("subscriber.edge"), 0);
}

TEST(PerformanceProfilerContractTest, FrameHistoryReturnsNewestRequestedFrames)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    for (std::int64_t index = 0; index < 3; ++index) {
        profiler.beginFrame(7200 + index, 640, 480, false);
        profiler.endFrame();
    }
    const auto latest = profiler.frameHistory(2);
    ASSERT_EQ(latest.size(), 2u);
    EXPECT_EQ(latest[0].frameIndex, 7201);
    EXPECT_EQ(latest[1].frameIndex, 7202);
    EXPECT_TRUE(profiler.frameHistory(0).empty());
    EXPECT_TRUE(profiler.frameHistory(-1).empty());
}

TEST(PerformanceProfilerContractTest, DisabledProfilerDoesNotAppendFramesOrScopes)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7300, 320, 240, false);
    profiler.endFrame();
    const auto before = profiler.lastFrameSnapshot();

    profiler.setEnabled(false);
    profiler.beginFrame(7301, 1, 1, true);
    EXPECT_FALSE(profiler.beginScope("disabled", ProfileCategory::UI));
    profiler.recordEvent("disabled.event", 10);
    profiler.endFrame();
    profiler.setEnabled(true);

    EXPECT_EQ(profiler.lastFrameSnapshot().frameIndex, before.frameIndex);
}

TEST(PerformanceProfilerContractTest, FrameAndScopeStatsSummarizeRecordedSamples)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7400, 800, 600, false);
    EXPECT_TRUE(profiler.beginScope("stats.scope.unique", ProfileCategory::Other));
    profiler.endScope();
    profiler.endFrame();

    const auto frameStats = profiler.computeFrameStats(1);
    const auto scopeStats = profiler.computeScopeStats(String("stats.scope.unique"), 1);
    EXPECT_EQ(frameStats.samples, 1);
    EXPECT_GE(frameStats.avgMs, 0.0);
    EXPECT_GE(frameStats.maxMs, 0.0);
    EXPECT_EQ(scopeStats.samples, 1);
    EXPECT_GE(scopeStats.avgMs, 0.0);
    EXPECT_GE(scopeStats.p95Ms, 0.0);
}

TEST(PerformanceProfilerContractTest, DiagnosticReportIncludesFrameAndTimerData)
{
    auto& profiler = Profiler::instance();
    profiler.setEnabled(true);
    profiler.beginFrame(7500, 100, 200, false);
    profiler.recordEvent("report.event", 1);
    profiler.endFrame();
    PerformanceRegistry::instance().recordSample(String("report.timer.unique"), 4.0);
    const std::string report = toStdString(profiler.generateDiagnosticReportZero(1));
    EXPECT_NE(report.find("Profiler report"), std::string::npos);
    EXPECT_NE(report.find("report.timer.unique"), std::string::npos);
    EXPECT_NE(report.find("events=1"), std::string::npos);
}

TEST(PerformanceProfilerContractTest, ScopeAndStartupCategoriesPreserveEnumValues)
{
    EXPECT_EQ(static_cast<std::uint8_t>(ProfileCategory::Render), 0);
    EXPECT_EQ(static_cast<std::uint8_t>(ProfileCategory::Audio), 7);
    EXPECT_EQ(static_cast<std::uint8_t>(StartupPhase::DeviceCreation), 0);
    EXPECT_EQ(static_cast<std::uint8_t>(StartupPhase::Custom), 9);
}

TEST(PerformanceProfilerContractTest, AudioProfilerAggregatesCallbacksAndFillLoops)
{
    auto& audio = AudioEngineProfiler::instance();
    audio.resetStats();
    audio.recordCallback(1'000'000, 512, 512);
    audio.recordCallback(3'000'000, 1024, 900);
    audio.recordFillLoop(2'000'000);
    audio.recordFillLoop(4'000'000);
    audio.setBufferLevel(27.5);

    const AudioCallbackStats stats = audio.snapshot();
    EXPECT_EQ(stats.totalCallbacks, 2);
    EXPECT_EQ(stats.totalUnderflows, 1);
    EXPECT_EQ(stats.totalFillLoops, 2);
    EXPECT_DOUBLE_EQ(stats.avgCallbackUs, 2.0);
    EXPECT_DOUBLE_EQ(stats.maxCallbackUs, 3.0);
    EXPECT_DOUBLE_EQ(stats.avgFillMs, 3.0);
    EXPECT_DOUBLE_EQ(stats.maxFillMs, 4.0);
    EXPECT_DOUBLE_EQ(stats.bufferLevelPct, 27.5);
    EXPECT_EQ(stats.lastCallbackFrames, 900);
    EXPECT_EQ(stats.lastRequestedFrames, 1024);
}

TEST(PerformanceProfilerContractTest, AudioProfilerResetClearsAggregateCountersAndLevel)
{
    auto& audio = AudioEngineProfiler::instance();
    audio.resetStats();
    audio.recordCallback(2'000'000, 256, 128);
    audio.recordFillLoop(5'000'000);
    audio.setBufferLevel(64.0);
    audio.resetStats();

    const AudioCallbackStats stats = audio.snapshot();
    EXPECT_EQ(stats.totalCallbacks, 0);
    EXPECT_EQ(stats.totalUnderflows, 0);
    EXPECT_EQ(stats.totalFillLoops, 0);
    EXPECT_DOUBLE_EQ(stats.avgCallbackUs, 0.0);
    EXPECT_DOUBLE_EQ(stats.maxCallbackUs, 0.0);
    EXPECT_DOUBLE_EQ(stats.avgFillMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.maxFillMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.bufferLevelPct, 0.0);
}

TEST(PerformanceProfilerContractTest, AudioProfilerClampsNegativeBufferLevelToZero)
{
    auto& audio = AudioEngineProfiler::instance();
    audio.resetStats();
    audio.setBufferLevel(-12.0);
    EXPECT_DOUBLE_EQ(audio.snapshot().bufferLevelPct, 0.0);
}

TEST(PerformanceProfilerContractTest, AudioProfilerClampsBufferLevelAboveOneHundredPercent)
{
    auto& audio = AudioEngineProfiler::instance();
    audio.resetStats();
    audio.setBufferLevel(125.0);

    EXPECT_DOUBLE_EQ(audio.snapshot().bufferLevelPct, 100.0);
}

TEST(PerformanceProfilerContractTest, AudioProfilerResetClearsLastCallbackFrameMetadata)
{
    auto& audio = AudioEngineProfiler::instance();
    audio.resetStats();
    audio.recordCallback(1'000'000, 512, 480);
    ASSERT_EQ(audio.snapshot().lastCallbackFrames, 480);
    ASSERT_EQ(audio.snapshot().lastRequestedFrames, 512);

    audio.resetStats();

    const AudioCallbackStats stats = audio.snapshot();
    EXPECT_EQ(stats.lastCallbackFrames, 0);
    EXPECT_EQ(stats.lastRequestedFrames, 0);
}

