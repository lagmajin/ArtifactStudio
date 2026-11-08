#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QString>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <future>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

import Core.Render.MFR.Dispatcher;
import Core.Render.MFR.Job;
import Render.JobModel;
import Utils.Id;

using namespace ArtifactCore;

TEST(RenderJobModelContractTest, EmptyModelHasFlatFourColumnRoot)
{
    RenderJobModel model;

    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.columnCount(), 4);
    EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
    EXPECT_EQ(model.columnCount(model.index(0, 0)), 0);
    EXPECT_FALSE(model.index(0, 0).isValid());
    EXPECT_FALSE(model.index(0, 4).isValid());
}

TEST(RenderJobModelContractTest, HorizontalHeadersExposeStableColumnNames)
{
    RenderJobModel model;
    const QString expected[] = {
        QStringLiteral("Composition"), QStringLiteral("Status"),
        QStringLiteral("Progress"), QStringLiteral("Output Path")
    };

    for (int column = 0; column < 4; ++column) {
        EXPECT_EQ(model.headerData(column, Qt::Horizontal).toString(),
                  expected[column]);
    }
    EXPECT_FALSE(model.headerData(0, Qt::Horizontal, Qt::ToolTipRole).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical).isValid());
}

TEST(RenderJobModelContractTest, IndexesRemainFlatAndRejectInvalidCoordinates)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("first"));
    model.addJob(Id::Nil(), QStringLiteral("second"));

    const QModelIndex firstCell = model.index(0, 0);
    ASSERT_TRUE(firstCell.isValid());
    EXPECT_EQ(firstCell.row(), 0);
    EXPECT_EQ(firstCell.column(), 0);
    EXPECT_FALSE(model.parent(firstCell).isValid());
    EXPECT_FALSE(model.index(0, 0, firstCell).isValid());
    EXPECT_FALSE(model.index(-1, 0).isValid());
    EXPECT_FALSE(model.index(2, 0).isValid());
    EXPECT_FALSE(model.index(0, -1).isValid());
    EXPECT_FALSE(model.index(0, 4).isValid());
}

TEST(RenderJobModelContractTest, InsertedJobsExposeNameStatusProgressAndOutput)
{
    RenderJobModel model;
    model.addJob(QStringLiteral("Main Comp"), QStringLiteral("rendering"),
                 37, QStringLiteral("D:/renders/main.exr"));

    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.data(model.index(0, 0)).toString(), QStringLiteral("Main Comp"));
    EXPECT_EQ(model.data(model.index(0, 1)).toString(), QStringLiteral("Rendering"));
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("37%"));
    EXPECT_EQ(model.data(model.index(0, 3)).toString(), QStringLiteral("D:/renders/main.exr"));
    EXPECT_EQ(model.jobAt(0)->progress, 0.37f);
    EXPECT_EQ(model.jobAt(0)->compositionId, Id::Nil());
}

TEST(RenderJobModelContractTest, BasicInsertionUsesStableRenderJobDefaults)
{
    RenderJobModel model;
    const Id compositionId;
    model.addJob(compositionId, QStringLiteral("Main Comp"));

    ASSERT_NE(model.jobAt(0), nullptr);
    const RenderJob& job = *model.jobAt(0);
    EXPECT_EQ(job.compositionId, compositionId);
    EXPECT_EQ(job.compositionName, QStringLiteral("Main Comp"));
    EXPECT_EQ(job.status, RenderJobStatus::Queued);
    EXPECT_FLOAT_EQ(job.progress, 0.0f);
    EXPECT_TRUE(job.outputPath.isEmpty());
    EXPECT_TRUE(job.statusMessage.isEmpty());
    EXPECT_EQ(job.startFrame, 0);
    EXPECT_EQ(job.endFrame, 299);
    EXPECT_EQ(job.frameStep, 1);
    EXPECT_TRUE(job.multiFrameEnabled);
    EXPECT_EQ(job.mfrConcurrentFrames, 0);
    EXPECT_EQ(job.mfrMemoryLimitMB, 8192u);
    EXPECT_EQ(job.mfrRetryBackoffMs, 0);
}

