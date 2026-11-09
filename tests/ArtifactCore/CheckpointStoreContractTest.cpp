#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <thread>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QStringList>
#include <QTemporaryDir>

import Render.Farm.Checkpoint;
import Render.Farm.Types;

using namespace ArtifactCore;

TEST(CheckpointStoreContractTest,
     ConcurrentReadersObserveCoherentCheckpointGenerations)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());

    constexpr int writerCount = 3;
    constexpr int generationsPerWriter = 600;
    std::atomic<int> ready{0};
    std::atomic<int> finishedWriters{0};
    std::atomic<bool> snapshotsWereCoherent{true};
    std::atomic<int> observedSnapshots{0};

    const auto makeCheckpoint = [](int generation) {
        CheckpointInfo checkpoint;
        checkpoint.jobId = QStringLiteral("concurrent-generation-job");
        checkpoint.completedUpToFrame = generation;
        checkpoint.totalFrames = generation + 100;
        checkpoint.failures.totalRetries = generation;
        checkpoint.failures.addFailure(
            generation, generation + 1, QString::number(generation));
        return checkpoint;
    };

    std::thread reader([&] {
        ready.fetch_add(1, std::memory_order_release);
        while (ready.load(std::memory_order_acquire) != writerCount + 1) {
            std::this_thread::yield();
        }
        do {
            const auto snapshot = store.load(
                QStringLiteral("concurrent-generation-job"));
            if (!snapshot) continue;
            observedSnapshots.fetch_add(1, std::memory_order_relaxed);
            const bool coherent = snapshot->completedUpToFrame > 0 &&
                snapshot->totalFrames == snapshot->completedUpToFrame + 100 &&
                snapshot->failures.totalRetries == snapshot->completedUpToFrame &&
                snapshot->failures.failedFrames.size() == 1 &&
                snapshot->failures.failedFrames.front().frame ==
                    snapshot->completedUpToFrame &&
                snapshot->failures.failedFrames.front().attempt ==
                    snapshot->completedUpToFrame + 1 &&
                snapshot->failures.failedFrames.front().errorMessage ==
                    QString::number(snapshot->completedUpToFrame);
            if (!coherent) {
                snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                break;
            }
        } while (finishedWriters.load(std::memory_order_acquire) != writerCount);
    });

    std::thread writers[writerCount];
    for (int writerIndex = 0; writerIndex < writerCount; ++writerIndex) {
        writers[writerIndex] = std::thread([&, writerIndex] {
            ready.fetch_add(1, std::memory_order_release);
            while (ready.load(std::memory_order_acquire) != writerCount + 1) {
                std::this_thread::yield();
            }
            for (int offset = 1; offset <= generationsPerWriter; ++offset) {
                const int generation =
                    writerIndex * generationsPerWriter + offset;
                if (!store.save(makeCheckpoint(generation))) {
                    snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                    break;
                }
            }
            finishedWriters.fetch_add(1, std::memory_order_release);
        });
    }

    reader.join();
    for (auto& writer : writers) writer.join();

    EXPECT_TRUE(snapshotsWereCoherent.load(std::memory_order_relaxed));
    EXPECT_GT(observedSnapshots.load(std::memory_order_relaxed), 0);
    const auto finalSnapshot = store.load(
        QStringLiteral("concurrent-generation-job"));
    ASSERT_TRUE(finalSnapshot.has_value());
    EXPECT_TRUE(finalSnapshot->completedUpToFrame > 0);
    EXPECT_EQ(finalSnapshot->totalFrames,
              finalSnapshot->completedUpToFrame + 100);
    EXPECT_EQ(finalSnapshot->failures.totalRetries,
              finalSnapshot->completedUpToFrame);
}

