#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <limits>
#include <mutex>
#include <vector>

#include <QString>

import Render.Farm.Master;
import Render.Farm.Types;

using namespace ArtifactCore;

TEST(RenderFarmMasterRetryContractTest,
     MoreWorkersThanFramesStillRendersEveryScheduledFrameExactlyOnce)
{
    std::mutex resultMutex;
    std::condition_variable resultChanged;
    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> completedResults;
    RenderFarmMaster master(4);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(resultMutex);
            completedResults.push_back(result);
        }
        resultChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("more-workers-than-frames-contract-job");
    request.range = {20, 22, 1};
    request.renderFrame = [&](int frame) {
        std::lock_guard lock(resultMutex);
        renderedFrames.push_back(frame);
        return true;
    };

    master.submitJob(request);
    {
        std::unique_lock lock(resultMutex);
        ASSERT_TRUE(resultChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return !completedResults.empty(); }));
        ASSERT_EQ(completedResults.size(), 1u);
        EXPECT_TRUE(completedResults.front().success);
        EXPECT_EQ(completedResults.front().renderedFrames, 2);
        std::sort(renderedFrames.begin(), renderedFrames.end());
        EXPECT_EQ(renderedFrames, (std::vector<int>{20, 21}));
    }
}

TEST(RenderFarmMasterRetryContractTest,
     MultiWorkerSteppedRangeNearIntMaximumKeepsEveryFrameInBounds)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> completedResults;
    RenderFarmMaster master(2);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            completedResults.push_back(result);
        }
        stateChanged.notify_all();
    });

    const int exclusiveEnd = std::numeric_limits<int>::max();
    RenderJobRequest request;
    request.jobId = QStringLiteral("stepped-range-int-maximum-contract-job");
    request.range = {exclusiveEnd - 5, exclusiveEnd, 2};
    request.renderFrame = [&](int frame) {
        std::lock_guard lock(stateMutex);
        renderedFrames.push_back(frame);
        return true;
    };

    master.submitJob(request);
    {
        std::unique_lock lock(stateMutex);
        ASSERT_TRUE(stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return !completedResults.empty(); }));
        ASSERT_EQ(completedResults.size(), 1u);
        EXPECT_TRUE(completedResults.front().success)
            << completedResults.front().errorMessage.toStdString();
        EXPECT_EQ(completedResults.front().renderedFrames, 3);
        EXPECT_EQ(completedResults.front().failedFrames, 0);
        std::sort(renderedFrames.begin(), renderedFrames.end());
        EXPECT_EQ(renderedFrames,
                  (std::vector<int>{exclusiveEnd - 5, exclusiveEnd - 3,
                                    exclusiveEnd - 1}));
    }
}

TEST(RenderFarmMasterRetryContractTest,
     BackoffStrategiesClampAttemptsFactorsAndArithmeticOverflow)
{
    RetryPolicy fixed;
    fixed.strategy = RetryBackoffStrategy::Fixed;
    fixed.initialBackoffMs = 25;
    fixed.maxBackoffMs = 100;
    EXPECT_EQ(fixed.backoffMs(0), 0);
    EXPECT_EQ(fixed.backoffMs(1), 0);
    EXPECT_EQ(fixed.backoffMs(2), 25);
    EXPECT_EQ(fixed.backoffMs(std::numeric_limits<int>::max()), 25);

    RetryPolicy linear;
    linear.strategy = RetryBackoffStrategy::Linear;
    linear.initialBackoffMs = 20;
    linear.maxBackoffMs = 100;
    EXPECT_EQ(linear.backoffMs(2), 20);
    EXPECT_EQ(linear.backoffMs(5), 80);
    EXPECT_EQ(linear.backoffMs(6), 100);
    EXPECT_EQ(linear.backoffMs(std::numeric_limits<int>::max()), 100);

    RetryPolicy exponential;
    exponential.strategy = RetryBackoffStrategy::Exponential;
    exponential.initialBackoffMs = 10;
    exponential.maxBackoffMs = 100;
    exponential.backoffFactor = 2.0;
    EXPECT_EQ(exponential.backoffMs(2), 10);
    EXPECT_EQ(exponential.backoffMs(3), 20);
    EXPECT_EQ(exponential.backoffMs(5), 80);
    EXPECT_EQ(exponential.backoffMs(6), 100);
    EXPECT_EQ(exponential.backoffMs(std::numeric_limits<int>::max()), 100);

    exponential.backoffFactor = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(exponential.backoffMs(4), 10);
    exponential.backoffFactor = std::numeric_limits<double>::infinity();
    EXPECT_EQ(exponential.backoffMs(4), 10);

    exponential.backoffFactor = 1.5;
    EXPECT_EQ(exponential.backoffMs(3), 10)
        << "fractional exponential delays truncate to whole milliseconds";
    EXPECT_EQ(exponential.backoffMs(4), 15);
    exponential.backoffFactor = 1.0;
    EXPECT_EQ(exponential.backoffMs(5), 10)
        << "a factor at or below one falls back to a constant delay";
    exponential.backoffFactor = 0.25;
    EXPECT_EQ(exponential.backoffMs(5), 10);

    exponential.initialBackoffMs = 250;
    exponential.maxBackoffMs = 40;
    EXPECT_EQ(exponential.backoffMs(2), 40)
        << "the configured cap also clamps the initial delay";
    EXPECT_EQ(exponential.backoffMs(std::numeric_limits<int>::max()), 40);

    linear.initialBackoffMs = -50;
    EXPECT_EQ(linear.backoffMs(2), 0);
    linear.initialBackoffMs = 50;
    linear.maxBackoffMs = -1;
    EXPECT_EQ(linear.backoffMs(2), 0);
}

