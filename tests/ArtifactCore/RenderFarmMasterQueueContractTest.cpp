#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

import Render.Farm.Master;
import Render.Farm.Types;

using namespace ArtifactCore;

TEST(RenderFarmMasterQueueContractTest,
     SchedulerRunsHighestPriorityReadyJobAndHonorsForwardDependencies)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<QString> renderedJobs;
    std::vector<QString> completedJobs;
    int successfulJobs = 0;
    bool blockerStarted = false;
    bool releaseBlocker = false;

    RenderFarmMaster master(1);
    master.setOnCompleted([&](const RenderJobResult& result) {
        // The current job ID is not part of RenderJobResult, so completion
        // order is captured by the corresponding frame callback below.
        std::lock_guard lock(stateMutex);
        completedJobs.push_back(renderedJobs.back());
        if (result.success) ++successfulJobs;
        stateChanged.notify_all();
    });

    const auto makeRequest = [&](const QString& jobId, int priority,
                                 const QStringList& dependencies) {
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {0, 1, 1};
        request.priority = priority;
        request.dependencies = dependencies;
        request.renderFrame = [&, jobId](int) {
            std::unique_lock lock(stateMutex);
            renderedJobs.push_back(jobId);
            if (jobId == QStringLiteral("blocking-job")) {
                blockerStarted = true;
                stateChanged.notify_all();
                return stateChanged.wait_for(lock, std::chrono::seconds(5),
                    [&] { return releaseBlocker; });
            }
            return true;
        };
        return request;
    };

    master.submitJob(makeRequest(QStringLiteral("blocking-job"), 200, {}));
    {
        std::unique_lock lock(stateMutex);
        const bool started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
        if (!started) {
            releaseBlocker = true;
            stateChanged.notify_all();
        }
        ASSERT_TRUE(started) << "the first job must be active before queueing others";
    }

    // Queue the high-priority dependent before its prerequisite. While the
    // dependency is unresolved, the scheduler should run the best ready job.
    master.submitJob(makeRequest(QStringLiteral("dependent-high"), 100,
                                 {QStringLiteral("prerequisite-low")}));
    master.submitJob(makeRequest(QStringLiteral("independent-mid"), 50));
    master.submitJob(makeRequest(QStringLiteral("prerequisite-low"), 10));

    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();

    {
        std::unique_lock lock(stateMutex);
        ASSERT_TRUE(stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return completedJobs.size() == 4; }));
        EXPECT_EQ(renderedJobs, (std::vector<QString>{
            QStringLiteral("blocking-job"),
            QStringLiteral("independent-mid"),
            QStringLiteral("prerequisite-low"),
            QStringLiteral("dependent-high")}));
        EXPECT_EQ(completedJobs, renderedJobs);
        EXPECT_EQ(successfulJobs, 4);
    }
}

TEST(RenderFarmMasterQueueContractTest,
     SchedulerResolvesForwardDependencyChainAcrossMultipleQueuePasses)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<QString> renderedJobs;
    std::vector<bool> completionSuccess;
    bool blockerStarted = false;
    bool releaseBlocker = false;

    RenderFarmMaster master(1);
    master.setOnCompleted([&](const RenderJobResult& result) {
        std::lock_guard lock(stateMutex);
        completionSuccess.push_back(result.success);
        stateChanged.notify_all();
    });

    const auto makeRequest = [&](const QString& jobId, int priority,
                                 const QStringList& dependencies) {
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {0, 1, 1};
        request.priority = priority;
        request.dependencies = dependencies;
        request.renderFrame = [&, jobId](int) {
            std::unique_lock lock(stateMutex);
            renderedJobs.push_back(jobId);
            if (jobId == QStringLiteral("chain-blocker")) {
                blockerStarted = true;
                stateChanged.notify_all();
                return stateChanged.wait_for(lock, std::chrono::seconds(5),
                    [&] { return releaseBlocker; });
            }
            return true;
        };
        return request;
    };

    master.submitJob(makeRequest(QStringLiteral("chain-blocker"), 200, {}));
    bool started = false;
    {
        std::unique_lock lock(stateMutex);
        started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
    }
    if (!started) {
        {
            std::lock_guard lock(stateMutex);
            releaseBlocker = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(started) << "the blocker must be active before queueing the graph";
        return;
    }

    // Queue the leaf and middle before their prerequisites. Readiness must be
    // reconsidered after each completed job, while unrelated ready work wins
    // over a newly unblocked job with lower priority.
    master.submitJob(makeRequest(QStringLiteral("graph-leaf"), 100,
        {QStringLiteral("graph-middle")}));
    master.submitJob(makeRequest(QStringLiteral("graph-middle"), 80,
        {QStringLiteral("graph-root")}));
    master.submitJob(makeRequest(QStringLiteral("graph-independent"), 50, {}));
    master.submitJob(makeRequest(QStringLiteral("graph-root"), 10, {}));

    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();

    bool completed = false;
    {
        std::unique_lock lock(stateMutex);
        completed = stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return completionSuccess.size() == 5; });
        if (completed) {
            EXPECT_EQ(renderedJobs, (std::vector<QString>{
                QStringLiteral("chain-blocker"),
                QStringLiteral("graph-independent"),
                QStringLiteral("graph-root"),
                QStringLiteral("graph-middle"),
                QStringLiteral("graph-leaf")}));
            EXPECT_EQ(completionSuccess, (std::vector<bool>{true, true, true, true, true}));
        }
    }
    if (!completed) master.cancelAll();
    EXPECT_TRUE(completed);
}

