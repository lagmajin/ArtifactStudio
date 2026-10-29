#include <gtest/gtest.h>

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
