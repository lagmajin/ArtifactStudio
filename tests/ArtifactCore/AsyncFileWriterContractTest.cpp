#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QVector>

import asio_async_file_writer;

using namespace ArtifactCore;

namespace {
std::future<WriteResult> enqueueWrite(AsioAsyncFileWriterManager& writer,
                                     const QString& path,
                                     const QByteArray& bytes)
{
    auto promise = std::make_shared<std::promise<WriteResult>>();
    auto result = promise->get_future();
    writer.writeFileAsync(path, bytes,
        [promise](const WriteResult& value) { promise->set_value(value); });
    return result;
}

void expectFileBytes(const QString& path, const QByteArray& expected)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), expected);
}
}

TEST(AsyncFileWriterContractTest, CreatesParentDirectoriesAndCommitsByteArray)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("nested/output.bin"));
    const QByteArray payload("async\0payload", 13);
    AsioAsyncFileWriterManager writer;
    auto result = enqueueWrite(writer, path, payload);

    ASSERT_EQ(result.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    const WriteResult outcome = result.get();
    ASSERT_TRUE(outcome.success) << outcome.errorMessage.toStdString();
    EXPECT_EQ(outcome.filePath, path);
    EXPECT_EQ(outcome.bytesWritten, static_cast<std::size_t>(payload.size()));
    expectFileBytes(path, payload);
}

TEST(AsyncFileWriterContractTest, OverwriteReplacesLongerExistingFileWithoutTrailingBytes)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("existing.bin"));
    const QByteArray original("old contents that are longer");
    const QByteArray replacement("new");
    QFile existing(path);
    ASSERT_TRUE(existing.open(QIODevice::WriteOnly));
    ASSERT_EQ(existing.write(original), original.size());
    existing.close();

    AsioAsyncFileWriterManager writer;
    auto result = enqueueWrite(writer, path, replacement);

    ASSERT_EQ(result.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    const WriteResult outcome = result.get();
    ASSERT_TRUE(outcome.success) << outcome.errorMessage.toStdString();
    EXPECT_EQ(outcome.bytesWritten, static_cast<std::size_t>(replacement.size()));
    expectFileBytes(path, replacement);
}

TEST(AsyncFileWriterContractTest, EmptyPayloadCommitsAsAnEmptyFile)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("empty-output.bin"));
    QFile existing(path);
    ASSERT_TRUE(existing.open(QIODevice::WriteOnly));
    ASSERT_EQ(existing.write(QByteArrayLiteral("old data")), 8);
    existing.close();

    AsioAsyncFileWriterManager writer;
    auto result = enqueueWrite(writer, path, QByteArray{});

    ASSERT_EQ(result.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    const WriteResult outcome = result.get();
    ASSERT_TRUE(outcome.success) << outcome.errorMessage.toStdString();
    EXPECT_EQ(outcome.bytesWritten, 0u);
    expectFileBytes(path, QByteArray{});
}

