#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include <QByteArray>
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
