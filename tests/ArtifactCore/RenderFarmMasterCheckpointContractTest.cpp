#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <QByteArray>
#include <QChar>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QTemporaryDir>

import Render.Farm.Master;
import Render.Farm.Types;

using namespace ArtifactCore;

TEST(RenderFarmMasterCheckpointContractTest,
     SuccessfulFrameCallbacksDoNotAcceptMissingOrEmptySequenceOutputs)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<RenderJobResult> results;
    std::vector<int> callbackFrames;
    RenderFarmMaster master(1);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            results.push_back(result);
        }
        stateChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("missing-output-validation-contract-job");
    request.range = {4, 6, 1};
    request.outputPath = directory.filePath(QStringLiteral("frame_%04d.bin"));
    request.renderFrame = [&](int frame) {
        if (frame == 4) {
            QString emptyPath = request.outputPath;
            emptyPath.replace(QStringLiteral("%04d"), QStringLiteral("0004"));
            QFile emptyOutput(emptyPath);
            if (!emptyOutput.open(QIODevice::WriteOnly)) return false;
        }
        {
            std::lock_guard lock(stateMutex);
            callbackFrames.push_back(frame);
        }
        return true; // Frame 4 is empty; frame 5 is missing despite callback success.
    };

    master.submitJob(request);
    {
        std::unique_lock lock(stateMutex);
        ASSERT_TRUE(stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return !results.empty(); }));
        ASSERT_EQ(results.size(), 1u);
        EXPECT_FALSE(results[0].success);
        EXPECT_EQ(results[0].renderedFrames, 2);
        EXPECT_EQ(callbackFrames, (std::vector<int>{4, 5}));
        EXPECT_NE(results[0].errorMessage.indexOf(QStringLiteral("Missing or empty")), -1)
            << results[0].errorMessage.toStdString();
    }
}

TEST(RenderFarmMasterCheckpointContractTest,
     CompletedCheckpointRerendersZeroByteFrameButKeepsValidOutputs)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<RenderJobResult> results;
    std::vector<int> renderedFrames;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy policy;
    policy.mode = CheckpointPolicy::Mode::EveryNFrames;
    policy.interval = 1;
    master.setCheckpointPolicy(policy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(stateMutex);
            results.push_back(result);
        }
        stateChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("zero-byte-output-resume-contract-job");
    request.range = {0, 3, 1};
    const QString outputPattern = directory.filePath(QStringLiteral("frame_%04d.bin"));
    request.outputPath = outputPattern;
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(stateMutex);
            renderedFrames.push_back(frame);
        }
        QString path = outputPattern;
        path.replace(QStringLiteral("%04d"),
                     QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
        QFile output(path);
        return output.open(QIODevice::WriteOnly) &&
               output.write(QByteArrayLiteral("frame")) == 5;
    };
    const auto waitFor = [&](std::size_t count) {
        std::unique_lock lock(stateMutex);
        return stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return results.size() >= count; });
    };

    master.submitJob(request);
    ASSERT_TRUE(waitFor(1));
    {
        std::lock_guard lock(stateMutex);
        ASSERT_TRUE(results[0].success) << results[0].errorMessage.toStdString();
        ASSERT_EQ(renderedFrames, (std::vector<int>{0, 1, 2}));
    }

    QString emptyOutputPath = outputPattern;
    emptyOutputPath.replace(QStringLiteral("%04d"), QStringLiteral("0001"));
    ASSERT_TRUE(QFile::remove(emptyOutputPath));
    QFile emptyOutput(emptyOutputPath);
    ASSERT_TRUE(emptyOutput.open(QIODevice::WriteOnly));
    emptyOutput.close();

    master.submitJob(request);
    ASSERT_TRUE(waitFor(2));
    std::lock_guard lock(stateMutex);
    ASSERT_TRUE(results[1].success) << results[1].errorMessage.toStdString();
    EXPECT_EQ(renderedFrames, (std::vector<int>{0, 1, 2, 1}));
}

