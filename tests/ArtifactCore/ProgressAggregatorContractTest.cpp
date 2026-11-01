#include <gtest/gtest.h>

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

import Render.Farm.Progress;
import Render.Farm.Types;

namespace {

using ArtifactCore::FarmProgressSnapshot;
using ArtifactCore::ProgressAggregator;
using ArtifactCore::WorkerProgressSnapshot;
using ArtifactCore::WorkerState;

auto findWorker(const FarmProgressSnapshot& snapshot, const QString& id)
    -> const WorkerProgressSnapshot*
{
    const auto it = std::find_if(snapshot.workers.begin(), snapshot.workers.end(),
        [&id](const auto& worker) { return worker.workerId == id; });
    return it == snapshot.workers.end() ? nullptr : &*it;
}

} // namespace

TEST(ProgressAggregatorContractTest, AggregatesWorkerFrameAndStateCounts)
{
    ProgressAggregator aggregator;
    EXPECT_EQ(aggregator.snapshot().totalWorkers, 0);
    EXPECT_FLOAT_EQ(aggregator.snapshot().overallProgress, 0.0f);

    aggregator.registerWorker(QStringLiteral("local"), true, 3);
    aggregator.registerWorker(QStringLiteral("remote"), false, 2);
    aggregator.reportFrameStarted(QStringLiteral("local"), 12);
    aggregator.reportFrameCompleted(QStringLiteral("local"), 12);
    aggregator.reportWorkerState(QStringLiteral("local"), WorkerState::Idle);
    aggregator.reportFrameFailed(QStringLiteral("remote"), 13,
                                 QStringLiteral("decode failed"));

    const auto snapshot = aggregator.snapshot();
    EXPECT_EQ(snapshot.totalFrames, 5);
    EXPECT_EQ(snapshot.totalCompletedFrames, 1);
    EXPECT_EQ(snapshot.totalFailedFrames, 1);
    EXPECT_EQ(snapshot.totalWorkers, 2);
    EXPECT_EQ(snapshot.activeWorkers, 0);
    EXPECT_EQ(snapshot.idleWorkers, 2);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 0.4f);

    const auto* local = findWorker(snapshot, QStringLiteral("local"));
    ASSERT_NE(local, nullptr);
    EXPECT_TRUE(local->isLocal);
    EXPECT_EQ(local->completedFrames, 1);
    EXPECT_EQ(local->currentFrame, -1);
    EXPECT_EQ(local->state, WorkerState::Idle);
    EXPECT_NEAR(local->progress(), 1.0f / 3.0f, 1.0e-6f);

    const auto* remote = findWorker(snapshot, QStringLiteral("remote"));
    ASSERT_NE(remote, nullptr);
    EXPECT_FALSE(remote->isLocal);
    EXPECT_EQ(remote->failedFrames, 1);
    EXPECT_EQ(remote->currentFrame, -1);
    EXPECT_EQ(remote->state, WorkerState::Idle);
    EXPECT_FLOAT_EQ(remote->progress(), 0.0f);
}

TEST(ProgressAggregatorContractTest, UnknownWorkersAreIgnoredAndRemovalRecalculatesTotals)
{
    ProgressAggregator aggregator;
    aggregator.reportFrameStarted(QStringLiteral("missing"), 8);
    aggregator.reportFrameCompleted(QStringLiteral("missing"), 8);
    aggregator.reportFrameFailed(QStringLiteral("missing"), 8, QStringLiteral("ignored"));
    aggregator.reportWorkerState(QStringLiteral("missing"), WorkerState::Rendering);
    aggregator.setTotalFrames(QStringLiteral("missing"), 20);
    EXPECT_EQ(aggregator.snapshot().totalWorkers, 0);

    aggregator.registerWorker(QStringLiteral("first"), false, 4);
    aggregator.registerWorker(QStringLiteral("second"), true, 6);
    aggregator.reportFrameCompleted(QStringLiteral("first"), 0);
    EXPECT_EQ(aggregator.snapshot().totalFrames, 10);
    EXPECT_EQ(aggregator.snapshot().totalCompletedFrames, 1);

    aggregator.unregisterWorker(QStringLiteral("first"));
    const auto remaining = aggregator.snapshot();
    EXPECT_EQ(remaining.totalWorkers, 1);
    EXPECT_EQ(remaining.totalFrames, 6);
    EXPECT_EQ(remaining.totalCompletedFrames, 0);
    EXPECT_EQ(remaining.idleWorkers, 1);

    aggregator.reset();
    const auto reset = aggregator.snapshot();
    EXPECT_EQ(reset.totalWorkers, 0);
    EXPECT_EQ(reset.totalFrames, 0);
    EXPECT_EQ(reset.totalCompletedFrames, 0);
    EXPECT_FLOAT_EQ(reset.overallProgress, 0.0f);
}