TEST(RenderJobModelContractTest, AddedProgressIsClampedAtBothPercentBoundaries)
{
    RenderJobModel model;
    model.addJob(QStringLiteral("below"), QStringLiteral("queued"), -20,
                 QString());
    model.addJob(QStringLiteral("above"), QStringLiteral("queued"), 140,
                 QString());

    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);
    EXPECT_FLOAT_EQ(model.jobAt(1)->progress, 1.0f);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));
    EXPECT_EQ(model.data(model.index(1, 2)).toString(), QStringLiteral("100%"));
}

TEST(RenderJobModelContractTest, StatusAliasesMapToStableDisplayLabels)
{
    const struct Case {
        const char* input;
        const char* expected;
    } cases[] = {
        {" queued ", "Queued"}, {"running", "Rendering"},
        {"completed", "Done"}, {"failed", "Error"},
        {"cancelled", "Canceled"}, {"Paused", "Paused"},
        {"unknown-status", "Queued"},
    };

    for (const auto& testCase : cases) {
        RenderJobModel model;
        model.addJob(QStringLiteral("job"), QString::fromLatin1(testCase.input),
                     0, QString());
        ASSERT_EQ(model.rowCount(), 1);
        EXPECT_EQ(model.data(model.index(0, 1)).toString(),
                  QString::fromLatin1(testCase.expected)) << testCase.input;
    }
}

TEST(RenderJobModelContractTest, StatusSetterUpdatesEachStableDisplayLabel)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));
    const struct Case {
        RenderJobStatus status;
        const char* expected;
    } cases[] = {
        {RenderJobStatus::Queued, "Queued"},
        {RenderJobStatus::Rendering, "Rendering"},
        {RenderJobStatus::Done, "Done"},
        {RenderJobStatus::Error, "Error"},
        {RenderJobStatus::Canceled, "Canceled"},
        {RenderJobStatus::Paused, "Paused"},
    };

    for (const auto& testCase : cases) {
        model.setJobStatus(0, testCase.status);
        EXPECT_EQ(model.data(model.index(0, 1)).toString(),
                  QString::fromLatin1(testCase.expected));
    }
}

TEST(RenderJobModelContractTest, UnknownStatusValueHasDefensiveDisplayLabel)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));

    model.setJobStatus(0, static_cast<RenderJobStatus>(999));

    EXPECT_EQ(model.data(model.index(0, 1)).toString(), QStringLiteral("Unknown"));
}

TEST(RenderJobModelContractTest, InvalidStatusAndProgressRowsDoNotChangeJobs)
{
    RenderJobModel model;
    model.addJob(QStringLiteral("job"), QStringLiteral("running"), 42,
                 QStringLiteral("output.png"));
    const RenderJobStatus originalStatus = model.jobAt(0)->status;
    const float originalProgress = model.jobAt(0)->progress;

    model.setJobStatus(-1, RenderJobStatus::Error);
    model.setJobStatus(1, RenderJobStatus::Error);
    model.setJobProgress(-1, 0.9f);
    model.setJobProgress(1, 0.9f);

    EXPECT_EQ(model.jobAt(0)->status, originalStatus);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, originalProgress);
}

TEST(RenderJobModelContractTest, ProgressSetterClampsAndSanitizesNonFiniteInput)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));

    model.setJobProgress(0, 1.5f);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 1.0f);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("100%"));

    model.setJobProgress(0, -0.5f);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));

    model.setJobProgress(0, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);
    model.setJobProgress(0, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);

    // The display layer also defends against state modified outside the setter.
    model.jobAt(0)->progress = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));
    model.jobAt(0)->progress = std::numeric_limits<float>::infinity();
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));
}