TEST(RenderFarmMasterRetryContractTest,
     ParallelRetriesCountAttemptsAndKeepOnlyTerminalFailures)
{
    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::array<std::atomic<int>, 4> attempts{};
    RenderFarmMaster master(4);
    RetryPolicy retryPolicy;
    retryPolicy.maxAttempts = 3;
    retryPolicy.initialBackoffMs = 0;
    retryPolicy.maxBackoffMs = 0;
    master.setRetryPolicy(retryPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(completionMutex);
            completedResults.push_back(result);
        }
        completionChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("retry-contract-job");
    request.range = {0, 4, 1};
    request.renderFrame = [&attempts](int frame) {
        const int attempt = attempts[static_cast<std::size_t>(frame)]
                                .fetch_add(1, std::memory_order_relaxed) + 1;
        switch (frame) {
        case 0: return attempt >= 3; // two failures, then recovery
        case 1: return false;        // exhaust all attempts
        case 2: return true;         // succeeds immediately
        case 3: return attempt >= 2; // one failure, then recovery
        default: return false;
        }
    };

    master.submitJob(request);
    {
        std::unique_lock lock(completionMutex);
        ASSERT_TRUE(completionChanged.wait_for(lock, std::chrono::seconds(10),
            [&completedResults] { return !completedResults.empty(); }));
        ASSERT_EQ(completedResults.size(), 1u);
        const auto& result = completedResults.front();
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.renderedFrames, 3);
        EXPECT_EQ(result.failedFrames, 1);
        EXPECT_EQ(attempts[0].load(), 3);
        EXPECT_EQ(attempts[1].load(), 3);
        EXPECT_EQ(attempts[2].load(), 1);
        EXPECT_EQ(attempts[3].load(), 2);

        EXPECT_EQ(result.failures.totalRetries, 5);
        ASSERT_EQ(result.failures.failedFrames.size(), 1u);
        EXPECT_EQ(result.failures.failedFrames.front().frame, 1);
        EXPECT_EQ(result.failures.failedFrames.front().attempt, 3);
        EXPECT_TRUE(result.failures.failedFrames.front().held);
        EXPECT_EQ(result.failures.heldCount(), 1);
    }
}

TEST(RenderFarmMasterRetryContractTest,
     ResubmittingTerminalFailureCreatesFreshFullRangeAttempt)
{
    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    std::array<std::atomic<int>, 3> attempts{};

    RenderFarmMaster master(1);
    RetryPolicy retryPolicy;
    retryPolicy.maxAttempts = 1;
    retryPolicy.initialBackoffMs = 0;
    retryPolicy.maxBackoffMs = 0;
    master.setRetryPolicy(retryPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(completionMutex);
            completedResults.push_back(result);
        }
        completionChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("terminal-failure-resubmit-source");
    request.range = {3, 6, 1};
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(completionMutex);
            renderedFrames.push_back(frame);
        }
        const int attempt = attempts[static_cast<std::size_t>(frame - 3)]
                                .fetch_add(1, std::memory_order_relaxed) + 1;
        return frame != 4 || attempt > 1;
    };

    const auto waitForCompletions = [&](std::size_t expectedCount) {
        std::unique_lock lock(completionMutex);
        return completionChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return completedResults.size() >= expectedCount; });
    };

    master.submitJob(request);
    ASSERT_TRUE(waitForCompletions(1));
    {
        std::lock_guard lock(completionMutex);
        ASSERT_EQ(completedResults.size(), 1u);
        EXPECT_FALSE(completedResults[0].success);
        EXPECT_EQ(completedResults[0].renderedFrames, 2);
        EXPECT_EQ(completedResults[0].failedFrames, 1);
        ASSERT_TRUE(completedResults[0].failures.isFrameHeld(4));
        EXPECT_EQ(renderedFrames, (std::vector<int>{3, 4, 5}));
    }

    ASSERT_TRUE(master.resubmitJob(request.jobId));
    ASSERT_TRUE(waitForCompletions(2));
    {
        std::lock_guard lock(completionMutex);
        ASSERT_EQ(completedResults.size(), 2u);
        EXPECT_TRUE(completedResults[1].success)
            << completedResults[1].errorMessage.toStdString();
        EXPECT_EQ(completedResults[1].renderedFrames, 3);
        EXPECT_EQ(completedResults[1].failedFrames, 0);
        EXPECT_TRUE(completedResults[1].failures.failedFrames.empty());
        EXPECT_EQ(renderedFrames, (std::vector<int>{3, 4, 5, 3, 4, 5}));
        EXPECT_EQ(attempts[0].load(), 2);
        EXPECT_EQ(attempts[1].load(), 2);
        EXPECT_EQ(attempts[2].load(), 2);
    }
    EXPECT_FALSE(master.resubmitJob(QStringLiteral("missing-job")));
}