TEST(RenderFarmMasterCheckpointContractTest,
     ResubmittingCompletedJobRestoresExclusiveEndAndSkipsExistingFrames)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 2;
    master.setCheckpointPolicy(checkpointPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(completionMutex);
            completedResults.push_back(result);
        }
        completionChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("resume-contract-job");
    request.range = {10, 15, 1};
    const QString outputPattern = directory.filePath(QStringLiteral("frame_%04d.bin"));
    request.outputPath = outputPattern;
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(completionMutex);
            renderedFrames.push_back(frame);
        }
        QString outputPath = outputPattern;
        outputPath.replace(QStringLiteral("%04d"),
                           QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
        QFile output(outputPath);
        return output.open(QIODevice::WriteOnly) &&
               output.write(QByteArrayLiteral("frame")) == 5;
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
        EXPECT_TRUE(completedResults[0].success)
            << completedResults[0].errorMessage.toStdString();
        EXPECT_EQ(completedResults[0].renderedFrames, 5);
        EXPECT_EQ(renderedFrames, (std::vector<int>{10, 11, 12, 13, 14}));
    }

    auto checkpoint = master.checkpointStore()->load(request.jobId);
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->completedUpToFrame, 15)
        << "checkpoint end is exclusive for the [10, 15) range";

    master.submitJob(request);
    ASSERT_TRUE(waitForCompletions(2));
    {
        std::lock_guard lock(completionMutex);
        ASSERT_EQ(completedResults.size(), 2u);
        EXPECT_TRUE(completedResults[1].success)
            << completedResults[1].errorMessage.toStdString();
        EXPECT_EQ(completedResults[1].renderedFrames, 5);
        EXPECT_EQ(renderedFrames, (std::vector<int>{10, 11, 12, 13, 14}))
            << "the restored checkpoint and existing frame files avoid rerendering";
    }
}

TEST(RenderFarmMasterCheckpointContractTest,
     SteppedRangeCheckpointUsesExclusiveScheduledFrameBoundary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 2;
    master.setCheckpointPolicy(checkpointPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(completionMutex);
            completedResults.push_back(result);
        }
        completionChanged.notify_all();
    });

    RenderJobRequest request;
    request.jobId = QStringLiteral("stepped-checkpoint-contract-job");
    request.range = {10, 16, 2};
    const QString outputPattern = directory.filePath(QStringLiteral("frame_%04d.bin"));
    request.outputPath = outputPattern;
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(completionMutex);
            renderedFrames.push_back(frame);
        }
        QString outputPath = outputPattern;
        outputPath.replace(QStringLiteral("%04d"),
                           QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
        QFile output(outputPath);
        return output.open(QIODevice::WriteOnly) &&
               output.write(QByteArrayLiteral("frame")) == 5;
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
        EXPECT_TRUE(completedResults[0].success)
            << completedResults[0].errorMessage.toStdString();
        EXPECT_EQ(completedResults[0].renderedFrames, 3);
        EXPECT_EQ(renderedFrames, (std::vector<int>{10, 12, 14}));
    }

    const auto checkpoint = master.checkpointStore()->load(request.jobId);
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->completedUpToFrame, 16)
        << "the exclusive boundary after scheduled frame 14 is 16";

    master.submitJob(request);
    ASSERT_TRUE(waitForCompletions(2));
    {
        std::lock_guard lock(completionMutex);
        ASSERT_EQ(completedResults.size(), 2u);
        EXPECT_TRUE(completedResults[1].success)
            << completedResults[1].errorMessage.toStdString();
        EXPECT_EQ(completedResults[1].renderedFrames, 3);
        EXPECT_EQ(renderedFrames, (std::vector<int>{10, 12, 14}));
    }
}