TEST(ProgressAggregatorContractTest, UpdatingWorkerTotalRecalculatesProgressWithoutLosingCompletions)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("dynamic"), true, 4);
    aggregator.reportFrameCompleted(QStringLiteral("dynamic"), 1);
    aggregator.reportFrameCompleted(QStringLiteral("dynamic"), 2);
    ASSERT_FLOAT_EQ(aggregator.snapshot().overallProgress, 0.5f);

    aggregator.setTotalFrames(QStringLiteral("dynamic"), 8);

    const auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.totalWorkers, 1);
    EXPECT_EQ(snapshot.totalFrames, 8);
    EXPECT_EQ(snapshot.totalCompletedFrames, 2);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 0.25f);
    const auto* worker = findWorker(snapshot, QStringLiteral("dynamic"));
    ASSERT_NE(worker, nullptr);
    EXPECT_EQ(worker->totalFrames, 8);
    EXPECT_EQ(worker->completedFrames, 2);
    EXPECT_FLOAT_EQ(worker->progress(), 0.25f);
}

TEST(ProgressAggregatorContractTest, RegisteringAnExistingWorkerIdReplacesItsEntry)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("worker"), true, 8);
    aggregator.reportFrameStarted(QStringLiteral("worker"), 17);
    aggregator.reportFrameCompleted(QStringLiteral("worker"), 16);

    aggregator.registerWorker(QStringLiteral("worker"), false, 3);

    const auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.totalWorkers, 1);
    EXPECT_EQ(snapshot.totalFrames, 3);
    EXPECT_EQ(snapshot.totalCompletedFrames, 0);
    EXPECT_EQ(snapshot.activeWorkers, 0);
    const auto* worker = findWorker(snapshot, QStringLiteral("worker"));
    ASSERT_NE(worker, nullptr);
    EXPECT_FALSE(worker->isLocal);
    EXPECT_EQ(worker->totalFrames, 3);
    EXPECT_EQ(worker->completedFrames, 0);
    EXPECT_EQ(worker->currentFrame, -1);
    EXPECT_EQ(worker->state, WorkerState::Idle);
}

TEST(ProgressAggregatorContractTest, FailedFramesCountAsProcessedWorkAndClearCurrentFrame)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("failing"), false, 4);
    aggregator.reportFrameStarted(QStringLiteral("failing"), 23);
    ASSERT_EQ(aggregator.snapshot().activeWorkers, 1);

    aggregator.reportFrameFailed(QStringLiteral("failing"), 23,
                                 QStringLiteral("render failed"));

    const auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.totalWorkers, 1);
    EXPECT_EQ(snapshot.totalFrames, 4);
    EXPECT_EQ(snapshot.totalCompletedFrames, 0);
    EXPECT_EQ(snapshot.totalFailedFrames, 1);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 0.25f);
    EXPECT_EQ(snapshot.activeWorkers, 0);
    EXPECT_EQ(snapshot.idleWorkers, 1);
    const auto* worker = findWorker(snapshot, QStringLiteral("failing"));
    ASSERT_NE(worker, nullptr);
    EXPECT_EQ(worker->currentFrame, -1);
    EXPECT_EQ(worker->failedFrames, 1);
    EXPECT_FLOAT_EQ(worker->progress(), 0.0f);
}