TEST(RenderFarmMasterQueueContractTest,
     PriorityBatchAndCancellationPersistTheExactRemainingQueue)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString queueFile = directory.filePath(QStringLiteral("render-queue.json"));

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    bool blockerStarted = false;
    bool releaseBlocker = false;
    int completedJobs = 0;

    RenderFarmMaster master(1);
    ASSERT_TRUE(master.saveQueue(queueFile));
    master.setOnCompleted([&](const RenderJobResult&) {
        std::lock_guard lock(stateMutex);
        ++completedJobs;
        stateChanged.notify_all();
    });

    RenderJobRequest blocker;
    blocker.jobId = QStringLiteral("queue-mutation-blocker");
    blocker.range = {0, 1, 1};
    blocker.priority = 200;
    blocker.renderFrame = [&](int) {
        std::unique_lock lock(stateMutex);
        blockerStarted = true;
        stateChanged.notify_all();
        return stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return releaseBlocker; });
    };
    master.submitJob(blocker);

    bool started = false;
    {
        std::unique_lock lock(stateMutex);
        started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
    }
    if (!started) {
        {
            std::lock_guard lock(stateMutex);
            releaseBlocker = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(started) << "the blocker must be running while queue mutations occur";
        return;
    }

    const auto queuedRequest = [](const QString& jobId, int priority) {
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {10, 20, 2};
        request.priority = priority;
        request.renderPayload = QJsonObject{{QStringLiteral("job"), jobId}};
        request.rendererExecutable = QStringLiteral("artifact-render-worker");
        request.compositionName = QStringLiteral("queue-round-trip-composition");
        request.outputPath = QStringLiteral("renders/%1.exr").arg(jobId);
        request.enableAudio = true;
        request.dependencies = {QStringLiteral("prerequisite-a"),
                                QStringLiteral("prerequisite-b")};
        request.jobPool = QStringLiteral("round-trip-pool");
        request.allowedWorkerIds = {QStringLiteral("worker-a"), QStringLiteral("worker-b")};
        request.jobTimeoutMs = 12345;
        request.frameTimeoutMs = 678;
        request.requiredCapabilities = QJsonObject{
            {QStringLiteral("gpu"), true},
            {QStringLiteral("memoryGb"), 12},
        };
        return request;
    };
    master.submitJob(queuedRequest(QStringLiteral("queued-a"), 10));
    master.submitJob(queuedRequest(QStringLiteral("queued-b"), 20));
    master.submitJob(queuedRequest(QStringLiteral("queued-c"), 15));

    EXPECT_TRUE(master.setQueuedJobPriority(QStringLiteral("queued-a"), 25));
    EXPECT_EQ(master.setQueuedJobPriorities(
        {QStringLiteral("queued-c"), QStringLiteral("missing-job")}, 30), 1);
    EXPECT_EQ(master.cancelJobs(
        {QStringLiteral("queued-b"), QStringLiteral("missing-job")}), 1);

    QFile file(queueFile);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    ASSERT_TRUE(document.isObject());
    const QJsonArray jobs = document.object().value(QStringLiteral("jobs")).toArray();
    ASSERT_EQ(jobs.size(), 2);
    EXPECT_EQ(jobs[0].toObject().value(QStringLiteral("jobId")).toString(),
              QStringLiteral("queued-c"));
    EXPECT_EQ(jobs[0].toObject().value(QStringLiteral("priority")).toInt(), 30);
    EXPECT_EQ(jobs[1].toObject().value(QStringLiteral("jobId")).toString(),
              QStringLiteral("queued-a"));
    EXPECT_EQ(jobs[1].toObject().value(QStringLiteral("priority")).toInt(), 25);
    file.close();

    const QString restoreFile = directory.filePath(QStringLiteral("restore-queue.json"));
    ASSERT_TRUE(QFile::copy(queueFile, restoreFile));

    EXPECT_EQ(master.clearQueuedJobs(), 2);
    QFile emptyQueue(queueFile);
    ASSERT_TRUE(emptyQueue.open(QIODevice::ReadOnly));
    const QJsonDocument emptyDocument = QJsonDocument::fromJson(emptyQueue.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    EXPECT_TRUE(emptyDocument.object().value(QStringLiteral("jobs")).toArray().isEmpty());

    // Restore while the blocker is active, so the deserialized requests stay
    // queued and can be inspected without invoking a renderer.
    EXPECT_TRUE(master.loadQueue(restoreFile));
    const QString restoredSnapshotPath = directory.filePath(
        QStringLiteral("restored-snapshot.json"));
    ASSERT_TRUE(master.saveQueue(restoredSnapshotPath));
    QFile restoredSnapshot(restoredSnapshotPath);
    ASSERT_TRUE(restoredSnapshot.open(QIODevice::ReadOnly));
    const QJsonDocument restoredDocument = QJsonDocument::fromJson(
        restoredSnapshot.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    const QJsonArray restoredJobs = restoredDocument.object()
        .value(QStringLiteral("jobs")).toArray();
    ASSERT_EQ(restoredJobs.size(), 2);
    const QJsonObject restoredA = restoredJobs[1].toObject();
    EXPECT_EQ(restoredA.value(QStringLiteral("jobId")).toString(),
              QStringLiteral("queued-a"));
    EXPECT_EQ(restoredA.value(QStringLiteral("priority")).toInt(), 25);
    EXPECT_EQ(restoredA.value(QStringLiteral("compositionName")).toString(),
              QStringLiteral("queue-round-trip-composition"));
    EXPECT_EQ(restoredA.value(QStringLiteral("outputPath")).toString(),
              QStringLiteral("renders/queued-a.exr"));
    EXPECT_FALSE(restoredA.value(QStringLiteral("autoVersionOutput")).toBool());
    EXPECT_EQ(restoredA.value(QStringLiteral("startFrame")).toInt(), 10);
    EXPECT_EQ(restoredA.value(QStringLiteral("endFrame")).toInt(), 20);
    EXPECT_EQ(restoredA.value(QStringLiteral("step")).toInt(), 2);
    EXPECT_EQ(restoredA.value(QStringLiteral("dependencies")).toArray(),
              (QJsonArray{QStringLiteral("prerequisite-a"),
                          QStringLiteral("prerequisite-b")}));
    EXPECT_EQ(restoredA.value(QStringLiteral("allowedWorkerIds")).toArray(),
              (QJsonArray{QStringLiteral("worker-a"), QStringLiteral("worker-b")}));
    EXPECT_EQ(restoredA.value(QStringLiteral("jobTimeoutMs")).toInt(), 12345);
    EXPECT_EQ(restoredA.value(QStringLiteral("frameTimeoutMs")).toInt(), 678);
    EXPECT_TRUE(restoredA.value(QStringLiteral("enableAudio")).toBool());
    EXPECT_EQ(restoredA.value(QStringLiteral("jobPool")).toString(),
              QStringLiteral("round-trip-pool"));
    EXPECT_EQ(restoredA.value(QStringLiteral("requiredCapabilities")).toObject(),
              (QJsonObject{{QStringLiteral("gpu"), true},
                           {QStringLiteral("memoryGb"), 12}}));
    EXPECT_EQ(restoredA.value(QStringLiteral("renderPayload")).toObject(),
              (QJsonObject{{QStringLiteral("job"), QStringLiteral("queued-a")}}));
    EXPECT_EQ(restoredA.value(QStringLiteral("rendererExecutable")).toString(),
              QStringLiteral("artifact-render-worker"));
    restoredSnapshot.close();
    EXPECT_EQ(master.clearQueuedJobs(), 2);

    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();
    bool blockerCompleted = false;
    {
        std::unique_lock lock(stateMutex);
        blockerCompleted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return completedJobs == 1; });
    }
    if (!blockerCompleted) master.cancelAll();
    EXPECT_TRUE(blockerCompleted);
}