TEST(RenderJobModelContractTest, InvalidRowsAndUnsupportedRolesReturnSafeResults)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));

    EXPECT_EQ(model.jobAt(-1), nullptr);
    EXPECT_EQ(model.jobAt(1), nullptr);
    EXPECT_FALSE(model.data(QModelIndex()).isValid());
    EXPECT_FALSE(model.data(model.index(-1, 0)).isValid());
    EXPECT_FALSE(model.data(model.index(1, 0)).isValid());
    EXPECT_FALSE(model.data(model.index(0, 0), Qt::ToolTipRole).isValid());
    EXPECT_FALSE(model.headerData(4, Qt::Horizontal).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical).isValid());
}

TEST(RenderJobModelContractTest, FrameRangeRejectsInvalidUpdatesWithoutChangingPreviousRange)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"), 10, 30, 2);
    RenderJob* job = model.jobAt(0);
    ASSERT_NE(job, nullptr);

    EXPECT_EQ(job->startFrame, 10);
    EXPECT_EQ(job->endFrame, 30);
    EXPECT_EQ(job->frameStep, 2);
    EXPECT_FALSE(model.setJobFrameRange(0, 31, 30, 1));
    EXPECT_FALSE(model.setJobFrameRange(0, 0, 10, 0));
    EXPECT_FALSE(model.setJobFrameRange(-1, 0, 10, 1));
    EXPECT_EQ(job->startFrame, 10);
    EXPECT_EQ(job->endFrame, 30);
    EXPECT_EQ(job->frameStep, 2);

    EXPECT_TRUE(model.setJobFrameRange(0, -5, 0, 3));
    EXPECT_EQ(job->startFrame, -5);
    EXPECT_EQ(job->endFrame, 0);
    EXPECT_EQ(job->frameStep, 3);
}

TEST(RenderJobModelContractTest, InvalidInitialFrameRangeDoesNotInsertJob)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("reversed"), 5, 4, 1);
    model.addJob(Id::Nil(), QStringLiteral("zero step"), 0, 1, 0);
    EXPECT_EQ(model.rowCount(), 0);

    model.addJob(Id::Nil(), QStringLiteral("single frame"), 5, 5, 1);
    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.jobAt(0)->startFrame, 5);
    EXPECT_EQ(model.jobAt(0)->endFrame, 5);
}

TEST(RenderJobModelContractTest, MfrSettingsRejectNegativeConcurrencyAndBackoffAtomically)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));
    RenderJob* job = model.jobAt(0);
    ASSERT_NE(job, nullptr);

    EXPECT_TRUE(model.setJobMfrSettings(0, true, 4, 2048u, 75));
    EXPECT_TRUE(job->multiFrameEnabled);
    EXPECT_EQ(job->mfrConcurrentFrames, 4);
    EXPECT_EQ(job->mfrMemoryLimitMB, 2048u);
    EXPECT_EQ(job->mfrRetryBackoffMs, 75);

    EXPECT_FALSE(model.setJobMfrSettings(0, false, -1, 0u, 0));
    EXPECT_FALSE(model.setJobMfrSettings(0, false, 1, 0u, -1));
    EXPECT_FALSE(model.setJobMfrSettings(4, false, 1, 0u, 0));
    EXPECT_TRUE(job->multiFrameEnabled);
    EXPECT_EQ(job->mfrConcurrentFrames, 4);
    EXPECT_EQ(job->mfrMemoryLimitMB, 2048u);
    EXPECT_EQ(job->mfrRetryBackoffMs, 75);
}

TEST(RenderJobModelContractTest, MfrSettingsAcceptZeroLimitsAndCanBeDisabled)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));
    RenderJob* job = model.jobAt(0);
    ASSERT_NE(job, nullptr);

    EXPECT_TRUE(model.setJobMfrSettings(0, false, 0, 0u, 0));
    EXPECT_FALSE(job->multiFrameEnabled);
    EXPECT_EQ(job->mfrConcurrentFrames, 0);
    EXPECT_EQ(job->mfrMemoryLimitMB, 0u);
    EXPECT_EQ(job->mfrRetryBackoffMs, 0);
}

