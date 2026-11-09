#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <QString>
#include <QTemporaryDir>

import Render.Farm.Master;
import Render.Farm.Types;

using namespace ArtifactCore;

TEST(RenderFarmMasterPauseContractTest,
     ProgressCallbackCanReplaceItselfForLaterFrames)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    int originalCallbackCalls = 0;
    int replacementCallbackCalls = 0;
    std::vector<RenderJobResult> completedResults;

    RenderFarmMaster master(1);
    master.setOnProgress([&](const RenderJobProgress&) {
        {
            std::lock_guard lock(stateMutex);
            ++originalCallbackCalls;
        }
        master.setOnProgress([&](const RenderJobProgress&) {
            {
                std::lock_guard lock(stateMutex);
                ++replacementCallbackCalls;
            }
            stateChanged.notify_all();
        });
        stateChanged.notify_all();
    });
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            completedResults.push_back(result);
        }
        stateChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("replace-progress-callback-contract-job");
    request.range = {0, 2, 1};
    request.renderFrame = [](int) { return true; };
    master.submitJob(request);

    bool completed = false;
    {
        std::unique_lock lock(stateMutex);
        completed = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return completedResults.size() == 1; });
        if (completed) {
            EXPECT_TRUE(completedResults.front().success)
                << completedResults.front().errorMessage.toStdString();
            EXPECT_EQ(completedResults.front().renderedFrames, 2);
        }
        EXPECT_EQ(originalCallbackCalls, 1);
        EXPECT_EQ(replacementCallbackCalls, 1);
    }
    if (!completed) master.cancelAll();
    EXPECT_TRUE(completed);
}

TEST(RenderFarmMasterPauseContractTest,
     PauseFinishesCurrentFrameAndResumeContinuesRemainingFrames)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> completedResults;
    bool firstFrameEntered = false;
    bool releaseFirstFrame = false;
    bool firstFrameCompletionObserved = false;

    RenderFarmMaster master(1);
    master.setOnProgress([&](const RenderJobProgress& progress) {
        if (progress.completedFrames.load() >= 1) {
            std::lock_guard lock(stateMutex);
            firstFrameCompletionObserved = true;
            stateChanged.notify_all();
        }
    });
    master.setOnCompleted([&](const RenderJobResult& result) {
        std::lock_guard lock(stateMutex);
        completedResults.push_back(result);
        stateChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("pause-resume-contract-job");
    request.range = {0, 3, 1};
    request.renderFrame = [&](int frame) {
        std::unique_lock lock(stateMutex);
        renderedFrames.push_back(frame);
        if (frame == 0) {
            firstFrameEntered = true;
            stateChanged.notify_all();
            return stateChanged.wait_for(lock, std::chrono::seconds(5),
                [&] { return releaseFirstFrame; });
        }
        return true;
    };

    master.submitJob(request);
    bool firstFrameStarted = false;
    {
        std::unique_lock lock(stateMutex);
        firstFrameStarted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return firstFrameEntered; });
    }
    if (!firstFrameStarted) {
        {
            std::lock_guard lock(stateMutex);
            releaseFirstFrame = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(firstFrameStarted) << "the first frame should enter before pause";
        return;
    }

    master.pause();
    EXPECT_TRUE(master.isPaused());
    {
        std::lock_guard lock(stateMutex);
        releaseFirstFrame = true;
    }
    stateChanged.notify_all();

    bool firstFrameCompleted = false;
    bool frameOneStartedWhilePaused = false;
    {
        std::unique_lock lock(stateMutex);
        firstFrameCompleted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return firstFrameCompletionObserved; });
        frameOneStartedWhilePaused = stateChanged.wait_for(lock, std::chrono::milliseconds(100),
            [&] { return renderedFrames.size() > 1; });
    }

    // Always release the paused worker before making fatal assertions.
    master.resume();
    bool completed = false;
    {
        std::unique_lock lock(stateMutex);
        completed = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return completedResults.size() == 1; });
        EXPECT_EQ(renderedFrames, (std::vector<int>{0, 1, 2}));
        if (completed) {
            EXPECT_TRUE(completedResults.front().success)
                << completedResults.front().errorMessage.toStdString();
            EXPECT_EQ(completedResults.front().renderedFrames, 3);
        }
    }
    if (!completed) master.cancelAll();

    EXPECT_TRUE(firstFrameCompleted);
    EXPECT_FALSE(frameOneStartedWhilePaused);
    EXPECT_TRUE(completed);
    EXPECT_FALSE(master.isPaused());
}

TEST(RenderFarmMasterPauseContractTest,
     JobSubmittedWhilePausedDoesNotStartUntilResume)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> completedResults;

    RenderFarmMaster master(1);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            completedResults.push_back(result);
        }
        stateChanged.notify_all();
    });

    master.pause();
    RenderJobRequest request;
    request.jobId = QStringLiteral("submit-while-paused-contract-job");
    request.range = {10, 13, 1};
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(stateMutex);
            renderedFrames.push_back(frame);
        }
        stateChanged.notify_all();
        return true;
    };

    master.submitJob(request);
    bool frameStartedWhilePaused = false;
    {
        std::unique_lock lock(stateMutex);
        frameStartedWhilePaused = stateChanged.wait_for(lock,
            std::chrono::milliseconds(100),
            [&] { return !renderedFrames.empty(); });
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
            EXPECT_EQ(completedResults.front().renderedFrames, 3);
            EXPECT_EQ(renderedFrames, (std::vector<int>{10, 11, 12}));
        }
    }
    if (!completed) master.cancelAll();

    EXPECT_FALSE(frameStartedWhilePaused);
    EXPECT_TRUE(completed);
    EXPECT_FALSE(master.isPaused());
}