TEST(RenderFarmMasterQueueContractTest,
     RejectsFutureQueueSchemaWithoutAddingJobsToExistingQueue)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString queuePath = directory.filePath(QStringLiteral("future-queue.json"));
    const QString snapshotPath = directory.filePath(QStringLiteral("current-queue.json"));

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    bool blockerStarted = false;
    bool releaseBlocker = false;
    int completedJobs = 0;
    RenderFarmMaster master(1);
    master.setOnCompleted([&](const RenderJobResult&) {
        std::lock_guard lock(stateMutex);
        ++completedJobs;
        stateChanged.notify_all();
    });

    ASSERT_TRUE(master.saveQueue(snapshotPath));
    RenderJobRequest blocker;
    blocker.jobId = QStringLiteral("future-schema-blocker");
    blocker.range = {0, 1, 1};
    blocker.renderFrame = [&](int) {
        std::unique_lock lock(stateMutex);
        blockerStarted = true;
        stateChanged.notify_all();
        return stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return releaseBlocker; });
    };
    master.submitJob(blocker);
    bool started = false;
    {
        std::unique_lock lock(stateMutex);
        started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
    }
    if (!started) {
        {
            std::lock_guard lock(stateMutex);
            releaseBlocker = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(started) << "the blocker must be active before queue loading";
        return;
    }

    const QJsonObject futureJob{
        {QStringLiteral("jobId"), QStringLiteral("future-format-job")},
        {QStringLiteral("startFrame"), 0},
        {QStringLiteral("endFrame"), 1},
        {QStringLiteral("step"), 1},
        {QStringLiteral("outputPath"), directory.filePath(QStringLiteral("future.bin"))},
        {QStringLiteral("renderPayload"),
         QJsonObject{{QStringLiteral("request"), QStringLiteral("valid-shape")}}},
        {QStringLiteral("rendererExecutable"), QStringLiteral("artifact-render-worker")},
    };
    QFile futureQueue(queuePath);
    ASSERT_TRUE(futureQueue.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray futureDocument = QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 99},
        {QStringLiteral("jobs"), QJsonArray{futureJob}},
    }).toJson(QJsonDocument::Compact);
    ASSERT_EQ(futureQueue.write(futureDocument), futureDocument.size());
    futureQueue.close();

    EXPECT_FALSE(master.loadQueue(queuePath));
    ASSERT_TRUE(master.saveQueue(snapshotPath));
    QFile currentQueue(snapshotPath);
    ASSERT_TRUE(currentQueue.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument currentDocument = QJsonDocument::fromJson(
        currentQueue.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    EXPECT_TRUE(currentDocument.object().value(QStringLiteral("jobs")).toArray().isEmpty());

    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();
    bool blockerCompleted = false;
    {
        std::unique_lock lock(stateMutex);
        blockerCompleted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return completedJobs == 1; });
    }
    if (!blockerCompleted) master.cancelAll();
    EXPECT_TRUE(blockerCompleted);
}