TEST(CheckpointStoreContractTest,
     IndependentStoreInstancesKeepCheckpointReadableDuringConcurrentSaves)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    constexpr int writerCount = 2;
    constexpr int generationsPerWriter = 120;
    constexpr int failureRecordsPerGeneration = 64;
    const QString jobId = QStringLiteral("shared-store-instance-job");
    const auto makeCheckpoint = [=](int generation) {
        CheckpointInfo checkpoint;
        checkpoint.jobId = jobId;
        checkpoint.completedUpToFrame = generation;
        checkpoint.totalFrames = generation + 100;
        checkpoint.failures.totalRetries = generation;
        for (int record = 0; record < failureRecordsPerGeneration; ++record) {
            checkpoint.failures.addFailure(
                generation, generation + 1, QString::number(generation));
        }
        return checkpoint;
    };

    CheckpointStore seedStore;
    seedStore.setBasePath(directory.path());
    ASSERT_TRUE(seedStore.save(makeCheckpoint(0)));

    std::atomic<int> readyWriters{0};
    std::atomic<int> finishedWriters{0};
    std::atomic<bool> begin{false};
    std::atomic<bool> snapshotsWereValid{true};
    std::atomic<int> observedSnapshots{0};

    std::thread reader([&] {
        CheckpointStore readerStore;
        readerStore.setBasePath(directory.path());
        while (!begin.load(std::memory_order_acquire))
            std::this_thread::yield();

        do {
            const auto snapshot = readerStore.load(jobId);
            if (!snapshot) {
                snapshotsWereValid.store(false, std::memory_order_relaxed);
                continue;
            }
            observedSnapshots.fetch_add(1, std::memory_order_relaxed);
            const int generation = snapshot->completedUpToFrame;
            bool coherent = generation >= 0 &&
                snapshot->totalFrames == generation + 100 &&
                snapshot->failures.totalRetries == generation &&
                snapshot->failures.failedFrames.size() ==
                    static_cast<std::size_t>(failureRecordsPerGeneration);
            for (const auto& failure : snapshot->failures.failedFrames) {
                coherent = coherent && failure.frame == generation &&
                    failure.attempt == generation + 1 &&
                    failure.errorMessage == QString::number(generation);
            }
            if (!coherent)
                snapshotsWereValid.store(false, std::memory_order_relaxed);
        } while (finishedWriters.load(std::memory_order_acquire) != writerCount);
    });

    std::thread writers[writerCount];
    for (int writerIndex = 0; writerIndex < writerCount; ++writerIndex) {
        writers[writerIndex] = std::thread([&, writerIndex] {
            CheckpointStore writerStore;
            writerStore.setBasePath(directory.path());
            readyWriters.fetch_add(1, std::memory_order_release);
            while (!begin.load(std::memory_order_acquire))
                std::this_thread::yield();

            const int generationBase = writerIndex * 1000;
            for (int offset = 1; offset <= generationsPerWriter; ++offset) {
                if (!writerStore.save(makeCheckpoint(generationBase + offset)))
                    snapshotsWereValid.store(false, std::memory_order_relaxed);
            }
            finishedWriters.fetch_add(1, std::memory_order_release);
        });
    }

    while (readyWriters.load(std::memory_order_acquire) != writerCount)
        std::this_thread::yield();
    begin.store(true, std::memory_order_release);

    reader.join();
    for (auto& writer : writers) writer.join();

    EXPECT_TRUE(snapshotsWereValid.load(std::memory_order_relaxed));
    EXPECT_GT(observedSnapshots.load(std::memory_order_relaxed), 0);
    const auto finalSnapshot = seedStore.load(jobId);
    ASSERT_TRUE(finalSnapshot.has_value());
    EXPECT_EQ(finalSnapshot->totalFrames,
              finalSnapshot->completedUpToFrame + 100);
    EXPECT_EQ(finalSnapshot->failures.totalRetries,
              finalSnapshot->completedUpToFrame);
    ASSERT_EQ(finalSnapshot->failures.failedFrames.size(),
              static_cast<std::size_t>(failureRecordsPerGeneration));
}