TEST(RenderFarmMasterPauseContractTest,
     PauseAtCheckpointBoundaryResumeContinuesWithoutDuplicatingFrames)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> completedResults;
    bool checkpointIntervalReached = false;

    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 2;
    master.setCheckpointPolicy(checkpointPolicy);
    master.setOnProgress([&](const RenderJobProgress& progress) {
        if (progress.completedFrames.load() >= 2) {
            std::lock_guard lock(stateMutex);
            checkpointIntervalReached = true;
            stateChanged.notify_all();
        }
    });
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            completedResults.push_back(result);
        }
        stateChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("pause-checkpoint-resume-contract-job");
    request.range = {5, 9, 1};
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(stateMutex);
            renderedFrames.push_back(frame);
        }
        if (frame == 6) master.pause();
        stateChanged.notify_all();
        return true;
    };

    master.submitJob(request);
    bool reachedCheckpointBoundary = false;
    {
        std::unique_lock lock(stateMutex);
        reachedCheckpointBoundary = stateChanged.wait_for(
            lock, std::chrono::seconds(5),
            [&] { return checkpointIntervalReached; });
    }
    if (!reachedCheckpointBoundary) {
        master.resume();
        master.cancelAll();
        EXPECT_TRUE(reachedCheckpointBoundary)
            << "the first checkpoint interval should complete before pausing";
        return;
    }

    bool checkpointSaved = false;
    const auto checkpointDeadline = std::chrono::steady_clock::now() +
                                    std::chrono::seconds(5);
    while (!checkpointSaved &&
           std::chrono::steady_clock::now() < checkpointDeadline) {
        const auto checkpoint = master.checkpointStore()->load(request.jobId);
        checkpointSaved = checkpoint.has_value() &&
                          checkpoint->completedUpToFrame == 7;
        if (!checkpointSaved) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!checkpointSaved) {
        master.resume();
        master.cancelAll();
        EXPECT_TRUE(checkpointSaved)
            << "the checkpoint must commit after the second frame completes";
        return;
    }
    EXPECT_TRUE(master.isPaused());
    {
        std::lock_guard lock(stateMutex);
        EXPECT_EQ(renderedFrames, (std::vector<int>{5, 6}));
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
            EXPECT_EQ(completedResults.front().renderedFrames, 4);
            EXPECT_EQ(renderedFrames, (std::vector<int>{5, 6, 7, 8}));
        }
    }
    if (!completed) master.cancelAll();
    EXPECT_TRUE(completed);
    EXPECT_FALSE(master.isPaused());
}

TEST(RenderFarmMasterPauseContractTest,
     CancellingWhilePausedWakesWorkerWithoutStartingAnotherFrame)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<int> renderedFrames;
    bool firstFrameEntered = false;
    bool releaseFirstFrame = false;
    bool firstFrameCompletionObserved = false;

    RenderFarmMaster master(1);
    master.setOnProgress([&](const RenderJobProgress& progress) {
        if (progress.completedFrames.load() >= 1) {
            std::lock_guard lock(stateMutex);
            firstFrameCompletionObserved = true;
            stateChanged.notify_all();
        }
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("cancel-while-paused-contract-job");
    request.range = {0, 4, 1};
    request.renderFrame = [&](int frame) {
        std::unique_lock lock(stateMutex);
        renderedFrames.push_back(frame);
        if (frame == 0) {
            firstFrameEntered = true;
            stateChanged.notify_all();
            return stateChanged.wait_for(lock, std::chrono::seconds(5),
                [&] { return releaseFirstFrame; });
        }
        return true;
    };

    master.submitJob(request);
    bool firstFrameStarted = false;
    {
        std::unique_lock lock(stateMutex);
        firstFrameStarted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return firstFrameEntered; });
    }
    if (!firstFrameStarted) {
        {
            std::lock_guard lock(stateMutex);
            releaseFirstFrame = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(firstFrameStarted) << "frame zero must enter before pause";
        return;
    }

    master.pause();
    {
        std::lock_guard lock(stateMutex);
        releaseFirstFrame = true;
    }
    stateChanged.notify_all();

    bool firstFrameCompleted = false;
    {
        std::unique_lock lock(stateMutex);
        firstFrameCompleted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return firstFrameCompletionObserved; });
    }

    EXPECT_TRUE(master.isPaused());
    master.cancelAll();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (master.isBusy() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(firstFrameCompleted);
    EXPECT_FALSE(master.isBusy())
        << "cancel should wake the worker blocked by pause";
    {
        std::lock_guard lock(stateMutex);
        EXPECT_EQ(renderedFrames, (std::vector<int>{0}));
    }
}