TEST(MfrDispatcherContractTest, RetriesSelectedFramesAndContinuesAfterPermanentFailure)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = -3;
    config.endFrame = 8;
    config.frameStep = 2;
    config.maxConcurrentFrames = 4;
    config.framesAreDependent = true;
    config.maxRetryCount = 1;
    config.continueOnError = true;

    constexpr std::array<int, 6> expectedFrames = {-3, -1, 1, 3, 5, 7};
    std::array<int, expectedFrames.size()> attempts{};
    std::vector<std::pair<int, int>> progress;
    MFRDispatcher dispatcher;
    const MFRJobResult result = dispatcher.executeBlocking(
        config,
        [&attempts, &expectedFrames](int frame) {
            const auto found = std::find(expectedFrames.begin(), expectedFrames.end(), frame);
            if (found == expectedFrames.end()) {
                return false;
            }
            const auto index = static_cast<std::size_t>(found - expectedFrames.begin());
            const int attempt = attempts[index]++;
            if (frame == 3) {
                return false;
            }
            return frame != -1 || attempt > 0;
        },
        [&progress](const MFRProgress& current) {
            progress.emplace_back(current.completedFrames.load(),
                                  current.failedFrames.load());
        });

    ASSERT_EQ(result.frames.size(), expectedFrames.size());
    EXPECT_EQ(result.completedCount, 5);
    EXPECT_EQ(result.failedCount, 1);
    EXPECT_FALSE(result.cancelled);
    for (std::size_t index = 0; index < expectedFrames.size(); ++index) {
        EXPECT_EQ(result.frames[index].frameNumber, expectedFrames[index]);
        EXPECT_EQ(result.frames[index].success, expectedFrames[index] != 3);
    }
    EXPECT_EQ(attempts, (std::array<int, 6>{1, 2, 1, 2, 1, 1}));
    ASSERT_EQ(progress.size(), expectedFrames.size());
    EXPECT_EQ(progress.back().first + progress.back().second,
              static_cast<int>(expectedFrames.size()));
}

TEST(MfrDispatcherContractTest,
     SparseFrameRangeUsesWideArithmeticAcrossIntegerBoundaries)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = std::numeric_limits<int>::min();
    config.endFrame = std::numeric_limits<int>::max();
    config.frameStep = std::numeric_limits<int>::max();
    config.maxConcurrentFrames = 1;

    std::vector<int> renderedFrames;
    MFRDispatcher dispatcher;
    const MFRJobResult result = dispatcher.executeBlocking(
        config, [&renderedFrames](int frame) {
            renderedFrames.push_back(frame);
            return true;
        });

    EXPECT_EQ(renderedFrames,
              (std::vector<int>{std::numeric_limits<int>::min(), -1,
                                std::numeric_limits<int>::max() - 1}));
    ASSERT_EQ(result.frames.size(), 3u);
    EXPECT_EQ(result.frames[0].frameNumber, std::numeric_limits<int>::min());
    EXPECT_EQ(result.frames[1].frameNumber, -1);
    EXPECT_EQ(result.frames[2].frameNumber, std::numeric_limits<int>::max() - 1);
    EXPECT_EQ(result.completedCount, 3);
    EXPECT_EQ(result.failedCount, 0);
    EXPECT_FALSE(result.cancelled);
}