TEST(RenderFarmMasterCheckpointContractTest,
     MidRangeSteppedCheckpointResumesAtNextScheduledFrameWithoutOvercounting)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> completedResults;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 2;
    master.setCheckpointPolicy(checkpointPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        completedResults.push_back(result);
    });

    bool cancelledAfterCheckpointInterval = false;
    RenderJobRequest request;
    request.jobId = QStringLiteral("stepped-midrange-resume-contract-job");
    request.range = {10, 16, 2};
    request.renderFrame = [&](int frame) {
        renderedFrames.push_back(frame);
        if (frame == 12 && !cancelledAfterCheckpointInterval) {
            cancelledAfterCheckpointInterval = true;
            master.cancelJob(request.jobId);
        }
        return true;
    };

    master.submitJob(request);
    ASSERT_TRUE(cancelledAfterCheckpointInterval);
    const auto checkpoint = master.checkpointStore()->load(request.jobId);
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->completedUpToFrame, 14)
        << "after frames 10 and 12, the exclusive scheduled boundary is 14";
    EXPECT_TRUE(completedResults.empty())
        << "the first attempt was cancelled before normal completion";

    master.submitJob(request);
    ASSERT_EQ(completedResults.size(), 1u);
    EXPECT_TRUE(completedResults.front().success)
        << completedResults.front().errorMessage.toStdString();
    EXPECT_EQ(completedResults.front().renderedFrames, 3);
    EXPECT_EQ(renderedFrames, (std::vector<int>{10, 12, 14}))
        << "resume must skip completed frames and render only frame 14";
}

TEST(RenderFarmMasterCheckpointContractTest,
     ReusedJobIdWithDifferentFrameCountDoesNotTreatNewRangeAsComplete)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 2;
    master.setCheckpointPolicy(checkpointPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(completionMutex);
            completedResults.push_back(result);
        }
        completionChanged.notify_all();
    });

    const auto makeRequest = [&](RenderFrameRange range,
                                 const QString& outputPrefix) {
        RenderJobRequest request;
        request.jobId = QStringLiteral("reused-range-contract-job");
        request.range = range;
        request.outputPath = directory.filePath(outputPrefix + QStringLiteral("%04d.bin"));
        const QString pattern = request.outputPath;
        request.renderFrame = [&](int frame) {
            renderedFrames.push_back(frame);
            QString path = pattern;
            path.replace(QStringLiteral("%04d"),
                         QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
            QFile output(path);
            return output.open(QIODevice::WriteOnly) &&
                   output.write(QByteArrayLiteral("frame")) == 5;
        };
        return request;
    };
    const auto waitForCompletions = [&](std::size_t expectedCount) {
        std::unique_lock lock(completionMutex);
        return completionChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return completedResults.size() >= expectedCount; });
    };

    const RenderJobRequest original = makeRequest(
        {10, 20, 1}, QStringLiteral("original_"));
    master.submitJob(original);
    ASSERT_TRUE(waitForCompletions(1));
    ASSERT_TRUE(completedResults[0].success)
        << completedResults[0].errorMessage.toStdString();
    const auto checkpoint = master.checkpointStore()->load(original.jobId);
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->totalFrames, 10);
    EXPECT_EQ(checkpoint->completedUpToFrame, 20);

    const RenderJobRequest changed = makeRequest(
        {0, 5, 1}, QStringLiteral("changed_"));
    master.submitJob(changed);
    ASSERT_TRUE(waitForCompletions(2));
    EXPECT_TRUE(completedResults[1].success)
        << completedResults[1].errorMessage.toStdString();
    EXPECT_EQ(completedResults[1].renderedFrames, 5);
    EXPECT_EQ(renderedFrames,
              (std::vector<int>{10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                0, 1, 2, 3, 4}));
}