TEST(RenderFarmMasterQueueContractTest,
     MalformedQueueEntryDoesNotPartiallyAppendEarlierValidJobs)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString queuePath = directory.filePath(QStringLiteral("existing-queue.json"));
    const QString malformedPath = directory.filePath(QStringLiteral("malformed-queue.json"));
    const QString snapshotPath = directory.filePath(QStringLiteral("queue-snapshot.json"));

    std::mutex stateMutex;
    std::condition_variable stateChanged;
    bool blockerStarted = false;
    bool releaseBlocker = false;
    int completedJobs = 0;

    RenderFarmMaster master(1);
    ASSERT_TRUE(master.saveQueue(queuePath));
    master.setOnCompleted([&](const RenderJobResult&) {
        std::lock_guard lock(stateMutex);
        ++completedJobs;
        stateChanged.notify_all();
    });

    RenderJobRequest blocker;
    blocker.jobId = QStringLiteral("malformed-queue-blocker");
    blocker.range = {0, 1, 1};
    blocker.renderFrame = [&](int) {
        std::unique_lock lock(stateMutex);
        blockerStarted = true;
        stateChanged.notify_all();
        return stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return releaseBlocker; });
    };
    master.submitJob(blocker);

    bool started = false;
    {
        std::unique_lock lock(stateMutex);
        started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
    }
    if (!started) {
        {
            std::lock_guard lock(stateMutex);
            releaseBlocker = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(started) << "the blocker must be active before queue loading";
        return;
    }

    RenderJobRequest existing;
    existing.jobId = QStringLiteral("preexisting-queued-job");
    existing.range = {0, 1, 1};
    existing.renderPayload = QJsonObject{{QStringLiteral("job"), existing.jobId}};
    existing.rendererExecutable = QStringLiteral("artifact-render-worker");
    master.submitJob(existing);

    const QJsonObject validJob{
        {QStringLiteral("jobId"), QStringLiteral("valid-prefix-job")},
        {QStringLiteral("startFrame"), 0},
        {QStringLiteral("endFrame"), 1},
        {QStringLiteral("step"), 1},
        {QStringLiteral("renderPayload"),
         QJsonObject{{QStringLiteral("job"), QStringLiteral("valid-prefix-job")}}},
        {QStringLiteral("rendererExecutable"), QStringLiteral("artifact-render-worker")},
    };
    QJsonObject invalidJob = validJob;
    invalidJob.insert(QStringLiteral("jobId"), QStringLiteral("empty-range-job"));
    invalidJob.insert(QStringLiteral("startFrame"), 4);
    invalidJob.insert(QStringLiteral("endFrame"), 4);
    invalidJob.insert(QStringLiteral("renderPayload"),
        QJsonObject{{QStringLiteral("job"), QStringLiteral("empty-range-job")}});
    QFile malformedFile(malformedPath);
    ASSERT_TRUE(malformedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray malformedDocument = QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("jobs"), QJsonArray{validJob, invalidJob}},
    }).toJson(QJsonDocument::Compact);
    ASSERT_EQ(malformedFile.write(malformedDocument), malformedDocument.size());
    malformedFile.close();

    EXPECT_FALSE(master.loadQueue(malformedPath));
    ASSERT_TRUE(master.saveQueue(snapshotPath));
    QFile snapshotFile(snapshotPath);
    ASSERT_TRUE(snapshotFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument snapshot = QJsonDocument::fromJson(
        snapshotFile.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError);
    const QJsonArray queuedJobs = snapshot.object().value(QStringLiteral("jobs")).toArray();
    ASSERT_EQ(queuedJobs.size(), 1);
    EXPECT_EQ(queuedJobs.front().toObject().value(QStringLiteral("jobId")).toString(),
              QStringLiteral("preexisting-queued-job"));

    EXPECT_EQ(master.clearQueuedJobs(), 1);
    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();
    bool blockerCompleted = false;
    {
        std::unique_lock lock(stateMutex);
        blockerCompleted = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return completedJobs == 1; });
    }
    if (!blockerCompleted) master.cancelAll();
    EXPECT_TRUE(blockerCompleted);
}