TEST(RenderFarmMasterRetryContractTest,
     CancellingAnInFlightFailedFramePreventsItsRetry)
{
    std::mutex frameMutex;
    std::condition_variable frameChanged;
    bool frameEntered = false;
    bool releaseFrame = false;
    std::atomic<int> attempts{0};

    RenderFarmMaster master(1);
    RetryPolicy retryPolicy;
    retryPolicy.maxAttempts = 5;
    retryPolicy.initialBackoffMs = 0;
    retryPolicy.maxBackoffMs = 0;
    master.setRetryPolicy(retryPolicy);
    RenderJobRequest request;
    request.jobId = QStringLiteral("cancel-in-flight-retry-contract");
    request.range = {0, 1, 1};
    request.renderFrame = [&](int) {
        attempts.fetch_add(1, std::memory_order_relaxed);
        std::unique_lock lock(frameMutex);
        frameEntered = true;
        frameChanged.notify_all();
        frameChanged.wait(lock, [&] { return releaseFrame; });
        return false;
    };

    master.submitJob(request);
    bool entered = false;
    {
        std::unique_lock lock(frameMutex);
        entered = frameChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return frameEntered; });
    }
    if (!entered) {
        master.cancelAll();
        {
            std::lock_guard lock(frameMutex);
            releaseFrame = true;
        }
        frameChanged.notify_all();
        EXPECT_TRUE(entered) << "the first frame attempt must enter before cancellation";
        return;
    }

    EXPECT_TRUE(master.cancelJob(request.jobId));
    {
        std::lock_guard lock(frameMutex);
        releaseFrame = true;
    }
    frameChanged.notify_all();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (master.isBusy() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    EXPECT_FALSE(master.isBusy());
    EXPECT_EQ(attempts.load(std::memory_order_relaxed), 1);
}

TEST(RenderFarmMasterRetryContractTest,
     PauseBetweenFailedAttemptAndRetryResumesTheSameFrame)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    int attempts = 0;
    bool firstFailureReturned = false;
    std::vector<RenderJobResult> completedResults;

    RenderFarmMaster master(1);
    RetryPolicy retryPolicy;
    retryPolicy.maxAttempts = 2;
    retryPolicy.initialBackoffMs = 0;
    retryPolicy.maxBackoffMs = 0;
    master.setRetryPolicy(retryPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            completedResults.push_back(result);
        }
        stateChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("pause-between-retry-attempts-contract");
    request.range = {8, 9, 1};
    request.renderFrame = [&](int frame) {
        EXPECT_EQ(frame, 8);
        int currentAttempt = 0;
        {
            std::lock_guard lock(stateMutex);
            currentAttempt = ++attempts;
        }
        stateChanged.notify_all();
        if (currentAttempt == 1) {
            master.pause();
            {
                std::lock_guard lock(stateMutex);
                firstFailureReturned = true;
            }
            stateChanged.notify_all();
            return false;
        }
        return true;
    };

    master.submitJob(request);
    bool firstAttemptObserved = false;
    {
        std::unique_lock lock(stateMutex);
        firstAttemptObserved = stateChanged.wait_for(
            lock, std::chrono::seconds(5), [&] { return firstFailureReturned; });
    }
    if (!firstAttemptObserved) {
        master.cancelAll();
        EXPECT_TRUE(firstAttemptObserved)
            << "the first failed attempt should pause before retry";
        return;
    }

    bool retriedWhilePaused = false;
    {
        std::unique_lock lock(stateMutex);
        retriedWhilePaused = stateChanged.wait_for(
            lock, std::chrono::milliseconds(100), [&] { return attempts > 1; });
    }
    EXPECT_TRUE(master.isPaused());
    EXPECT_FALSE(retriedWhilePaused);
    {
        std::lock_guard lock(stateMutex);
        EXPECT_EQ(attempts, 1);
    }

    master.resume();
    bool completed = false;
    {
        std::unique_lock lock(stateMutex);
        completed = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return completedResults.size() == 1; });
        if (completed) {
            EXPECT_TRUE(completedResults.front().success)
                << completedResults.front().errorMessage.toStdString();
            EXPECT_EQ(completedResults.front().renderedFrames, 1);
            EXPECT_EQ(completedResults.front().failedFrames, 0);
            EXPECT_TRUE(completedResults.front().failures.failedFrames.empty());
            EXPECT_EQ(attempts, 2);
        }
    }
    if (!completed) master.cancelAll();
    EXPECT_TRUE(completed);
    EXPECT_FALSE(master.isPaused());
}