TEST(RenderFarmMasterCheckpointContractTest,
     ReusedJobIdWithSameCountButEarlierRangeDoesNotReuseAbsoluteCheckpointEnd)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 2;
    master.setCheckpointPolicy(checkpointPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        {
            std::lock_guard lock(completionMutex);
            completedResults.push_back(result);
        }
        completionChanged.notify_all();
    });

    const auto makeRequest = [&](RenderFrameRange range,
                                 const QString& outputPrefix) {
        RenderJobRequest request;
        request.jobId = QStringLiteral("same-count-shifted-range-contract-job");
        request.range = range;
        request.outputPath = directory.filePath(outputPrefix + QStringLiteral("%04d.bin"));
        const QString pattern = request.outputPath;
        request.renderFrame = [&](int frame) {
            {
                std::lock_guard lock(completionMutex);
                renderedFrames.push_back(frame);
            }
            QString path = pattern;
            path.replace(QStringLiteral("%04d"),
                         QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
            QFile output(path);
            return output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                   output.write(QByteArrayLiteral("frame")) == 5;
        };
        return request;
    };
    const auto waitForCompletions = [&](std::size_t expectedCount) {
        std::unique_lock lock(completionMutex);
        return completionChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return completedResults.size() >= expectedCount; });
    };

    const RenderJobRequest original = makeRequest(
        {100, 110, 1}, QStringLiteral("original_"));
    master.submitJob(original);
    ASSERT_TRUE(waitForCompletions(1));
    ASSERT_TRUE(completedResults[0].success)
        << completedResults[0].errorMessage.toStdString();
    const auto checkpoint = master.checkpointStore()->load(original.jobId);
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->totalFrames, 10);
    EXPECT_EQ(checkpoint->completedUpToFrame, 110);

    const RenderJobRequest shiftedEarlier = makeRequest(
        {0, 10, 1}, QStringLiteral("shifted_"));
    master.submitJob(shiftedEarlier);
    ASSERT_TRUE(waitForCompletions(2));
    EXPECT_TRUE(completedResults[1].success)
        << completedResults[1].errorMessage.toStdString();
    EXPECT_EQ(completedResults[1].renderedFrames, 10);
    EXPECT_EQ(renderedFrames,
              (std::vector<int>{100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
                                0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(RenderFarmMasterCheckpointContractTest,
     ResumeDoesNotSkipEarlierFailedFrameWhenLaterFrameCompleted)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex completionMutex;
    std::condition_variable completionChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    int firstFrameAttempts = 0;
    RenderFarmMaster master(1);
    master.checkpointStore()->setBasePath(
        directory.filePath(QStringLiteral("checkpoints")));
    CheckpointPolicy checkpointPolicy;
    checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
    checkpointPolicy.interval = 1;
    master.setCheckpointPolicy(checkpointPolicy);
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
    request.jobId = QStringLiteral("failed-prefix-resume-contract-job");
    request.range = {0, 2, 1};
    request.outputPath = directory.filePath(QStringLiteral("frame_####.bin"));
    request.renderFrame = [&](int frame) {
        {
            std::lock_guard lock(completionMutex);
            renderedFrames.push_back(frame);
            if (frame == 0) ++firstFrameAttempts;
        }
        if (frame == 0 && firstFrameAttempts == 1) return false;

        QString outputPath = request.outputPath;
        outputPath.replace(QStringLiteral("####"),
                           QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
        QFile output(outputPath);
        return output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               output.write(QByteArrayLiteral("frame")) == 5;
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
        EXPECT_EQ(completedResults[0].failedFrames, 1);
        ASSERT_TRUE(completedResults[0].failures.isFrameHeld(0));
    }

    const auto checkpoint = master.checkpointStore()->load(request.jobId);
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->completedUpToFrame, 1);
    EXPECT_TRUE(checkpoint->failures.isFrameHeld(0));

    master.submitJob(request);
    ASSERT_TRUE(waitForCompletions(2));
    {
        std::lock_guard lock(completionMutex);
        ASSERT_EQ(completedResults.size(), 2u);
        EXPECT_TRUE(completedResults[1].success)
            << completedResults[1].errorMessage.toStdString();
        EXPECT_EQ(completedResults[1].failedFrames, 0);
        EXPECT_EQ(firstFrameAttempts, 2);
        EXPECT_EQ(renderedFrames, (std::vector<int>{0, 1, 0}));
        EXPECT_FALSE(completedResults[1].failures.isFrameHeld(0));
    }
}

TEST(RenderFarmMasterCheckpointContractTest,
     NewMasterResumesLastFailedFrameFromPersistedCheckpointAndOutputFiles)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<RenderJobResult> completedResults;
    std::vector<int> renderedFrames;
    const QString checkpointPath = directory.filePath(QStringLiteral("checkpoints"));
    const QString outputPattern = directory.filePath(QStringLiteral("frame_%04d.bin"));
    const auto makeRequest = [&](bool failLastFrame) {
        RenderJobRequest request;
        request.jobId = QStringLiteral("restart-resume-contract-job");
        request.range = {0, 3, 1};
        request.outputPath = outputPattern;
        request.renderFrame = [&, failLastFrame](int frame) {
            {
                std::lock_guard lock(stateMutex);
                renderedFrames.push_back(frame);
            }
            if (failLastFrame && frame == 2) return false;
            QString path = outputPattern;
            path.replace(QStringLiteral("%04d"),
                         QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
            QFile output(path);
            return output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                   output.write(QByteArrayLiteral("frame")) == 5;
        };
        return request;
    };
    const auto configure = [&](RenderFarmMaster& master) {
        master.checkpointStore()->setBasePath(checkpointPath);
        CheckpointPolicy checkpointPolicy;
        checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
        checkpointPolicy.interval = 1;
        master.setCheckpointPolicy(checkpointPolicy);
        RetryPolicy retryPolicy;
        retryPolicy.maxAttempts = 1;
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
    };
    const auto waitForCompletions = [&](std::size_t expectedCount) {
        std::unique_lock lock(stateMutex);
        return stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return completedResults.size() >= expectedCount; });
    };

    {
        RenderFarmMaster firstMaster(1);
        configure(firstMaster);
        firstMaster.submitJob(makeRequest(true));
        ASSERT_TRUE(waitForCompletions(1));
        {
            std::lock_guard lock(stateMutex);
            ASSERT_FALSE(completedResults[0].success);
            EXPECT_EQ(completedResults[0].failedFrames, 1);
            EXPECT_TRUE(completedResults[0].failures.isFrameHeld(2));
        }
    }

    CheckpointStore checkpointStore;
    checkpointStore.setBasePath(checkpointPath);
    const auto checkpoint = checkpointStore.load(
        QStringLiteral("restart-resume-contract-job"));
    ASSERT_TRUE(checkpoint.has_value());
    EXPECT_EQ(checkpoint->completedUpToFrame, 2);
    EXPECT_TRUE(checkpoint->failures.isFrameHeld(2));

    {
        RenderFarmMaster restartedMaster(1);
        configure(restartedMaster);
        restartedMaster.submitJob(makeRequest(false));
        ASSERT_TRUE(waitForCompletions(2));
        {
            std::lock_guard lock(stateMutex);
            ASSERT_TRUE(completedResults[1].success)
                << completedResults[1].errorMessage.toStdString();
            EXPECT_EQ(completedResults[1].renderedFrames, 3);
            EXPECT_EQ(renderedFrames,
                      (std::vector<int>{0, 1, 2, 2}));
            EXPECT_FALSE(completedResults[1].failures.isFrameHeld(2));
        }
    }
}

