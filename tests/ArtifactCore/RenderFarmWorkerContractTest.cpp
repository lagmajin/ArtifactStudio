#include <gtest/gtest.h>

#include <atomic>
#include <array>
#include <chrono>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>

#include <QString>

import Render.Farm.Types;
import Render.Farm.Worker;

using namespace ArtifactCore;

TEST(RenderFarmWorkerContractTest,
     CancellationCompletesInflightFrameAndRejectsConcurrentStart)
{
    RenderFarmWorker worker(7);
    std::promise<void> frameStartedPromise;
    auto frameStarted = frameStartedPromise.get_future();
    std::promise<void> releaseFramePromise;
    const auto releaseFrame = releaseFramePromise.get_future().share();
    std::promise<RenderJobResult> completedPromise;
    auto completedResult = completedPromise.get_future();
    std::atomic<int> firstRequestCalls{0};
    std::atomic<int> competingRequestCalls{0};
    std::atomic<int> progressCallbackCalls{0};
    std::atomic<int> completionCallbackCalls{0};

    RenderJobRequest request;
    request.renderFrame = [&frameStartedPromise, releaseFrame,
                           &firstRequestCalls](int frame) {
        firstRequestCalls.fetch_add(1, std::memory_order_relaxed);
        frameStartedPromise.set_value();
        releaseFrame.wait();
        return frame == 10;
    };
    RenderJobRequest competingRequest;
    competingRequest.renderFrame = [&competingRequestCalls](int) {
        competingRequestCalls.fetch_add(1, std::memory_order_relaxed);
        return true;
    };
    worker.setOnProgress([&progressCallbackCalls](const RenderJobProgress&) {
        progressCallbackCalls.fetch_add(1, std::memory_order_relaxed);
    });
    worker.setOnCompleted([&completedPromise, &completionCallbackCalls](
                              const RenderJobResult& result) {
        if (completionCallbackCalls.fetch_add(1, std::memory_order_relaxed) == 0) {
            completedPromise.set_value(result);
        }
    });

    std::thread rendering([&worker, &request] {
        worker.start(request, RenderFrameRange{10, 15, 1});
    });
    if (frameStarted.wait_for(std::chrono::seconds(2)) !=
        std::future_status::ready) {
        releaseFramePromise.set_value();
        rendering.join();
        FAIL() << "first frame did not enter its render callback";
        return;
    }

    EXPECT_TRUE(worker.isBusy());
    EXPECT_EQ(worker.state(), WorkerState::Rendering);
    const RenderJobProgress inFlightProgress = worker.progress();
    EXPECT_EQ(inFlightProgress.totalFrames, 5);
    EXPECT_EQ(inFlightProgress.completedFrames.load(), 0);
    EXPECT_EQ(inFlightProgress.failedFrames.load(), 0);

    worker.start(competingRequest, RenderFrameRange{100, 101, 1});
    EXPECT_EQ(competingRequestCalls.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(firstRequestCalls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(worker.state(), WorkerState::Rendering);

    worker.cancel();
    releaseFramePromise.set_value();
    rendering.join();

    ASSERT_EQ(completedResult.wait_for(std::chrono::seconds(0)),
              std::future_status::ready);
    const RenderJobResult result = completedResult.get();
    EXPECT_EQ(worker.id(), 7);
    EXPECT_FALSE(worker.isBusy());
    EXPECT_EQ(worker.state(), WorkerState::Cancelled);
    EXPECT_EQ(firstRequestCalls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(competingRequestCalls.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(progressCallbackCalls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(completionCallbackCalls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(result.renderedFrames, 1);
    EXPECT_EQ(result.failedFrames, 0);

    const RenderJobProgress finalProgress = worker.progress();
    EXPECT_EQ(finalProgress.totalFrames, 5);
    EXPECT_EQ(finalProgress.completedFrames.load(), 1);
    EXPECT_EQ(finalProgress.failedFrames.load(), 0);
}

TEST(RenderFarmWorkerContractTest,
     SteppedRangeContinuesAfterFalseAndThrownFramesAndAggregatesProgress)
{
    RenderFarmWorker worker(11);
    std::array<int, 3> attemptedFrames{-1, -1, -1};
    std::array<RenderJobProgress, 3> progressSnapshots{};
    int attemptedCount = 0;
    int progressCount = 0;
    RenderJobResult completedResult;
    int completionCount = 0;

    RenderJobRequest request;
    request.renderFrame = [&attemptedFrames, &attemptedCount](int frame) {
        const int index = attemptedCount++;
        if (index >= 0 && static_cast<std::size_t>(index) < attemptedFrames.size()) {
            attemptedFrames[static_cast<std::size_t>(index)] = frame;
        }
        if (frame == 5) return false;
        if (frame == 8) throw std::runtime_error("frame renderer threw");
        return true;
    };
    worker.setOnProgress([&progressSnapshots, &progressCount](
                             const RenderJobProgress& progress) {
        const int index = progressCount++;
        if (index >= 0 && static_cast<std::size_t>(index) < progressSnapshots.size()) {
            progressSnapshots[static_cast<std::size_t>(index)] = progress;
        }
    });
    worker.setOnCompleted([&completedResult, &completionCount](
                              const RenderJobResult& result) {
        completedResult = result;
        ++completionCount;
    });

    worker.start(request, RenderFrameRange{2, 9, 3});

    EXPECT_EQ(worker.state(), WorkerState::Completed);
    EXPECT_FALSE(worker.isBusy());
    EXPECT_EQ(attemptedCount, 3);
    EXPECT_EQ((attemptedFrames), (std::array<int, 3>{2, 5, 8}));
    EXPECT_EQ(progressCount, 3);
    for (std::size_t index = 0; index < progressSnapshots.size(); ++index) {
        EXPECT_EQ(progressSnapshots[index].totalFrames, 3);
        EXPECT_EQ(progressSnapshots[index].completedFrames.load(), 1);
        EXPECT_EQ(progressSnapshots[index].failedFrames.load(),
                  static_cast<int>(index));
    }
    EXPECT_FALSE(completedResult.success);
    EXPECT_EQ(completedResult.renderedFrames, 1);
    EXPECT_EQ(completedResult.failedFrames, 2);
    EXPECT_EQ(completedResult.errorMessage,
              QStringLiteral("Worker 11: 2 frames failed"));
    EXPECT_EQ(completionCount, 1);

    const RenderJobProgress finalProgress = worker.progress();
    EXPECT_EQ(finalProgress.totalFrames, 3);
    EXPECT_EQ(finalProgress.completedFrames.load(), 1);
    EXPECT_EQ(finalProgress.failedFrames.load(), 2);
}

TEST(RenderFarmWorkerContractTest,
     ReuseAfterProgressCallbackCancellationResetsCancelFlagAndProgress)
{
    RenderFarmWorker worker(13);
    std::array<int, 3> secondRequestFrames{-1, -1, -1};
    std::array<RenderJobResult, 2> results{};
    int firstRequestCalls = 0;
    int secondRequestCalls = 0;
    int completionCount = 0;
    bool cancelFirstRequestOnProgress = true;

    worker.setOnCompleted([&results, &completionCount](
                              const RenderJobResult& result) {
        const int index = completionCount++;
        if (index >= 0 && static_cast<std::size_t>(index) < results.size()) {
            results[static_cast<std::size_t>(index)] = result;
        }
    });
    worker.setOnProgress([&worker, &cancelFirstRequestOnProgress](
                             const RenderJobProgress& progress) {
        if (cancelFirstRequestOnProgress &&
            progress.completedFrames.load() == 1) {
            cancelFirstRequestOnProgress = false;
            worker.cancel();
        }
    });

    RenderJobRequest firstRequest;
    firstRequest.renderFrame = [&firstRequestCalls](int) {
        ++firstRequestCalls;
        return true;
    };
    worker.start(firstRequest, RenderFrameRange{10, 13, 1});

    EXPECT_EQ(worker.state(), WorkerState::Cancelled);
    EXPECT_EQ(firstRequestCalls, 1);
    EXPECT_EQ(results[0].renderedFrames, 1);
    EXPECT_EQ(results[0].failedFrames, 0);
    EXPECT_EQ(worker.progress().totalFrames, 3);
    EXPECT_EQ(worker.progress().completedFrames.load(), 1);

    RenderJobRequest secondRequest;
    secondRequest.renderFrame = [&secondRequestFrames, &secondRequestCalls](int frame) {
        const int index = secondRequestCalls++;
        if (index >= 0 && static_cast<std::size_t>(index) < secondRequestFrames.size()) {
            secondRequestFrames[static_cast<std::size_t>(index)] = frame;
        }
        return true;
    };
    worker.start(secondRequest, RenderFrameRange{20, 23, 1});

    EXPECT_EQ(worker.state(), WorkerState::Completed);
    EXPECT_FALSE(worker.isBusy());
    EXPECT_EQ((secondRequestFrames), (std::array<int, 3>{20, 21, 22}));
    EXPECT_EQ(secondRequestCalls, 3);
    ASSERT_EQ(completionCount, 2);
    EXPECT_TRUE(results[1].success);
    EXPECT_EQ(results[1].renderedFrames, 3);
    EXPECT_EQ(results[1].failedFrames, 0);
    const RenderJobProgress finalProgress = worker.progress();
    EXPECT_EQ(finalProgress.totalFrames, 3);
    EXPECT_EQ(finalProgress.completedFrames.load(), 3);
    EXPECT_EQ(finalProgress.failedFrames.load(), 0);
}


TEST(RenderFarmWorkerContractTest,
     ProgressCallbackExceptionDoesNotStrandWorkerOrPreventReuse)
{
    RenderFarmWorker worker(17);
    int renderedFrames = 0;
    int progressCalls = 0;
    int completionCalls = 0;

    worker.setOnProgress([&progressCalls](const RenderJobProgress&) {
        ++progressCalls;
        if (progressCalls == 1) {
            throw std::runtime_error("progress observer threw");
        }
    });
    worker.setOnCompleted([&completionCalls](const RenderJobResult&) {
        ++completionCalls;
    });

    RenderJobRequest request;
    request.renderFrame = [&renderedFrames](int) {
        ++renderedFrames;
        return true;
    };

    EXPECT_NO_THROW(worker.start(request, RenderFrameRange{30, 32, 1}));

    EXPECT_EQ(renderedFrames, 2);
    EXPECT_EQ(progressCalls, 2);
    EXPECT_EQ(completionCalls, 1);
    EXPECT_EQ(worker.state(), WorkerState::Completed);
    EXPECT_FALSE(worker.isBusy());

    EXPECT_NO_THROW(worker.start(request, RenderFrameRange{40, 41, 1}));

    EXPECT_EQ(renderedFrames, 3);
    EXPECT_EQ(progressCalls, 3);
    EXPECT_EQ(completionCalls, 2);
    EXPECT_EQ(worker.state(), WorkerState::Completed);
    EXPECT_FALSE(worker.isBusy());
    EXPECT_EQ(worker.progress().completedFrames.load(), 1);
    EXPECT_EQ(worker.progress().failedFrames.load(), 0);
}

TEST(RenderFarmWorkerContractTest,
     SteppedRangeNearIntMaximumDoesNotWrapIntoExtraFrames)
{
    RenderFarmWorker worker(19);
    const int finalRepresentableFrame = std::numeric_limits<int>::max() - 1;
    int callbackCount = 0;
    std::array<int, 2> observedFrames{-1, -1};
    RenderJobResult completedResult;
    int completionCount = 0;

    RenderJobRequest request;
    request.renderFrame = [&worker, &callbackCount, &observedFrames,
                           finalRepresentableFrame](int frame) {
        const int index = callbackCount++;
        if (index >= 0 && static_cast<std::size_t>(index) < observedFrames.size()) {
            observedFrames[static_cast<std::size_t>(index)] = frame;
        }
        if (frame != finalRepresentableFrame) {
            // Bound the regression case if an overflowing increment produces
            // an out-of-range frame on the next loop iteration.
            worker.cancel();
        }
        return true;
    };
    worker.setOnCompleted([&completedResult, &completionCount](
                              const RenderJobResult& result) {
        completedResult = result;
        ++completionCount;
    });

    worker.start(request, RenderFrameRange{finalRepresentableFrame,
                                            std::numeric_limits<int>::max(), 2});

    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(observedFrames[0], finalRepresentableFrame);
    EXPECT_EQ(worker.state(), WorkerState::Completed);
    EXPECT_FALSE(worker.isBusy());
    ASSERT_EQ(completionCount, 1);
    EXPECT_TRUE(completedResult.success);
    EXPECT_EQ(completedResult.renderedFrames, 1);
    EXPECT_EQ(completedResult.failedFrames, 0);
    EXPECT_EQ(worker.progress().totalFrames, 1);
    EXPECT_EQ(worker.progress().completedFrames.load(), 1);
    EXPECT_EQ(worker.progress().failedFrames.load(), 0);
}

TEST(RenderFarmWorkerContractTest,
     FrameRangeCountUsesWideArithmeticCeilingDivisionAndSaturation)
{
    const int minimum = std::numeric_limits<int>::min();
    const int maximum = std::numeric_limits<int>::max();

    EXPECT_EQ((RenderFrameRange{minimum, maximum, 1}).count(), maximum)
        << "a full int-domain span must saturate instead of overflowing";
    EXPECT_EQ((RenderFrameRange{minimum, maximum, maximum}).count(), 3)
        << "the positive span must use ceiling division for a partial final step";
    EXPECT_EQ((RenderFrameRange{maximum - 1, maximum, 2}).count(), 1);
    EXPECT_EQ((RenderFrameRange{0, 5, 0}).count(), 0);
    EXPECT_EQ((RenderFrameRange{0, 5, -1}).count(), 0);
    EXPECT_EQ((RenderFrameRange{5, 5, 1}).count(), 0);
    EXPECT_EQ((RenderFrameRange{5, 0, 1}).count(), 0);
}