TEST(MfrDispatcherContractTest,
     DependentFramesWaitForTheCurrentFrameRetryBeforeStartingLaterFrames)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = 0;
    config.endFrame = 2;
    config.maxConcurrentFrames = 1;
    config.framesAreDependent = true;
    config.maxRetryCount = 1;

    std::mutex mutex;
    std::condition_variable changed;
    int activeCallbacks = 0;
    int maximumActiveCallbacks = 0;
    int frameZeroAttempts = 0;
    bool laterFrameStarted = false;
    std::vector<int> callOrder;

    MFRDispatcher dispatcher;
    const MFRJobResult result = dispatcher.executeBlocking(
        config,
        [&](int frame) {
            bool failThisAttempt = false;
            {
                std::unique_lock lock(mutex);
                ++activeCallbacks;
                maximumActiveCallbacks =
                    std::max(maximumActiveCallbacks, activeCallbacks);
                callOrder.push_back(frame);
                if (frame != 0) {
                    laterFrameStarted = true;
                    changed.notify_all();
                } else if (frameZeroAttempts++ == 0) {
                    changed.wait_for(lock, std::chrono::milliseconds(100),
                                     [&] { return laterFrameStarted; });
                    failThisAttempt = true;
                }
                --activeCallbacks;
            }
            changed.notify_all();
            return !failThisAttempt;
        });

    EXPECT_EQ(result.completedCount, 3);
    EXPECT_EQ(result.failedCount, 0);
    EXPECT_FALSE(result.cancelled);
    EXPECT_EQ(frameZeroAttempts, 2);
    EXPECT_EQ(maximumActiveCallbacks, 1);
    EXPECT_EQ(callOrder, (std::vector<int>{0, 0, 1, 2}));
    ASSERT_EQ(result.frames.size(), 3u);
    for (const auto& frame : result.frames) EXPECT_TRUE(frame.success);
}

TEST(MfrDispatcherContractTest, CancellationStopsNewFramesAfterInFlightFramesFinish)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = 10;
    config.endFrame = 19;
    config.maxConcurrentFrames = 2;
    config.maxRetryCount = 0;

    MFRDispatcher dispatcher;
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    int startedFrames = 0;
    bool releaseFrames = false;
    std::vector<int> frameNumbers;
    std::vector<std::pair<int, int>> progress;

    auto resultFuture = std::async(std::launch::async,
        [&dispatcher, &config, &gateMutex, &gateChanged, &startedFrames,
         &releaseFrames, &frameNumbers, &progress] {
            return dispatcher.executeBlocking(
                config,
                [&gateMutex, &gateChanged, &startedFrames, &releaseFrames,
                 &frameNumbers](int frame) {
                    std::unique_lock lock(gateMutex);
                    frameNumbers.push_back(frame);
                    ++startedFrames;
                    gateChanged.notify_all();
                    gateChanged.wait(lock, [&releaseFrames] {
                        return releaseFrames;
                    });
                    return true;
                },
                [&progress](const MFRProgress& current) {
                    progress.emplace_back(current.completedFrames.load(),
                                          current.failedFrames.load());
                });
        });

    std::unique_lock gateLock(gateMutex);
    const bool bothWorkersStarted = gateChanged.wait_for(
        gateLock, std::chrono::seconds(5), [&startedFrames] {
            return startedFrames == 2;
        });
    if (!bothWorkersStarted) {
        releaseFrames = true;
        gateLock.unlock();
        gateChanged.notify_all();
        (void)resultFuture.get();
        FAIL() << "both frame workers did not enter the gated callback";
        return;
    }

    gateLock.unlock();
    dispatcher.cancel();
    {
        std::lock_guard lock(gateMutex);
        releaseFrames = true;
    }
    gateChanged.notify_all();

    const MFRJobResult result = resultFuture.get();
    EXPECT_EQ(result.completedCount, 2);
    EXPECT_EQ(result.failedCount, 0);
    EXPECT_TRUE(result.cancelled);
    ASSERT_EQ(result.frames.size(), 10u);
    EXPECT_EQ(std::count_if(result.frames.begin(), result.frames.end(),
                            [](const auto& frame) { return frame.success; }), 2);
    ASSERT_EQ(frameNumbers.size(), 2u);
    std::sort(frameNumbers.begin(), frameNumbers.end());
    EXPECT_EQ(frameNumbers, (std::vector<int>{10, 11}));
    ASSERT_EQ(progress.size(), 2u);
    EXPECT_EQ(progress.back().first, 2);
    EXPECT_EQ(progress.back().second, 0);
}