TEST(AsyncFileWriterContractTest, QVectorAndStdVectorOverloadsWriteExactBytes)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QByteArray expected("xyz");
    const QString qvectorPath = tempDir.filePath(QStringLiteral("qvector.bin"));
    const QString vectorPath = tempDir.filePath(QStringLiteral("vector.bin"));
    const QVector<unsigned char> qvector{'x', 'y', 'z'};
    const std::vector<unsigned char> vector{'x', 'y', 'z'};
    AsioAsyncFileWriterManager writer;
    auto qvectorPromise = std::make_shared<std::promise<WriteResult>>();
    auto vectorPromise = std::make_shared<std::promise<WriteResult>>();
    auto qvectorResult = qvectorPromise->get_future();
    auto vectorResult = vectorPromise->get_future();

    writer.writeFileAsync(qvectorPath, qvector,
        [qvectorPromise](const WriteResult& value) { qvectorPromise->set_value(value); });
    writer.writeFileAsync(vectorPath, vector,
        [vectorPromise](const WriteResult& value) { vectorPromise->set_value(value); });

    ASSERT_EQ(qvectorResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    ASSERT_EQ(vectorResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult qvectorOutcome = qvectorResult.get();
    const WriteResult vectorOutcome = vectorResult.get();
    ASSERT_TRUE(qvectorOutcome.success) << qvectorOutcome.errorMessage.toStdString();
    ASSERT_TRUE(vectorOutcome.success) << vectorOutcome.errorMessage.toStdString();
    EXPECT_EQ(qvectorOutcome.bytesWritten, 3u);
    EXPECT_EQ(vectorOutcome.bytesWritten, 3u);
    expectFileBytes(qvectorPath, expected);
    expectFileBytes(vectorPath, expected);
}

TEST(AsyncFileWriterContractTest, ConcurrentWritesToDistinctTargetsRemainIsolated)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    ASSERT_TRUE(QDir().mkpath(tempDir.filePath(QStringLiteral("parallel"))));
    constexpr int writeCount = 12;
    AsioAsyncFileWriterManager writer;
    std::vector<QString> paths;
    std::vector<QByteArray> payloads;
    std::vector<std::future<WriteResult>> results;
    paths.reserve(writeCount);
    payloads.reserve(writeCount);
    results.reserve(writeCount);

    for (int writeIndex = 0; writeIndex < writeCount; ++writeIndex) {
        paths.push_back(tempDir.filePath(
            QStringLiteral("parallel/output-%1.bin").arg(writeIndex)));
        QByteArray payload(64 * 1024 + writeIndex * 997, '\0');
        for (qsizetype byteIndex = 0; byteIndex < payload.size(); ++byteIndex) {
            payload[byteIndex] = static_cast<char>(
                (writeIndex * 31 + byteIndex * 17 + byteIndex / 251) & 0xff);
        }
        payloads.push_back(std::move(payload));
        results.push_back(enqueueWrite(writer, paths.back(), payloads.back()));
    }

    for (int writeIndex = 0; writeIndex < writeCount; ++writeIndex) {
        ASSERT_EQ(results[writeIndex].wait_for(std::chrono::seconds(10)),
                  std::future_status::ready)
            << "write=" << writeIndex;
        const WriteResult outcome = results[writeIndex].get();
        ASSERT_TRUE(outcome.success) << outcome.errorMessage.toStdString();
        EXPECT_EQ(outcome.filePath, paths[writeIndex]);
        EXPECT_EQ(outcome.bytesWritten,
                  static_cast<std::size_t>(payloads[writeIndex].size()));
        expectFileBytes(paths[writeIndex], payloads[writeIndex]);
    }
}

