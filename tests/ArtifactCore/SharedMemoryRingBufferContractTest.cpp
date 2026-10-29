#include <gtest/gtest.h>

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <cstdint>
#include <memory>

import IPC.SharedMemoryRingBuffer;

using ArtifactCore::IPC::SharedMemoryRingBuffer;

namespace {
QString uniqueBufferName()
{
    return QStringLiteral("ArtifactCoreRingTest_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

SharedMemoryRingBuffer::Config configFor(const QString& name, std::size_t size = 1024,
                                         std::size_t maxEntrySize = 128)
{
    SharedMemoryRingBuffer::Config config;
    config.name = name;
    config.totalSize = size;
    config.maxEntrySize = maxEntrySize;
    return config;
}
}

TEST(SharedMemoryRingBufferContractTest, RejectsInvalidCreationConfigurations)
{
    const auto headerAndEntry = sizeof(ArtifactCore::IPC::RingBufferHeader) +
                                sizeof(ArtifactCore::IPC::RingBufferEntryHeader);
    EXPECT_EQ(SharedMemoryRingBuffer::create(configFor(QString())), nullptr);
    EXPECT_EQ(SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), headerAndEntry)), nullptr);
    EXPECT_EQ(SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 256, 1024)), nullptr);
}

TEST(SharedMemoryRingBufferContractTest, WriteAndReadPreservePayloadSequenceFlagsAndStats)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName()));
    ASSERT_NE(ring, nullptr);
    ASSERT_TRUE(ring->isOpen());
    EXPECT_TRUE(ring->isEmpty());

    const QByteArray payload("frame\0data", 10);
    const auto write = ring->write(payload, 0xA5u);
    ASSERT_TRUE(write.success) << write.error.toStdString();
    EXPECT_EQ(write.sequenceNumber, 0u);
    EXPECT_EQ(ring->availableForRead(), sizeof(ArtifactCore::IPC::RingBufferEntryHeader) + 16u);
    EXPECT_FALSE(ring->isEmpty());

    const auto read = ring->read();
    ASSERT_TRUE(read.success) << read.error.toStdString();
    EXPECT_EQ(read.sequenceNumber, write.sequenceNumber);
    EXPECT_EQ(read.flags, 0xA5u);
    EXPECT_EQ(read.data, payload);
    EXPECT_TRUE(ring->isEmpty());

    const auto stats = ring->stats();
    EXPECT_EQ(stats.totalWrites, 1u);
    EXPECT_EQ(stats.totalReads, 1u);
    EXPECT_EQ(stats.totalWriteBytes, payload.size());
    EXPECT_EQ(stats.totalReadBytes, payload.size());
    EXPECT_EQ(stats.droppedWrites, 0u);
    EXPECT_EQ(stats.lastWriteTimestampNs, read.timestampNs);
}

TEST(SharedMemoryRingBufferContractTest, RejectsInvalidPayloadsAndOversizedEntries)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 16));
    ASSERT_NE(ring, nullptr);

    const auto nullPayload = ring->write(static_cast<const std::uint8_t*>(nullptr), 1);
    EXPECT_FALSE(nullPayload.success);
    const std::uint8_t byte = 1;
    EXPECT_FALSE(ring->write(&byte, 0).success);
    EXPECT_FALSE(ring->write(&byte, 17).success);
    EXPECT_EQ(ring->stats().droppedWrites, 0u);
    EXPECT_TRUE(ring->isEmpty());
}

TEST(SharedMemoryRingBufferContractTest, FullBufferDropsWriteWithoutChangingQueuedEntries)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 256));
    ASSERT_NE(ring, nullptr);
    const QByteArray payload(240, 'x');
    ASSERT_TRUE(ring->write(payload).success);

    const QByteArray second(32, 'y');
    const auto dropped = ring->write(second);
    EXPECT_FALSE(dropped.success);
    EXPECT_EQ(dropped.error, QStringLiteral("Buffer full"));
    EXPECT_TRUE(ring->isFull());
    EXPECT_EQ(ring->stats().droppedWrites, 1u);

    const auto read = ring->read();
    ASSERT_TRUE(read.success);
    EXPECT_EQ(read.data, payload);
    EXPECT_EQ(ring->availableForWrite(), ring->capacity());
}

TEST(SharedMemoryRingBufferContractTest, WrapsEntriesAcrossPhysicalEndOfRing)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 64));
    ASSERT_NE(ring, nullptr);
    const QByteArray payload(16, 'a');

    for (int index = 0; index < 5; ++index) {
        const auto write = ring->write(payload, static_cast<std::uint32_t>(index));
        ASSERT_TRUE(write.success) << write.error.toStdString();
    }
    for (int index = 0; index < 4; ++index) {
        const auto read = ring->read();
        ASSERT_TRUE(read.success) << read.error.toStdString();
        EXPECT_EQ(read.sequenceNumber, static_cast<std::uint64_t>(index) * 40u);
        EXPECT_EQ(read.flags, static_cast<std::uint32_t>(index));
    }

    const auto wrapped = ring->write(payload, 99u);
    ASSERT_TRUE(wrapped.success) << wrapped.error.toStdString();
    const auto wrappedAgain = ring->write(payload, 100u);
    ASSERT_TRUE(wrappedAgain.success) << wrappedAgain.error.toStdString();
    const auto remaining = ring->read();
    ASSERT_TRUE(remaining.success) << remaining.error.toStdString();
    EXPECT_EQ(remaining.flags, 4u);
    const auto afterWrap = ring->read();
    ASSERT_TRUE(afterWrap.success) << afterWrap.error.toStdString();
    EXPECT_EQ(afterWrap.flags, 99u);
    const auto afterSecondWrap = ring->read();
    ASSERT_TRUE(afterSecondWrap.success) << afterSecondWrap.error.toStdString();
    EXPECT_EQ(afterSecondWrap.flags, 100u);
    EXPECT_TRUE(ring->isEmpty());
}

TEST(SharedMemoryRingBufferContractTest, ResetDiscardsPendingEntriesAndCloseDisablesAccess)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName()));
    ASSERT_NE(ring, nullptr);
    ASSERT_TRUE(ring->write(QByteArray("pending")).success);
    ring->reset();
    EXPECT_TRUE(ring->isEmpty());
    EXPECT_FALSE(ring->read().success);

    ring->close();
    EXPECT_FALSE(ring->isOpen());
    EXPECT_EQ(ring->capacity(), 0u);
    EXPECT_FALSE(ring->write(QByteArray("closed")).success);
    EXPECT_FALSE(ring->read().success);
}