TEST(RenderFarmMasterQueueContractTest,
     FailedDependencyPrunesTransitiveJobsWithoutStarvingIndependentQueue)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<QString> renderedJobs;
    std::vector<bool> completionSuccess;
    bool blockerStarted = false;
    bool releaseBlocker = false;

    RenderFarmMaster master(1);
    RetryPolicy retryPolicy;
    retryPolicy.maxAttempts = 1;
    retryPolicy.initialBackoffMs = 0;
    retryPolicy.maxBackoffMs = 0;
    master.setRetryPolicy(retryPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        std::lock_guard lock(stateMutex);
        completionSuccess.push_back(result.success);
        stateChanged.notify_all();
    });

    const auto makeRequest = [&](const QString& jobId, int priority,
                                 const QStringList& dependencies,
                                 bool frameSucceeds) {
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {0, 1, 1};
        request.priority = priority;
        request.dependencies = dependencies;
        request.renderFrame = [&, jobId, frameSucceeds](int) {
            std::unique_lock lock(stateMutex);
            renderedJobs.push_back(jobId);
            if (jobId == QStringLiteral("failure-chain-blocker")) {
                blockerStarted = true;
                stateChanged.notify_all();
                return stateChanged.wait_for(lock, std::chrono::seconds(5),
                    [&] { return releaseBlocker; });
            }
            return frameSucceeds;
        };
        return request;
    };

    master.submitJob(makeRequest(QStringLiteral("failure-chain-blocker"), 200, {}, true));
    bool started = false;
    {
        std::unique_lock lock(stateMutex);
        started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
    }
    if (!started) {
        {
            std::lock_guard lock(stateMutex);
            releaseBlocker = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(started);
        return;
    }

    master.submitJob(makeRequest(QStringLiteral("dependent-root"), 100,
        {QStringLiteral("failing-root")}, true));
    master.submitJob(makeRequest(QStringLiteral("transitive-dependent"), 90,
        {QStringLiteral("dependent-root")}, true));
    master.submitJob(makeRequest(QStringLiteral("independent-job"), 50, {}, true));
    master.submitJob(makeRequest(QStringLiteral("failing-root"), 10, {}, false));
    master.submitJob(makeRequest(QStringLiteral("queue-drained-sentinel"), 1, {}, true));

    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();

    {
        std::unique_lock lock(stateMutex);
        ASSERT_TRUE(stateChanged.wait_for(lock, std::chrono::seconds(10),
            [&] { return renderedJobs.size() == 4 && completionSuccess.size() == 4; }));
        EXPECT_EQ(renderedJobs, (std::vector<QString>{
            QStringLiteral("failure-chain-blocker"),
            QStringLiteral("independent-job"),
            QStringLiteral("failing-root"),
            QStringLiteral("queue-drained-sentinel")}));
        EXPECT_EQ(completionSuccess, (std::vector<bool>{true, true, false, true}));
    }
}