TEST(CheckpointStoreContractTest, PersistsFailureManifestAcrossInstancesAndMaintainsIndex)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointInfo checkpoint;
    checkpoint.jobId = QStringLiteral("job-alpha");
    checkpoint.completedUpToFrame = 47;
    checkpoint.totalFrames = 120;
    checkpoint.schemaVersion = 3;
    checkpoint.createdAt = QDateTime::fromString(
        QStringLiteral("2026-11-07T09:15:30.123+09:00"), Qt::ISODate);
    checkpoint.updatedAt = QDateTime::fromString(
        QStringLiteral("2026-11-07T11:45:12.456+09:00"), Qt::ISODate);
    ASSERT_TRUE(checkpoint.createdAt.isValid());
    ASSERT_TRUE(checkpoint.updatedAt.isValid());
    checkpoint.failures.totalRetries = 5;
    checkpoint.failures.addFailure(12, 3, QStringLiteral("decoder timeout"));
    checkpoint.failures.setHeld(12);
    checkpoint.failures.addFailure(44, 2, QStringLiteral("GPU allocation failed"));

    {
        CheckpointStore writer;
        writer.setBasePath(directory.path());
        ASSERT_TRUE(writer.save(checkpoint));
        EXPECT_TRUE(writer.checkpointExists(checkpoint.jobId));

        auto secondJob = checkpoint;
        secondJob.jobId = QStringLiteral("job-beta");
        ASSERT_TRUE(writer.save(secondJob));
        EXPECT_EQ(writer.listCheckpoints(),
                  (QStringList{QStringLiteral("job-alpha"),
                               QStringLiteral("job-beta")}));
    }

    CheckpointStore reader;
    reader.setBasePath(directory.path());
    auto loaded = reader.load(QStringLiteral("job-alpha"));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->jobId, checkpoint.jobId);
    EXPECT_EQ(loaded->completedUpToFrame, checkpoint.completedUpToFrame);
    EXPECT_EQ(loaded->totalFrames, checkpoint.totalFrames);
    EXPECT_EQ(loaded->schemaVersion, checkpoint.schemaVersion);
    EXPECT_EQ(loaded->createdAt.toMSecsSinceEpoch(),
              checkpoint.createdAt.toMSecsSinceEpoch());
    EXPECT_EQ(loaded->updatedAt.toMSecsSinceEpoch(),
              checkpoint.updatedAt.toMSecsSinceEpoch());
    EXPECT_EQ(loaded->failures.totalRetries, 5);
    ASSERT_EQ(loaded->failures.failedFrames.size(), 2u);
    EXPECT_EQ(loaded->failures.failedFrames[0].frame, 12);
    EXPECT_EQ(loaded->failures.failedFrames[0].attempt, 3);
    EXPECT_EQ(loaded->failures.failedFrames[0].errorMessage,
              QStringLiteral("decoder timeout"));
    EXPECT_TRUE(loaded->failures.failedFrames[0].held);
    EXPECT_EQ(loaded->failures.failedFrames[1].frame, 44);
    EXPECT_EQ(loaded->failures.failedFrames[1].attempt, 2);
    EXPECT_EQ(loaded->failures.failedFrames[1].errorMessage,
              QStringLiteral("GPU allocation failed"));
    EXPECT_FALSE(loaded->failures.failedFrames[1].held);
    EXPECT_EQ(loaded->failures.heldCount(), 1);

    ASSERT_TRUE(reader.remove(QStringLiteral("job-alpha")));
    EXPECT_FALSE(reader.checkpointExists(QStringLiteral("job-alpha")));
    EXPECT_FALSE(reader.load(QStringLiteral("job-alpha")).has_value());
    EXPECT_EQ(reader.listCheckpoints(), QStringList{QStringLiteral("job-beta")});
}