TEST(ProgressAggregatorContractTest, OverallProgressIncludesFailedFramesButWorkerProgressDoesNot)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("mixed-results"), true, 4);
    aggregator.reportFrameCompleted(QStringLiteral("mixed-results"), 10);
    aggregator.reportFrameFailed(QStringLiteral("mixed-results"), 11,
                                 QStringLiteral("decode failed"));

    const auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.workers.size(), 1u);
    EXPECT_EQ(snapshot.totalCompletedFrames, 1);
    EXPECT_EQ(snapshot.totalFailedFrames, 1);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 0.5f);
    EXPECT_EQ(snapshot.workers.front().completedFrames, 1);
    EXPECT_EQ(snapshot.workers.front().failedFrames, 1);
    EXPECT_FLOAT_EQ(snapshot.workers.front().progress(), 0.25f);
}

TEST(ProgressAggregatorContractTest, ZeroFrameWorkerKeepsProgressAtZeroAfterCompletion)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("zero-frame"), true, 0);
    aggregator.reportFrameCompleted(QStringLiteral("zero-frame"), 0);

    const auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.totalWorkers, 1);
    EXPECT_EQ(snapshot.totalFrames, 0);
    EXPECT_EQ(snapshot.totalCompletedFrames, 1);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 0.0f);
    EXPECT_TRUE(std::isfinite(snapshot.overallProgress));
    const auto* worker = findWorker(snapshot, QStringLiteral("zero-frame"));
    ASSERT_NE(worker, nullptr);
    EXPECT_EQ(worker->totalFrames, 0);
    EXPECT_EQ(worker->completedFrames, 1);
    EXPECT_FLOAT_EQ(worker->progress(), 0.0f);
    EXPECT_TRUE(std::isfinite(worker->progress()));
}

TEST(ProgressAggregatorContractTest, NegativeWorkerTotalsAreClampedToZero)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("negative-total"), true, -5);

    auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.workers.size(), 1u);
    EXPECT_EQ(snapshot.totalFrames, 0);
    EXPECT_EQ(snapshot.workers.front().totalFrames, 0);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 0.0f);

    aggregator.setTotalFrames(QStringLiteral("negative-total"), -2);
    snapshot = aggregator.snapshot();
    EXPECT_EQ(snapshot.totalFrames, 0);
    ASSERT_EQ(snapshot.workers.size(), 1u);
    EXPECT_EQ(snapshot.workers.front().totalFrames, 0);
}

TEST(ProgressAggregatorContractTest, ConcurrentFrameCompletionsAreCountedExactlyOnce)
{
    constexpr int threadCount = 4;
    constexpr int completionsPerThread = 100;
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("parallel"), false,
                              threadCount * completionsPerThread);

    std::vector<std::thread> reporters;
    reporters.reserve(threadCount);
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        reporters.emplace_back([&aggregator, threadIndex] {
            for (int completion = 0; completion < completionsPerThread; ++completion) {
                aggregator.reportFrameCompleted(
                    QStringLiteral("parallel"),
                    threadIndex * completionsPerThread + completion);
            }
        });
    }
    for (auto& reporter : reporters) {
        reporter.join();
    }

    const auto snapshot = aggregator.snapshot();
    ASSERT_EQ(snapshot.totalWorkers, 1);
    EXPECT_EQ(snapshot.totalCompletedFrames,
              threadCount * completionsPerThread);
    EXPECT_FLOAT_EQ(snapshot.overallProgress, 1.0f);
    const auto* worker = findWorker(snapshot, QStringLiteral("parallel"));
    ASSERT_NE(worker, nullptr);
    EXPECT_EQ(worker->completedFrames, threadCount * completionsPerThread);
    EXPECT_NE(worker->lastUpdated, QDateTime{});
}

TEST(ProgressAggregatorContractTest, ResetClearsStartTimeUntilAWorkerIsRegisteredAgain)
{
    ProgressAggregator aggregator;
    aggregator.registerWorker(QStringLiteral("reset-time"), true, 1);
    ASSERT_TRUE(aggregator.snapshot().startedAt.isValid());

    aggregator.reset();

    EXPECT_FALSE(aggregator.snapshot().startedAt.isValid());
    aggregator.registerWorker(QStringLiteral("after-reset"), false, 2);
    EXPECT_TRUE(aggregator.snapshot().startedAt.isValid());
}