TEST(MfrDispatcherContractTest,
     AsyncCancellationWaitsForInflightFrameAndSkipsUnstartedFrames)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = 20;
    config.endFrame = 29;
    config.maxConcurrentFrames = 1;

    MFRDispatcher dispatcher;
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    bool frameEntered = false;
    bool releaseFrame = false;
    std::vector<int> frameNumbers;

    auto resultFuture = dispatcher.executeAsync(
        config, [&gateMutex, &gateChanged, &frameEntered, &releaseFrame,
                 &frameNumbers](int frame) {
            std::unique_lock lock(gateMutex);
            frameNumbers.push_back(frame);
            frameEntered = true;
            gateChanged.notify_all();
            gateChanged.wait(lock, [&releaseFrame] { return releaseFrame; });
            return true;
        });

    std::unique_lock lock(gateMutex);
    const bool entered = gateChanged.wait_for(
        lock, std::chrono::seconds(5), [&frameEntered] { return frameEntered; });
    if (!entered) {
        releaseFrame = true;
        lock.unlock();
        gateChanged.notify_all();
        (void)resultFuture.get();
        FAIL() << "the asynchronous frame callback did not start";
        return;
    }

    dispatcher.cancel();
    releaseFrame = true;
    lock.unlock();
    gateChanged.notify_all();

    const MFRJobResult result = resultFuture.get();
    EXPECT_TRUE(result.cancelled);
    EXPECT_EQ(result.completedCount, 1);
    EXPECT_EQ(result.failedCount, 0);
    ASSERT_EQ(frameNumbers.size(), 1u);
    EXPECT_EQ(frameNumbers.front(), 20);
    ASSERT_EQ(result.frames.size(), 10u);
    EXPECT_TRUE(result.frames.front().success);
    for (std::size_t index = 1; index < result.frames.size(); ++index) {
        EXPECT_FALSE(result.frames[index].success);
    }
}

TEST(MfrDispatcherContractTest,
     AsyncProgressCallbackExceptionDoesNotAbandonRemainingFrames)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = 4;
    config.endFrame = 7;
    config.maxConcurrentFrames = 1;

    int progressCalls = 0;
    std::vector<int> renderedFrames;
    MFRDispatcher dispatcher;
    auto resultFuture = dispatcher.executeAsync(
        config, [&renderedFrames](int frame) {
            renderedFrames.push_back(frame);
            return true;
        }, [&progressCalls](const MFRProgress&) {
            ++progressCalls;
            if (progressCalls == 1) throw 1;
        });

    const MFRJobResult result = resultFuture.get();
    EXPECT_EQ(renderedFrames, (std::vector<int>{4, 5, 6, 7}));
    EXPECT_EQ(progressCalls, 4);
    EXPECT_EQ(result.completedCount, 4);
    EXPECT_EQ(result.failedCount, 0);
    EXPECT_FALSE(result.cancelled);
    ASSERT_EQ(result.frames.size(), 4u);
    for (std::size_t index = 0; index < result.frames.size(); ++index) {
        EXPECT_TRUE(result.frames[index].success);
        EXPECT_EQ(result.frames[index].frameNumber, 4 + static_cast<int>(index));
    }
}