TEST(CheckpointStoreContractTest,
     ConcurrentSaveAndRemoveLeaveOneLinearizableFinalState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    const QString jobId = QStringLiteral("save-remove-race-job");

    for (int generation = 1; generation <= 64; ++generation) {
        CheckpointInfo baseline;
        baseline.jobId = jobId;
        baseline.completedUpToFrame = generation;
        baseline.totalFrames = generation + 100;
        ASSERT_TRUE(store.save(baseline));

        CheckpointInfo replacement;
        replacement.jobId = jobId;
        replacement.completedUpToFrame = generation + 1000;
        replacement.totalFrames = generation + 1100;
        std::atomic<int> ready{0};
        std::atomic<bool> start{false};
        bool saveSucceeded = false;
        bool removeSucceeded = false;

        std::thread saver([&] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            saveSucceeded = store.save(replacement);
        });
        std::thread remover([&] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            removeSucceeded = store.remove(jobId);
        });

        while (ready.load(std::memory_order_acquire) != 2) {
            std::this_thread::yield();
        }
        start.store(true, std::memory_order_release);
        saver.join();
        remover.join();

        EXPECT_TRUE(saveSucceeded);
        EXPECT_TRUE(removeSucceeded);
        const bool exists = store.checkpointExists(jobId);
        const auto loaded = store.load(jobId);
        EXPECT_EQ(loaded.has_value(), exists);
        EXPECT_EQ(store.listCheckpoints().contains(jobId), exists);
        if (loaded) {
            EXPECT_EQ(loaded->completedUpToFrame, generation + 1000);
            EXPECT_EQ(loaded->totalFrames, generation + 1100);
        }
    }
}

TEST(CheckpointStoreContractTest, RejectsPathLikeJobIdsBeforeCreatingCheckpointPaths)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    CheckpointInfo checkpoint;
    checkpoint.jobId = QStringLiteral("../outside");

    EXPECT_FALSE(store.save(checkpoint));
    EXPECT_FALSE(store.checkpointExists(checkpoint.jobId));
    EXPECT_FALSE(store.remove(checkpoint.jobId));
    EXPECT_TRUE(CheckpointStore::checkpointFilePath(directory.path(), checkpoint.jobId)
                    .isEmpty());
    EXPECT_TRUE(store.listCheckpoints().isEmpty());
}

TEST(CheckpointStoreContractTest,
     RejectsMalformedAndMismatchedDocumentsWithoutAffectingOtherJobs)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());

    CheckpointInfo valid;
    valid.jobId = QStringLiteral("valid-job");
    valid.completedUpToFrame = 12;
    valid.totalFrames = 20;
    ASSERT_TRUE(store.save(valid));

    const auto writeDocument = [&](const QString& jobId,
                                   const QByteArray& payload) {
        const QString path = CheckpointStore::checkpointFilePath(
            directory.path(), jobId);
        const QFileInfo fileInfo(path);
        ASSERT_TRUE(QDir().mkpath(fileInfo.path()));
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        EXPECT_EQ(file.write(payload), payload.size());
        file.close();
    };

    writeDocument(QStringLiteral("malformed-job"), QByteArrayLiteral("{"));
    writeDocument(QStringLiteral("scalar-job"), QByteArrayLiteral("[1,2,3]"));
    writeDocument(QStringLiteral("mismatched-job"),
                  QByteArrayLiteral("{\"jobId\":\"another-job\",\"completedUpToFrame\":4}"));

    EXPECT_FALSE(store.load(QStringLiteral("malformed-job")).has_value());
    EXPECT_FALSE(store.load(QStringLiteral("scalar-job")).has_value());
    EXPECT_FALSE(store.load(QStringLiteral("mismatched-job")).has_value());

    const auto loadedValid = store.load(QStringLiteral("valid-job"));
    ASSERT_TRUE(loadedValid.has_value());
    EXPECT_EQ(loadedValid->completedUpToFrame, 12);
    EXPECT_EQ(loadedValid->totalFrames, 20);
}

TEST(CheckpointStoreContractTest, RejectsCheckpointWithWrongTypedProgressFields)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    const QString jobId = QStringLiteral("wrong-types-job");
    const QString path = CheckpointStore::checkpointFilePath(directory.path(), jobId);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).path()));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = QByteArrayLiteral(
        "{\"jobId\":\"wrong-types-job\",\"completedUpToFrame\":\"12\","
        "\"totalFrames\":20,\"schemaVersion\":1,\"failures\":[]}");
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    EXPECT_FALSE(store.load(jobId).has_value());
}