TEST(RenderFarmMasterCheckpointContractTest,
     OutOfOrderCompletionDoesNotAdvanceResumePastAnUnfinishedFrame)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    bool firstFrameEntered = false;
    bool releaseFirstFrame = false;
    std::vector<int> renderedFrames;
    std::vector<RenderJobResult> results;
    const QString checkpointPath = directory.filePath(QStringLiteral("frontier"));
    const QString outputPattern = directory.filePath(QStringLiteral("frame_%04d.bin"));
    const QString jobId = QStringLiteral("out-of-order-checkpoint-frontier-job");

    const auto configure = [&](RenderFarmMaster& master) {
        master.checkpointStore()->setBasePath(checkpointPath);
        CheckpointPolicy checkpointPolicy;
        checkpointPolicy.mode = CheckpointPolicy::Mode::EveryNFrames;
        checkpointPolicy.interval = 1;
        master.setCheckpointPolicy(checkpointPolicy);
        RetryPolicy retryPolicy;
        retryPolicy.maxAttempts = 1;
        retryPolicy.initialBackoffMs = 0;
        retryPolicy.maxBackoffMs = 0;
        master.setRetryPolicy(retryPolicy);
        master.setOnCompleted([&](const RenderJobResult& result) {
            {
                std::lock_guard lock(stateMutex);
                results.push_back(result);
            }
            stateChanged.notify_all();
        });
    };

    std::uint64_t earlyCheckpointBoundary = 0;
    bool earlyCheckpointObserved = false;
    bool firstFrameWasBlocked = false;
    {
        RenderFarmMaster firstMaster(2);
        configure(firstMaster);
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {0, 2, 1};
        request.outputPath = outputPattern;
        request.renderFrame = [&](int frame) {
            if (frame == 0) {
                std::unique_lock lock(stateMutex);
                renderedFrames.push_back(frame);
                firstFrameEntered = true;
                stateChanged.notify_all();
                stateChanged.wait(lock, [&] { return releaseFirstFrame; });
                return false;
            }

            {
                std::lock_guard lock(stateMutex);
                renderedFrames.push_back(frame);
            }
            QString framePath = outputPattern;
            framePath.replace(QStringLiteral("%04d"),
                              QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
            QFile output(framePath);
            return output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                   output.write(QByteArrayLiteral("frame")) == 5;
        };

        firstMaster.submitJob(request);
        {
            std::unique_lock lock(stateMutex);
            firstFrameWasBlocked = stateChanged.wait_for(
                lock, std::chrono::seconds(5), [&] { return firstFrameEntered; });
        }

        const auto checkpointDeadline = std::chrono::steady_clock::now() +
                                        std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < checkpointDeadline) {
            const auto checkpoint = firstMaster.checkpointStore()->load(jobId);
            if (checkpoint) {
                earlyCheckpointObserved = true;
                earlyCheckpointBoundary = checkpoint->completedUpToFrame;
                break;
            }
            std::this_thread::yield();
        }

        {
            std::lock_guard lock(stateMutex);
            releaseFirstFrame = true;
        }
        stateChanged.notify_all();
        {
            std::unique_lock lock(stateMutex);
            stateChanged.wait_for(lock, std::chrono::seconds(10),
                                  [&] { return !results.empty(); });
        }
    }

    EXPECT_TRUE(firstFrameWasBlocked);
    if (earlyCheckpointObserved)
        EXPECT_LE(earlyCheckpointBoundary, 0u);
    {
        std::lock_guard lock(stateMutex);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_FALSE(results.front().success);
        EXPECT_EQ(results.front().failedFrames, 1);
        EXPECT_EQ(renderedFrames, (std::vector<int>{0, 1}));
    }

    CheckpointStore persistedStore;
    persistedStore.setBasePath(checkpointPath);
    const auto persisted = persistedStore.load(jobId);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_LE(persisted->completedUpToFrame, 0);
    EXPECT_TRUE(persisted->failures.isFrameHeld(0));

    {
        RenderFarmMaster restartedMaster(1);
        configure(restartedMaster);
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {0, 2, 1};
        request.outputPath = outputPattern;
        request.renderFrame = [&](int frame) {
            {
                std::lock_guard lock(stateMutex);
                renderedFrames.push_back(frame);
            }
            QString framePath = outputPattern;
            framePath.replace(QStringLiteral("%04d"),
                              QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
            QFile output(framePath);
            return output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                   output.write(QByteArrayLiteral("frame")) == 5;
        };
        restartedMaster.submitJob(request);
        std::unique_lock lock(stateMutex);
        ASSERT_TRUE(stateChanged.wait_for(lock, std::chrono::seconds(10),
                                          [&] { return results.size() >= 2; }));
        ASSERT_EQ(results.size(), 2u);
        EXPECT_TRUE(results.back().success)
            << results.back().errorMessage.toStdString();
        EXPECT_EQ(renderedFrames, (std::vector<int>{0, 1, 0}));
    }
}