TEST(MfrDispatcherContractTest,
     CancellationBeforeRetryPreventsAnotherAttemptOfTheFailedFrame)
{
    using namespace ArtifactCore::Render::MFR;

    MFRJobConfig config;
    config.startFrame = 7;
    config.endFrame = 7;
    config.maxConcurrentFrames = 1;
    config.maxRetryCount = 3;
    config.retryBackoffMs = 100;

    MFRDispatcher dispatcher;
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    bool firstAttemptEntered = false;
    bool releaseFirstAttempt = false;
    int attempts = 0;

    auto resultFuture = std::async(std::launch::async,
        [&dispatcher, &config, &gateMutex, &gateChanged, &firstAttemptEntered,
         &releaseFirstAttempt, &attempts] {
            return dispatcher.executeBlocking(config, [&](int frame) {
                std::unique_lock lock(gateMutex);
                EXPECT_EQ(frame, 7);
                ++attempts;
                firstAttemptEntered = true;
                gateChanged.notify_all();
                gateChanged.wait(lock, [&releaseFirstAttempt] {
                    return releaseFirstAttempt;
                });
                return false;
            });
        });

    std::unique_lock lock(gateMutex);
    const bool attemptEntered = gateChanged.wait_for(
        lock, std::chrono::seconds(5), [&firstAttemptEntered] {
            return firstAttemptEntered;
        });
    if (!attemptEntered) {
        releaseFirstAttempt = true;
        lock.unlock();
        gateChanged.notify_all();
        (void)resultFuture.get();
        FAIL() << "the first retryable frame attempt did not start";
        return;
    }

    dispatcher.cancel();
    releaseFirstAttempt = true;
    lock.unlock();
    gateChanged.notify_all();

    const MFRJobResult result = resultFuture.get();
    EXPECT_EQ(attempts, 1);
    EXPECT_EQ(result.completedCount, 0);
    EXPECT_EQ(result.failedCount, 1);
    ASSERT_EQ(result.frames.size(), 1u);
    EXPECT_EQ(result.frames.front().frameNumber, 7);
    EXPECT_FALSE(result.frames.front().success);
}

TEST(RenderJobModelContractTest,
     PersistentIndexesTrackJobsAcrossInsertionAndRemoval)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("first"));
    model.addJob(Id::Nil(), QStringLiteral("second"));

    QPersistentModelIndex secondJob(model.index(1, 0));
    ASSERT_TRUE(secondJob.isValid());
    EXPECT_EQ(secondJob.data().toString(), QStringLiteral("second"));

    model.removeJob(-1);
    model.removeJob(model.rowCount());
    EXPECT_TRUE(secondJob.isValid());
    EXPECT_EQ(secondJob.row(), 1);
    EXPECT_EQ(secondJob.data().toString(), QStringLiteral("second"));

    model.addJob(Id::Nil(), QStringLiteral("third"));
    EXPECT_TRUE(secondJob.isValid());
    EXPECT_EQ(secondJob.row(), 1);
    EXPECT_EQ(secondJob.data().toString(), QStringLiteral("second"));

    model.removeJob(0);
    ASSERT_TRUE(secondJob.isValid());
    EXPECT_EQ(secondJob.row(), 0);
    EXPECT_EQ(secondJob.data().toString(), QStringLiteral("second"));

    model.removeJob(0);
    EXPECT_FALSE(secondJob.isValid());

    model.addJob(Id::Nil(), QStringLiteral("reset target"));
    QPersistentModelIndex resetTarget(model.index(0, 0));
    ASSERT_TRUE(resetTarget.isValid());
    model.clearJobs();
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_FALSE(resetTarget.isValid());
}

TEST(RenderJobModelContractTest, RemoveAndClearMaintainRowCountsAndBounds)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("first"));
    model.addJob(Id::Nil(), QStringLiteral("second"));
    model.addJob(Id::Nil(), QStringLiteral("third"));

    model.removeJob(-1);
    model.removeJob(3);
    ASSERT_EQ(model.rowCount(), 3);
    EXPECT_EQ(model.data(model.index(1, 0)).toString(), QStringLiteral("second"));

    model.removeJob(1);
    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(model.data(model.index(1, 0)).toString(), QStringLiteral("third"));
    model.clearJobs();
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.jobAt(0), nullptr);
}
