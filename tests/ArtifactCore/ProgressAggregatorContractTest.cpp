#include <gtest/gtest.h>

#include <QDateTime>
#include <QString>

#include <algorithm>
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