TEST(RenderFarmMasterQueueContractTest,
     FailedExternalPrerequisitePrunesCyclicDependentsAndQueueContinues)
{
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<QString> renderedJobs;
    std::vector<bool> completionSuccess;
    bool blockerStarted = false;
    bool releaseBlocker = false;

    RenderFarmMaster master(1);
    RetryPolicy retryPolicy;
    retryPolicy.maxAttempts = 1;
    retryPolicy.initialBackoffMs = 0;
    retryPolicy.maxBackoffMs = 0;
    master.setRetryPolicy(retryPolicy);
    master.setOnCompleted([&](const RenderJobResult& result) {
        std::lock_guard lock(stateMutex);
        completionSuccess.push_back(result.success);
        stateChanged.notify_all();
    });

    const auto makeRequest = [&](const QString& jobId, int priority,
                                 const QStringList& dependencies,
                                 bool frameSucceeds) {
        RenderJobRequest request;
        request.jobId = jobId;
        request.range = {0, 1, 1};
        request.priority = priority;
        request.dependencies = dependencies;
        request.renderFrame = [&, jobId, frameSucceeds](int) {
            std::unique_lock lock(stateMutex);
            renderedJobs.push_back(jobId);
            if (jobId == QStringLiteral("cycle-prune-blocker")) {
                blockerStarted = true;
                stateChanged.notify_all();
                return stateChanged.wait_for(lock, std::chrono::seconds(5),
                    [&] { return releaseBlocker; });
            }
            return frameSucceeds;
        };
        return request;
    };

    master.submitJob(makeRequest(QStringLiteral("cycle-prune-blocker"), 200, {}, true));
    bool started = false;
    {
        std::unique_lock lock(stateMutex);
        started = stateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return blockerStarted; });
    }
    if (!started) {
        {
            std::lock_guard lock(stateMutex);
            releaseBlocker = true;
        }
        stateChanged.notify_all();
        EXPECT_TRUE(started) << "the blocker must run before dependency jobs are queued";
        return;
    }

    master.submitJob(makeRequest(QStringLiteral("cycle-a"), 100,
        {QStringLiteral("cycle-b"), QStringLiteral("failed-prerequisite")}, true));
    master.submitJob(makeRequest(QStringLiteral("cycle-b"), 90,
        {QStringLiteral("cycle-a")}, true));
    master.submitJob(makeRequest(QStringLiteral("cycle-independent"), 50, {}, true));
    master.submitJob(makeRequest(QStringLiteral("failed-prerequisite"), 10, {}, false));
    master.submitJob(makeRequest(QStringLiteral("cycle-prune-sentinel"), 1, {}, true));

    {
        std::lock_guard lock(stateMutex);
        releaseBlocker = true;
    }
    stateChanged.notify_all();

    bool completed = false;
    {
        std::unique_lock lock(stateMutex);
        completed = stateChanged.wait_for(lock, std::chrono::seconds(10), [&] {
            return renderedJobs.size() == 4 && completionSuccess.size() == 4;
        });
        if (completed) {
            EXPECT_EQ(renderedJobs, (std::vector<QString>{
                QStringLiteral("cycle-prune-blocker"),
                QStringLiteral("cycle-independent"),
                QStringLiteral("failed-prerequisite"),
                QStringLiteral("cycle-prune-sentinel")}));
            EXPECT_EQ(completionSuccess, (std::vector<bool>{true, true, false, true}));
        }
    }
    if (!completed) master.cancelAll();
    EXPECT_TRUE(completed);
}