TEST(AsyncFileWriterContractTest, ConcurrentWritesToSameTargetCommitOneWholePayload)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("shared-target.bin"));
    QByteArray firstPayload(2 * 1024 * 1024, 'a');
    QByteArray secondPayload(2 * 1024 * 1024, 'b');
    firstPayload[0] = 'A';
    firstPayload[firstPayload.size() - 1] = 'Z';
    secondPayload[0] = 'B';
    secondPayload[secondPayload.size() - 1] = 'Y';

    AsioAsyncFileWriterManager writer;
    auto firstResult = enqueueWrite(writer, path, firstPayload);
    auto secondResult = enqueueWrite(writer, path, secondPayload);

    ASSERT_EQ(firstResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    ASSERT_EQ(secondResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult firstOutcome = firstResult.get();
    const WriteResult secondOutcome = secondResult.get();
    ASSERT_TRUE(firstOutcome.success) << firstOutcome.errorMessage.toStdString();
    ASSERT_TRUE(secondOutcome.success) << secondOutcome.errorMessage.toStdString();
    EXPECT_EQ(firstOutcome.bytesWritten,
              static_cast<std::size_t>(firstPayload.size()));
    EXPECT_EQ(secondOutcome.bytesWritten,
              static_cast<std::size_t>(secondPayload.size()));

    QFile finalFile(path);
    ASSERT_TRUE(finalFile.open(QIODevice::ReadOnly));
    const QByteArray committedPayload = finalFile.readAll();
    EXPECT_TRUE(committedPayload == firstPayload || committedPayload == secondPayload)
        << "same-target writes may finish in either order, but the committed file "
           "must match one complete input snapshot";
}

TEST(AsyncFileWriterContractTest, EmptyVectorOverloadsCommitEmptyFiles)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString qvectorPath = tempDir.filePath(QStringLiteral("empty-qvector.bin"));
    const QString vectorPath = tempDir.filePath(QStringLiteral("empty-vector.bin"));
    const QVector<unsigned char> qvector;
    const std::vector<unsigned char> vector;
    AsioAsyncFileWriterManager writer;
    std::promise<WriteResult> qvectorPromise;
    std::promise<WriteResult> vectorPromise;
    auto qvectorResult = qvectorPromise.get_future();
    auto vectorResult = vectorPromise.get_future();

    writer.writeFileAsync(qvectorPath, qvector,
        [&qvectorPromise](const WriteResult& value) { qvectorPromise.set_value(value); });
    writer.writeFileAsync(vectorPath, vector,
        [&vectorPromise](const WriteResult& value) { vectorPromise.set_value(value); });

    ASSERT_EQ(qvectorResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    ASSERT_EQ(vectorResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult qvectorOutcome = qvectorResult.get();
    const WriteResult vectorOutcome = vectorResult.get();
    ASSERT_TRUE(qvectorOutcome.success) << qvectorOutcome.errorMessage.toStdString();
    ASSERT_TRUE(vectorOutcome.success) << vectorOutcome.errorMessage.toStdString();
    EXPECT_EQ(qvectorOutcome.bytesWritten, 0u);
    EXPECT_EQ(vectorOutcome.bytesWritten, 0u);
    expectFileBytes(qvectorPath, QByteArray{});
    expectFileBytes(vectorPath, QByteArray{});
}

TEST(AsyncFileWriterContractTest, InputBuffersAreSnapshottedBeforeAsyncWrite)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString byteArrayPath = tempDir.filePath(QStringLiteral("byte-array.bin"));
    const QString qvectorPath = tempDir.filePath(QStringLiteral("qvector-snapshot.bin"));
    const QString vectorPath = tempDir.filePath(QStringLiteral("vector-snapshot.bin"));
    const QByteArray expected("snapshot");
    QByteArray byteArray = expected;
    QVector<unsigned char> qvector;
    std::vector<unsigned char> vector;
    for (const char byte : expected) {
        qvector.push_back(static_cast<unsigned char>(byte));
        vector.push_back(static_cast<unsigned char>(byte));
    }

    AsioAsyncFileWriterManager writer;
    std::promise<WriteResult> byteArrayPromise;
    std::promise<WriteResult> qvectorPromise;
    std::promise<WriteResult> vectorPromise;
    auto byteArrayResult = byteArrayPromise.get_future();
    auto qvectorResult = qvectorPromise.get_future();
    auto vectorResult = vectorPromise.get_future();

    writer.writeFileAsync(byteArrayPath, byteArray,
        [&byteArrayPromise](const WriteResult& value) {
            byteArrayPromise.set_value(value);
        });
    writer.writeFileAsync(qvectorPath, qvector,
        [&qvectorPromise](const WriteResult& value) {
            qvectorPromise.set_value(value);
        });
    writer.writeFileAsync(vectorPath, vector,
        [&vectorPromise](const WriteResult& value) {
            vectorPromise.set_value(value);
        });

    byteArray.fill('x');
    qvector.fill(static_cast<unsigned char>('x'));
    std::fill(vector.begin(), vector.end(), static_cast<unsigned char>('x'));

    ASSERT_EQ(byteArrayResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    ASSERT_EQ(qvectorResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    ASSERT_EQ(vectorResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    EXPECT_TRUE(byteArrayResult.get().success);
    EXPECT_TRUE(qvectorResult.get().success);
    EXPECT_TRUE(vectorResult.get().success);
    expectFileBytes(byteArrayPath, expected);
    expectFileBytes(qvectorPath, expected);
    expectFileBytes(vectorPath, expected);
}

TEST(AsyncFileWriterContractTest, EmptyPathCompletesWithFailureWithoutCreatingFile)
{
    AsioAsyncFileWriterManager writer;
    auto promise = std::make_shared<std::promise<WriteResult>>();
    auto result = promise->get_future();
    writer.writeFileAsync(QString(), QByteArray("data"),
        [promise](const WriteResult& value) { promise->set_value(value); });

    ASSERT_EQ(result.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    const WriteResult outcome = result.get();
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.filePath.isEmpty());
    EXPECT_EQ(outcome.bytesWritten, 0u);
    EXPECT_FALSE(outcome.errorMessage.isEmpty());
}

TEST(AsyncFileWriterContractTest, CompletionCallbackRunsExactlyOnceForSuccessAndFailure)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("callback.bin"));
    const QByteArray payload("callback payload");
    std::atomic<int> successCallbackCount{0};
    std::atomic<int> failureCallbackCount{0};
    std::promise<WriteResult> successPromise;
    std::promise<WriteResult> failurePromise;
    auto successResult = successPromise.get_future();
    auto failureResult = failurePromise.get_future();

    {
        AsioAsyncFileWriterManager writer;
        writer.writeFileAsync(path, payload,
            [&successCallbackCount, &successPromise](const WriteResult& value) {
                if (successCallbackCount.fetch_add(1) == 0) {
                    successPromise.set_value(value);
                }
            });
        writer.writeFileAsync(QString(), payload,
            [&failureCallbackCount, &failurePromise](const WriteResult& value) {
                if (failureCallbackCount.fetch_add(1) == 0) {
                    failurePromise.set_value(value);
                }
            });
    }

    ASSERT_EQ(successResult.wait_for(std::chrono::seconds(0)), std::future_status::ready);
    ASSERT_EQ(failureResult.wait_for(std::chrono::seconds(0)), std::future_status::ready);
    EXPECT_TRUE(successResult.get().success);
    EXPECT_FALSE(failureResult.get().success);
    EXPECT_EQ(successCallbackCount.load(), 1);
    EXPECT_EQ(failureCallbackCount.load(), 1);
    expectFileBytes(path, payload);
}

TEST(AsyncFileWriterContractTest,
     CompletionCallbackCanEnqueueAndAwaitAFollowupWrite)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString firstPath = tempDir.filePath(QStringLiteral("first.bin"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.bin"));
    const QByteArray firstPayload("first stage");
    const QByteArray secondPayload("callback followup");
    std::promise<WriteResult> firstPromise;
    std::promise<WriteResult> secondPromise;
    auto firstResult = firstPromise.get_future();
    auto secondResult = secondPromise.get_future();
    AsioAsyncFileWriterManager writer;

    writer.writeFileAsync(firstPath, firstPayload,
        [&writer, &firstPromise, &secondPromise, secondPath, secondPayload](
            const WriteResult& value) {
            firstPromise.set_value(value);
            writer.writeFileAsync(secondPath, secondPayload,
                [&secondPromise](const WriteResult& followup) {
                    secondPromise.set_value(followup);
                });
        });

    ASSERT_EQ(firstResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult firstOutcome = firstResult.get();
    ASSERT_TRUE(firstOutcome.success) << firstOutcome.errorMessage.toStdString();
    ASSERT_EQ(secondResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult secondOutcome = secondResult.get();
    ASSERT_TRUE(secondOutcome.success) << secondOutcome.errorMessage.toStdString();
    EXPECT_EQ(firstOutcome.filePath, firstPath);
    EXPECT_EQ(secondOutcome.filePath, secondPath);
    expectFileBytes(firstPath, firstPayload);
    expectFileBytes(secondPath, secondPayload);
}

TEST(AsyncFileWriterContractTest,
     DestructionCompletesEveryAcceptedConcurrentWriteExactlyOnce)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    constexpr int writeCount = 48;
    std::vector<QString> paths;
    std::vector<QByteArray> payloads;
    std::vector<std::shared_ptr<std::promise<WriteResult>>> promises;
    std::vector<std::future<WriteResult>> results;
    std::vector<std::shared_ptr<std::atomic<int>>> callbackCounts;
    paths.reserve(writeCount);
    payloads.reserve(writeCount);
    promises.reserve(writeCount);
    results.reserve(writeCount);
    callbackCounts.reserve(writeCount);

    for (int index = 0; index < writeCount; ++index) {
        paths.push_back(tempDir.filePath(
            QStringLiteral("shutdown/write-%1.bin").arg(index)));
        payloads.emplace_back(128 * 1024, static_cast<char>('a' + index % 26));
        promises.push_back(std::make_shared<std::promise<WriteResult>>());
        results.push_back(promises.back()->get_future());
        callbackCounts.push_back(std::make_shared<std::atomic<int>>(0));
    }

    {
        AsioAsyncFileWriterManager writer;
        for (int index = 0; index < writeCount; ++index) {
            writer.writeFileAsync(paths[index], payloads[index],
                [promise = promises[index], count = callbackCounts[index]](
                    const WriteResult& value) {
                    if (count->fetch_add(1) == 0) promise->set_value(value);
                });
        }
        // Leaving scope requests shutdown while writes can still be queued or
        // active. The manager must drain every accepted callback before return.
    }

    for (int index = 0; index < writeCount; ++index) {
        ASSERT_EQ(results[index].wait_for(std::chrono::seconds(0)),
                  std::future_status::ready)
            << "write=" << index;
        const WriteResult outcome = results[index].get();
        EXPECT_EQ(callbackCounts[index]->load(), 1) << "write=" << index;
        EXPECT_EQ(outcome.filePath, paths[index]) << "write=" << index;
        if (outcome.success) {
            EXPECT_EQ(outcome.bytesWritten,
                      static_cast<std::size_t>(payloads[index].size()))
                << "write=" << index;
            expectFileBytes(paths[index], payloads[index]);
        } else {
            EXPECT_EQ(outcome.bytesWritten, 0u) << "write=" << index;
            EXPECT_FALSE(outcome.errorMessage.isEmpty()) << "write=" << index;
        }
    }
}

TEST(AsyncFileWriterContractTest,
     ConcurrentSameTargetWritesAreFullyReportedWhenManagerIsDestroyed)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    constexpr int producerCount = 4;
    constexpr int writesPerProducer = 12;
    constexpr int writeCount = producerCount * writesPerProducer;
    const QString path = tempDir.filePath(QStringLiteral("shutdown/shared.bin"));

    std::vector<QByteArray> payloads;
    std::vector<std::shared_ptr<std::promise<WriteResult>>> promises;
    std::vector<std::future<WriteResult>> results;
    std::vector<std::shared_ptr<std::atomic<int>>> callbackCounts;
    payloads.reserve(writeCount);
    promises.reserve(writeCount);
    results.reserve(writeCount);
    callbackCounts.reserve(writeCount);
    for (int index = 0; index < writeCount; ++index) {
        payloads.emplace_back(64 * 1024, static_cast<char>('a' + index % 26));
        promises.push_back(std::make_shared<std::promise<WriteResult>>());
        results.push_back(promises.back()->get_future());
        callbackCounts.push_back(std::make_shared<std::atomic<int>>(0));
    }

    std::atomic<int> readyProducers{0};
    std::atomic<bool> begin{false};
    std::vector<QByteArray> successfulPayloads;
    successfulPayloads.reserve(writeCount);
    {
        AsioAsyncFileWriterManager writer;
        std::thread producers[producerCount];
        for (int producerIndex = 0; producerIndex < producerCount;
             ++producerIndex) {
            producers[producerIndex] = std::thread([&, producerIndex] {
                readyProducers.fetch_add(1, std::memory_order_release);
                while (!begin.load(std::memory_order_acquire))
                    std::this_thread::yield();

                for (int offset = 0; offset < writesPerProducer; ++offset) {
                    const int index = producerIndex * writesPerProducer + offset;
                    writer.writeFileAsync(
                        path, payloads[index],
                        [promise = promises[index], count = callbackCounts[index]](
                            const WriteResult& value) {
                            if (count->fetch_add(1, std::memory_order_relaxed) == 0)
                                promise->set_value(value);
                        });
                }
            });
        }

        while (readyProducers.load(std::memory_order_acquire) != producerCount)
            std::this_thread::yield();
        begin.store(true, std::memory_order_release);
        for (auto& producer : producers) producer.join();
        // Manager shutdown starts with every same-target write accepted.
    }

    for (int index = 0; index < writeCount; ++index) {
        ASSERT_EQ(results[index].wait_for(std::chrono::seconds(0)),
                  std::future_status::ready)
            << "write=" << index;
        const WriteResult outcome = results[index].get();
        EXPECT_EQ(callbackCounts[index]->load(std::memory_order_relaxed), 1)
            << "write=" << index;
        EXPECT_EQ(outcome.filePath, path) << "write=" << index;
        if (outcome.success) {
            EXPECT_EQ(outcome.bytesWritten,
                      static_cast<std::size_t>(payloads[index].size()))
                << "write=" << index;
            successfulPayloads.push_back(payloads[index]);
        } else {
            EXPECT_EQ(outcome.bytesWritten, 0u) << "write=" << index;
            EXPECT_FALSE(outcome.errorMessage.isEmpty()) << "write=" << index;
        }
    }

    QFile finalFile(path);
    if (successfulPayloads.empty()) {
        EXPECT_FALSE(QFile::exists(path));
    } else {
        ASSERT_TRUE(finalFile.open(QIODevice::ReadOnly));
        const QByteArray committedPayload = finalFile.readAll();
        EXPECT_TRUE(std::find(successfulPayloads.begin(),
                              successfulPayloads.end(), committedPayload) !=
                    successfulPayloads.end());
    }
}

TEST(AsyncFileWriterContractTest, WritingToDirectoryCompletesWithFailure)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    AsioAsyncFileWriterManager writer;
    auto result = enqueueWrite(writer, tempDir.path(), QByteArray("data"));

    ASSERT_EQ(result.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    const WriteResult outcome = result.get();
    EXPECT_FALSE(outcome.success);
    EXPECT_EQ(outcome.filePath, tempDir.path());
    EXPECT_EQ(outcome.bytesWritten, 0u);
    EXPECT_FALSE(outcome.errorMessage.isEmpty());
}

TEST(AsyncFileWriterContractTest,
     CompletionCallbackCanEnqueueAndAwaitASecondWriteOnTheSameManager)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString firstPath = tempDir.filePath(QStringLiteral("callback/first.bin"));
    const QString secondPath = tempDir.filePath(QStringLiteral("callback/second.bin"));
    const QByteArray firstPayload = QByteArrayLiteral("first");
    const QByteArray secondPayload = QByteArrayLiteral("queued from callback");
    auto firstPromise = std::make_shared<std::promise<WriteResult>>();
    auto secondPromise = std::make_shared<std::promise<WriteResult>>();
    auto firstResult = firstPromise->get_future();
    auto secondResult = secondPromise->get_future();
    std::atomic<int> firstCallbackCount{0};
    std::atomic<int> secondCallbackCount{0};
    AsioAsyncFileWriterManager writer;

    writer.writeFileAsync(firstPath, firstPayload,
        [&writer, secondPath, secondPayload, firstPromise, secondPromise,
         &firstCallbackCount, &secondCallbackCount](const WriteResult& result) {
            firstCallbackCount.fetch_add(1, std::memory_order_relaxed);
            firstPromise->set_value(result);
            writer.writeFileAsync(secondPath, secondPayload,
                [secondPromise, &secondCallbackCount](const WriteResult& chainedResult) {
                    secondCallbackCount.fetch_add(1, std::memory_order_relaxed);
                    secondPromise->set_value(chainedResult);
                });
        });

    ASSERT_EQ(firstResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult first = firstResult.get();
    ASSERT_TRUE(first.success) << first.errorMessage.toStdString();
    EXPECT_EQ(first.bytesWritten, static_cast<std::size_t>(firstPayload.size()));

    ASSERT_EQ(secondResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult second = secondResult.get();
    ASSERT_TRUE(second.success) << second.errorMessage.toStdString();
    EXPECT_EQ(second.bytesWritten, static_cast<std::size_t>(secondPayload.size()));
    EXPECT_EQ(firstCallbackCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(secondCallbackCount.load(std::memory_order_relaxed), 1);
    expectFileBytes(firstPath, firstPayload);
    expectFileBytes(secondPath, secondPayload);
}

TEST(AsyncFileWriterContractTest, WriterRemainsUsableAfterAWriteFailure)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString successfulPath = tempDir.filePath(QStringLiteral("after-failure.bin"));
    const QByteArray payload("writer recovered after failure");
    std::atomic<int> failedCallbackCount{0};
    std::atomic<int> successfulCallbackCount{0};
    std::promise<WriteResult> failurePromise;
    std::promise<WriteResult> successPromise;
    auto failureResult = failurePromise.get_future();
    auto successResult = successPromise.get_future();
    AsioAsyncFileWriterManager writer;

    writer.writeFileAsync(tempDir.path(), QByteArrayLiteral("cannot replace a directory"),
        [&failedCallbackCount, &failurePromise](const WriteResult& value) {
            failedCallbackCount.fetch_add(1, std::memory_order_relaxed);
            failurePromise.set_value(value);
        });

    ASSERT_EQ(failureResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult failed = failureResult.get();
    ASSERT_FALSE(failed.success);
    EXPECT_EQ(failed.bytesWritten, 0u);
    EXPECT_FALSE(failed.errorMessage.isEmpty());

    writer.writeFileAsync(successfulPath, payload,
        [&successfulCallbackCount, &successPromise](const WriteResult& value) {
            successfulCallbackCount.fetch_add(1, std::memory_order_relaxed);
            successPromise.set_value(value);
        });

    ASSERT_EQ(successResult.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    const WriteResult succeeded = successResult.get();
    ASSERT_TRUE(succeeded.success) << succeeded.errorMessage.toStdString();
    EXPECT_EQ(succeeded.bytesWritten, static_cast<std::size_t>(payload.size()));
    EXPECT_EQ(failedCallbackCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(successfulCallbackCount.load(std::memory_order_relaxed), 1);
    expectFileBytes(successfulPath, payload);
}