TEST(CheckpointStoreContractTest,
     RejectsFractionalFrameProgressInsteadOfLoadingFallbackInteger)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    const QString jobId = QStringLiteral("fractional-progress-job");
    const QString path = CheckpointStore::checkpointFilePath(directory.path(), jobId);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).path()));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = QByteArrayLiteral(
        "{\"jobId\":\"fractional-progress-job\",\"completedUpToFrame\":12.5,"
        "\"totalFrames\":20,\"schemaVersion\":1,\"failures\":[]}");
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    EXPECT_FALSE(store.load(jobId).has_value());
}

TEST(CheckpointStoreContractTest,
     RejectsMalformedFailureRecordInsteadOfSilentlyDroppingFields)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    const QString jobId = QStringLiteral("malformed-failure-record-job");
    const QString path = CheckpointStore::checkpointFilePath(directory.path(), jobId);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).path()));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = QByteArrayLiteral(
        "{\"jobId\":\"malformed-failure-record-job\","
        "\"completedUpToFrame\":12,\"totalFrames\":20,"
        "\"schemaVersion\":1,\"failures\":["
        "{\"frame\":4,\"attempt\":2,\"error\":\"retryable\",\"held\":false},"
        "{\"frame\":\"not-a-frame\",\"attempt\":1,"
        "\"error\":\"corrupt\",\"held\":false}]}");
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    EXPECT_FALSE(store.load(jobId).has_value());
}

TEST(CheckpointStoreContractTest,
     RejectsWrongTypedFailureCollectionInsteadOfTreatingItAsEmpty)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    const QString jobId = QStringLiteral("wrong-typed-failures-job");
    const QString path = CheckpointStore::checkpointFilePath(directory.path(), jobId);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).path()));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = QByteArrayLiteral(
        "{\"jobId\":\"wrong-typed-failures-job\","
        "\"completedUpToFrame\":12,\"totalFrames\":20,"
        "\"schemaVersion\":1,\"failures\":{\"frame\":4}}");
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    EXPECT_FALSE(store.load(jobId).has_value());
}

TEST(CheckpointStoreContractTest,
     ValidSaveRepairsMalformedCheckpointForTheSameJob)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    CheckpointStore store;
    store.setBasePath(directory.path());
    const QString jobId = QStringLiteral("repair-malformed-checkpoint-job");
    const QString path = CheckpointStore::checkpointFilePath(directory.path(), jobId);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(path).path()));
    {
        QFile corruptFile(path);
        ASSERT_TRUE(corruptFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray corruptPayload = QByteArrayLiteral("{ truncated");
        ASSERT_EQ(corruptFile.write(corruptPayload), corruptPayload.size());
    }

    EXPECT_FALSE(store.load(jobId).has_value());
    EXPECT_TRUE(store.checkpointExists(jobId));

    CheckpointInfo repaired;
    repaired.jobId = jobId;
    repaired.completedUpToFrame = 37;
    repaired.totalFrames = 52;
    repaired.failures.totalRetries = 4;
    repaired.failures.addFailure(41, 2, QStringLiteral("recovered failure"));
    ASSERT_TRUE(store.save(repaired));

    const auto loaded = store.load(jobId);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->completedUpToFrame, 37);
    EXPECT_EQ(loaded->totalFrames, 52);
    EXPECT_EQ(loaded->failures.totalRetries, 4);
    ASSERT_EQ(loaded->failures.failedFrames.size(), 1u);
    EXPECT_EQ(loaded->failures.failedFrames.front().frame, 41);
    EXPECT_EQ(loaded->failures.failedFrames.front().attempt, 2);
    EXPECT_EQ(loaded->failures.failedFrames.front().errorMessage,
              QStringLiteral("recovered failure"));
}
