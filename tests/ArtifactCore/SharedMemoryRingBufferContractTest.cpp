#include <gtest/gtest.h>

#include <QByteArray>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QString>
#include <QUuid>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

import IPC.SharedMemoryRingBuffer;
import IPC.RenderFarmSharedBuffer;
import IPC.IPCChannel;
import Image.ImageF32x4_RGBA;

using ArtifactCore::IPC::SharedMemoryRingBuffer;
using ArtifactCore::IPC::RenderFarmSharedBuffer;
using ArtifactCore::IPC::IPCChannel;
using ArtifactCore::IPC::IPCChannelConfig;
using ArtifactCore::IPC::IPCTransport;
using ArtifactCore::ImageF32x4_RGBA;

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

TEST(SharedMemoryRingBufferContractTest,
     RejectsCorruptSharedHeadersWithoutBreakingTheOwnerHandle)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName()));
    ASSERT_NE(ring, nullptr);
    const QByteArray payload("header survives rejected attachment");
    const auto write = ring->write(payload, 0x31u);
    ASSERT_TRUE(write.success) << write.error.toStdString();

    QSharedMemory peerMemory(ring->name());
    ASSERT_TRUE(peerMemory.attach());
    ASSERT_TRUE(peerMemory.lock());
    auto* header = static_cast<ArtifactCore::IPC::RingBufferHeader*>(peerMemory.data());
    ASSERT_NE(header, nullptr);

    const auto originalMagic = header->magic;
    const auto originalVersion = header->version;
    const auto originalCapacity = header->totalCapacity;

    header->magic = 0;
    ASSERT_TRUE(peerMemory.unlock());
    EXPECT_EQ(SharedMemoryRingBuffer::open(ring->name()), nullptr)
        << "an unknown shared-memory signature must not be attached";
    ASSERT_TRUE(peerMemory.lock());
    header->magic = originalMagic;
    header->version = originalVersion + 1u;
    ASSERT_TRUE(peerMemory.unlock());
    EXPECT_EQ(SharedMemoryRingBuffer::open(ring->name()), nullptr)
        << "an unsupported shared-memory layout version must not be attached";
    ASSERT_TRUE(peerMemory.lock());
    header->version = originalVersion;
    header->totalCapacity = originalCapacity + 1u;
    ASSERT_TRUE(peerMemory.unlock());
    EXPECT_EQ(SharedMemoryRingBuffer::open(ring->name()), nullptr)
        << "capacity beyond the mapped segment must not be attached";
    ASSERT_TRUE(peerMemory.lock());
    header->totalCapacity = originalCapacity;
    ASSERT_TRUE(peerMemory.unlock());

    const auto read = ring->read();
    ASSERT_TRUE(read.success) << read.error.toStdString();
    EXPECT_EQ(read.sequenceNumber, write.sequenceNumber);
    EXPECT_EQ(read.flags, 0x31u);
    EXPECT_EQ(read.data, payload);
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

TEST(SharedMemoryRingBufferContractTest, SeparateHandlesShareEntriesAndResetState)
{
    const QString name = uniqueBufferName();
    auto producer = SharedMemoryRingBuffer::create(configFor(name));
    ASSERT_NE(producer, nullptr);
    auto consumer = SharedMemoryRingBuffer::open(name);
    ASSERT_NE(consumer, nullptr);
    ASSERT_TRUE(consumer->isOpen());

    const auto firstWrite = producer->write(QByteArray("producer-to-consumer"), 0x11u);
    ASSERT_TRUE(firstWrite.success) << firstWrite.error.toStdString();
    const auto firstRead = consumer->read();
    ASSERT_TRUE(firstRead.success) << firstRead.error.toStdString();
    EXPECT_EQ(firstRead.data, QByteArray("producer-to-consumer"));
    EXPECT_EQ(firstRead.flags, 0x11u);
    EXPECT_EQ(firstRead.sequenceNumber, firstWrite.sequenceNumber);

    const auto replyWrite = consumer->write(QByteArray("consumer-to-producer"), 0x22u);
    ASSERT_TRUE(replyWrite.success) << replyWrite.error.toStdString();
    const auto replyRead = producer->read();
    ASSERT_TRUE(replyRead.success) << replyRead.error.toStdString();
    EXPECT_EQ(replyRead.data, QByteArray("consumer-to-producer"));
    EXPECT_EQ(replyRead.flags, 0x22u);
    EXPECT_EQ(replyRead.sequenceNumber, replyWrite.sequenceNumber);

    ASSERT_TRUE(producer->write(QByteArray("discarded by peer reset")).success);
    consumer->reset();
    EXPECT_TRUE(producer->isEmpty());
    EXPECT_FALSE(consumer->read().success);

    EXPECT_EQ(producer->stats().totalWrites, 2u);
    EXPECT_EQ(producer->stats().totalReads, 1u);
    EXPECT_EQ(consumer->stats().totalWrites, 1u);
    EXPECT_EQ(consumer->stats().totalReads, 1u);
}

TEST(SharedMemoryRingBufferContractTest,
     SeparateHandlesExposeOneWriteAndReadNotificationPerCommittedEntry)
{
    const QString name = uniqueBufferName();
    auto producer = SharedMemoryRingBuffer::create(configFor(name));
    ASSERT_NE(producer, nullptr);
    auto consumer = SharedMemoryRingBuffer::open(name);
    ASSERT_NE(consumer, nullptr);
    ASSERT_NE(consumer->writeSemaphore(), nullptr);
    ASSERT_NE(producer->readSemaphore(), nullptr);

    EXPECT_FALSE(consumer->writeSemaphore()->tryAcquire());
    EXPECT_FALSE(producer->readSemaphore()->tryAcquire());

    const auto firstWrite = producer->write(QByteArray("first notification"));
    ASSERT_TRUE(firstWrite.success) << firstWrite.error.toStdString();
    EXPECT_TRUE(consumer->writeSemaphore()->tryAcquire());
    EXPECT_FALSE(consumer->writeSemaphore()->tryAcquire())
        << "one committed entry releases exactly one write-ready permit";
    const auto firstRead = consumer->read();
    ASSERT_TRUE(firstRead.success) << firstRead.error.toStdString();
    EXPECT_TRUE(producer->readSemaphore()->tryAcquire());
    EXPECT_FALSE(producer->readSemaphore()->tryAcquire())
        << "one consumed entry releases exactly one read-complete permit";

    const auto secondWrite = producer->write(QByteArray("second notification"));
    ASSERT_TRUE(secondWrite.success) << secondWrite.error.toStdString();
    EXPECT_TRUE(consumer->writeSemaphore()->tryAcquire());
    EXPECT_FALSE(consumer->writeSemaphore()->tryAcquire());
    const auto secondRead = consumer->read();
    ASSERT_TRUE(secondRead.success) << secondRead.error.toStdString();
    EXPECT_TRUE(producer->readSemaphore()->tryAcquire());
    EXPECT_FALSE(producer->readSemaphore()->tryAcquire());
}

TEST(SharedMemoryRingBufferContractTest,
     PersistentBufferReopensAfterFinalHandleClosesAndKeepsCommittedEntry)
{
    const QString name = uniqueBufferName();
    auto config = configFor(name, 512, 64);
    config.persistent = true;
    auto writer = SharedMemoryRingBuffer::create(config);
    ASSERT_NE(writer, nullptr);

    const QByteArray payload("survives final detach\0payload", 29);
    const auto write = writer->write(payload, 0xC0FFEEu);
    ASSERT_TRUE(write.success) << write.error.toStdString();
    writer->close();
    writer.reset();

    auto reopened = SharedMemoryRingBuffer::open(name);
    ASSERT_NE(reopened, nullptr)
        << "persistent storage should remain attachable after the last handle closes";
    ASSERT_TRUE(reopened->isOpen());
    const auto read = reopened->read();
    ASSERT_TRUE(read.success) << read.error.toStdString();
    EXPECT_EQ(read.sequenceNumber, write.sequenceNumber);
    EXPECT_EQ(read.flags, 0xC0FFEEu);
    EXPECT_EQ(read.data, payload);
    EXPECT_TRUE(reopened->isEmpty());
}

TEST(SharedMemoryRingBufferContractTest, CorruptPayloadFailsChecksumAndConsumesEntry)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName()));
    ASSERT_NE(ring, nullptr);
    const QByteArray payload("checksum protected payload");
    const auto write = ring->write(payload, 0x44u);
    ASSERT_TRUE(write.success) << write.error.toStdString();

    QSharedMemory peerMemory(ring->name());
    ASSERT_TRUE(peerMemory.attach());
    ASSERT_TRUE(peerMemory.lock());
    auto* sharedBytes = static_cast<std::uint8_t*>(peerMemory.data());
    ASSERT_NE(sharedBytes, nullptr);
    auto* payloadBytes = sharedBytes + sizeof(ArtifactCore::IPC::RingBufferHeader) +
                         sizeof(ArtifactCore::IPC::RingBufferEntryHeader);
    payloadBytes[0] = static_cast<std::uint8_t>(payloadBytes[0] ^ 0xFFu);
    ASSERT_TRUE(peerMemory.unlock());

    const auto read = ring->read();
    EXPECT_FALSE(read.success);
    EXPECT_EQ(read.sequenceNumber, write.sequenceNumber);
    EXPECT_EQ(read.flags, 0x44u);
    EXPECT_EQ(read.error, QStringLiteral("Checksum mismatch"));
    EXPECT_TRUE(ring->isEmpty())
        << "a corrupt entry is consumed so readers do not get stuck on it";
    EXPECT_EQ(ring->stats().totalWrites, 1u);
    EXPECT_EQ(ring->stats().totalReads, 0u);

    const QByteArray recoveryPayload("ring recovers after corrupt entry");
    const auto recoveryWrite = ring->write(recoveryPayload, 0x55u);
    ASSERT_TRUE(recoveryWrite.success) << recoveryWrite.error.toStdString();
    EXPECT_GT(recoveryWrite.sequenceNumber, write.sequenceNumber);

    const auto recoveryRead = ring->read();
    ASSERT_TRUE(recoveryRead.success) << recoveryRead.error.toStdString();
    EXPECT_EQ(recoveryRead.sequenceNumber, recoveryWrite.sequenceNumber);
    EXPECT_EQ(recoveryRead.flags, 0x55u);
    EXPECT_EQ(recoveryRead.data, recoveryPayload);
    EXPECT_TRUE(ring->isEmpty());
    EXPECT_EQ(ring->stats().totalWrites, 2u);
    EXPECT_EQ(ring->stats().totalReads, 1u);
}

TEST(SharedMemoryRingBufferContractTest,
     OversizedCorruptEntryLengthIsRejectedAndRingRecovers)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 1024, 128));
    ASSERT_NE(ring, nullptr);
    const auto write = ring->write(QByteArrayLiteral("valid entry before header corruption"),
                                   0xA7u);
    ASSERT_TRUE(write.success) << write.error.toStdString();

    QSharedMemory peerMemory(ring->name());
    ASSERT_TRUE(peerMemory.attach());
    ASSERT_TRUE(peerMemory.lock());
    auto* sharedBytes = static_cast<std::uint8_t*>(peerMemory.data());
    ASSERT_NE(sharedBytes, nullptr);
    auto* entry = reinterpret_cast<ArtifactCore::IPC::RingBufferEntryHeader*>(
        sharedBytes + sizeof(ArtifactCore::IPC::RingBufferHeader));
    entry->payloadSize = 129u;
    ASSERT_TRUE(peerMemory.unlock());

    const auto rejected = ring->read();
    EXPECT_FALSE(rejected.success);
    EXPECT_EQ(rejected.error, QStringLiteral("Corrupt ring buffer entry"));
    EXPECT_TRUE(ring->isEmpty())
        << "invalid lengths must discard the unread region instead of trapping readers";
    EXPECT_EQ(ring->stats().totalReads, 0u);

    const QByteArray recoveryPayload("valid entry after corrupt header");
    const auto recoveryWrite = ring->write(recoveryPayload, 0x5Cu);
    ASSERT_TRUE(recoveryWrite.success) << recoveryWrite.error.toStdString();
    const auto recovered = ring->read();
    ASSERT_TRUE(recovered.success) << recovered.error.toStdString();
    EXPECT_EQ(recovered.sequenceNumber, recoveryWrite.sequenceNumber);
    EXPECT_EQ(recovered.flags, 0x5Cu);
    EXPECT_EQ(recovered.data, recoveryPayload);
    EXPECT_EQ(ring->stats().totalWrites, 2u);
    EXPECT_EQ(ring->stats().totalReads, 1u);
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

TEST(SharedMemoryRingBufferContractTest,
     ReadBlockingHonorsTimeoutAndAcceptsDataBeforeDeadline)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName()));
    ASSERT_NE(ring, nullptr);

    const auto timeoutStart = std::chrono::steady_clock::now();
    const auto timedOut = ring->readBlocking(35);
    const auto timeoutElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - timeoutStart);
    EXPECT_FALSE(timedOut.success);
    EXPECT_EQ(timedOut.error, QStringLiteral("Buffer is empty"));
    EXPECT_GE(timeoutElapsed.count(), 20);
    EXPECT_LT(timeoutElapsed.count(), 1500);

    const QByteArray payload("arrived before the blocking read deadline");
    std::atomic<bool> writeSucceeded{false};
    std::thread delayedProducer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        writeSucceeded.store(ring->write(payload, 0xCAFEu).success,
                             std::memory_order_release);
    });

    const auto readStart = std::chrono::steady_clock::now();
    const auto received = ring->readBlocking(1000);
    const auto readElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - readStart);
    delayedProducer.join();

    EXPECT_TRUE(writeSucceeded.load(std::memory_order_acquire));
    ASSERT_TRUE(received.success) << received.error.toStdString();
    EXPECT_EQ(received.data, payload);
    EXPECT_EQ(received.flags, 0xCAFEu);
    EXPECT_GE(readElapsed.count(), 10);
    EXPECT_LT(readElapsed.count(), 2000);
    EXPECT_TRUE(ring->isEmpty());
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

TEST(SharedMemoryRingBufferContractTest,
     SeparateHandlesTransferConcurrentSpscStreamAcrossRepeatedWraps)
{
    constexpr std::uint32_t itemCount = 1200;
    const QString name = uniqueBufferName();
    auto producer = SharedMemoryRingBuffer::create(configFor(name, 512, 64));
    ASSERT_NE(producer, nullptr);
    auto consumer = SharedMemoryRingBuffer::open(name);
    ASSERT_NE(consumer, nullptr);

    const auto payloadFor = [](std::uint32_t index) {
        QByteArray payload = QByteArray::number(static_cast<unsigned long long>(index));
        payload.append(':');
        payload.append(24, static_cast<char>((index * 37u) & 0xffu));
        return payload;
    };
    std::atomic<bool> stopProducer{false};
    std::atomic<bool> transferFailed{false};
    std::atomic<std::uint32_t> completedWrites{0};
    std::atomic<std::uint64_t> lastWriteSequence{0};

    std::thread producerThread([&] {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(10);
        for (std::uint32_t index = 0; index < itemCount; ++index) {
            const QByteArray payload = payloadFor(index);
            const std::uint32_t flags = index ^ 0xA500u;
            while (!stopProducer.load(std::memory_order_acquire)) {
                const auto result = producer->write(payload, flags);
                if (result.success) {
                    lastWriteSequence.store(result.sequenceNumber,
                                             std::memory_order_release);
                    completedWrites.fetch_add(1, std::memory_order_release);
                    break;
                }
                if (result.error != QStringLiteral("Buffer full") ||
                    std::chrono::steady_clock::now() >= deadline) {
                    transferFailed.store(true, std::memory_order_release);
                    stopProducer.store(true, std::memory_order_release);
                    return;
                }
                std::this_thread::yield();
            }
        }
    });

    std::uint32_t consumed = 0;
    std::uint64_t previousSequence = 0;
    bool hasPreviousSequence = false;
    bool sequenceIncreasedAcrossWrapPadding = false;
    const auto consumerDeadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(10);
    while (consumed < itemCount &&
           !transferFailed.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < consumerDeadline) {
        const auto result = consumer->readBlocking(2);
        if (!result.success) {
            if (result.error != QStringLiteral("Buffer is empty")) {
                transferFailed.store(true, std::memory_order_release);
                stopProducer.store(true, std::memory_order_release);
                break;
            }
            continue;
        }

        EXPECT_EQ(result.data, payloadFor(consumed)) << "item=" << consumed;
        EXPECT_EQ(result.flags, consumed ^ 0xA500u) << "item=" << consumed;
        if (hasPreviousSequence) {
            EXPECT_GT(result.sequenceNumber, previousSequence)
                << "item=" << consumed;
            if (result.sequenceNumber > previousSequence &&
                result.sequenceNumber - previousSequence > 56u) {
                sequenceIncreasedAcrossWrapPadding = true;
            }
        }
        previousSequence = result.sequenceNumber;
        hasPreviousSequence = true;
        ++consumed;
    }

    if (consumed != itemCount) {
        stopProducer.store(true, std::memory_order_release);
    }
    producerThread.join();

    EXPECT_FALSE(transferFailed.load(std::memory_order_acquire));
    EXPECT_EQ(completedWrites.load(std::memory_order_acquire), itemCount);
    EXPECT_EQ(consumed, itemCount);
    EXPECT_TRUE(sequenceIncreasedAcrossWrapPadding)
        << "physical ring wraps advance the sequence past entry payload spacing";
    EXPECT_GT(lastWriteSequence.load(std::memory_order_acquire),
              static_cast<std::uint64_t>(itemCount - 1u) * 56u);
    EXPECT_TRUE(consumer->isEmpty());
    EXPECT_EQ(producer->stats().totalWrites, itemCount);
    EXPECT_EQ(consumer->stats().totalReads, itemCount);
}

TEST(SharedMemoryRingBufferContractTest,
     ConcurrentWritersOnOneHandleDeliverEachPerWriterSequenceExactlyOnce)
{
    constexpr int writerCount = 4;
    constexpr std::uint32_t itemsPerWriter = 300;
    constexpr std::uint32_t totalItems =
        static_cast<std::uint32_t>(writerCount) * itemsPerWriter;
    struct StreamItem {
        std::uint32_t writer;
        std::uint32_t sequence;
    };

    const QString name = uniqueBufferName();
    auto producer = SharedMemoryRingBuffer::create(configFor(name, 512, 64));
    ASSERT_NE(producer, nullptr);
    auto consumer = SharedMemoryRingBuffer::open(name);
    ASSERT_NE(consumer, nullptr);

    std::atomic<int> ready{0};
    std::atomic<int> finishedWriters{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stopWriters{false};
    std::atomic<bool> transferFailed{false};
    std::thread writers[writerCount];
    for (int writer = 0; writer < writerCount; ++writer) {
        writers[writer] = std::thread([&, writer] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(10);
            for (std::uint32_t sequence = 0;
                 sequence < itemsPerWriter &&
                 !stopWriters.load(std::memory_order_acquire); ++sequence) {
                const StreamItem item{static_cast<std::uint32_t>(writer), sequence};
                const QByteArray payload(reinterpret_cast<const char*>(&item),
                                         static_cast<qsizetype>(sizeof(item)));
                bool written = false;
                while (!stopWriters.load(std::memory_order_acquire)) {
                    const auto result = producer->write(
                        payload, static_cast<std::uint32_t>(writer));
                    if (result.success) {
                        written = true;
                        break;
                    }
                    if (result.error != QStringLiteral("Buffer full") ||
                        std::chrono::steady_clock::now() >= deadline) {
                        transferFailed.store(true, std::memory_order_release);
                        stopWriters.store(true, std::memory_order_release);
                        break;
                    }
                    std::this_thread::yield();
                }
                if (!written) break;
            }
            finishedWriters.fetch_add(1, std::memory_order_release);
        });
    }

    while (ready.load(std::memory_order_acquire) != writerCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    bool seen[writerCount][itemsPerWriter]{};
    std::uint32_t nextSequence[writerCount]{};
    std::uint32_t consumed = 0;
    const auto consumerDeadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(10);
    while ((finishedWriters.load(std::memory_order_acquire) != writerCount ||
            !consumer->isEmpty()) &&
           std::chrono::steady_clock::now() < consumerDeadline) {
        const auto result = consumer->readBlocking(2);
        if (!result.success) {
            if (result.error != QStringLiteral("Buffer is empty")) {
                transferFailed.store(true, std::memory_order_release);
                break;
            }
            continue;
        }

        if (result.data.size() != static_cast<qsizetype>(sizeof(StreamItem))) {
            transferFailed.store(true, std::memory_order_release);
            stopWriters.store(true, std::memory_order_release);
            continue;
        }
        StreamItem item{};
        std::memcpy(&item, result.data.constData(), sizeof(item));
        if (item.writer >= static_cast<std::uint32_t>(writerCount) ||
            item.sequence >= itemsPerWriter) {
            transferFailed.store(true, std::memory_order_release);
            stopWriters.store(true, std::memory_order_release);
            continue;
        }
        EXPECT_EQ(result.flags, item.writer);
        EXPECT_FALSE(seen[item.writer][item.sequence]);
        EXPECT_EQ(item.sequence, nextSequence[item.writer]++);
        seen[item.writer][item.sequence] = true;
        ++consumed;
    }

    for (auto& writer : writers) writer.join();

    EXPECT_FALSE(transferFailed.load(std::memory_order_acquire));
    EXPECT_EQ(finishedWriters.load(std::memory_order_acquire), writerCount);
    EXPECT_EQ(consumed, totalItems);
    EXPECT_TRUE(consumer->isEmpty());
    for (int writer = 0; writer < writerCount; ++writer) {
        EXPECT_EQ(nextSequence[writer], itemsPerWriter)
            << "writer=" << writer;
        for (std::uint32_t sequence = 0; sequence < itemsPerWriter; ++sequence) {
            EXPECT_TRUE(seen[writer][sequence])
                << "writer=" << writer << " sequence=" << sequence;
        }
    }
    EXPECT_EQ(producer->stats().totalWrites, totalItems);
    EXPECT_EQ(consumer->stats().totalReads, totalItems);
}

TEST(SharedMemoryRingBufferContractTest, ReadSequenceSkipsOlderEntriesAndAcceptsTheFirstGreaterSequence)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 64));
    ASSERT_NE(ring, nullptr);
    const QByteArray first(1, 'a');
    const QByteArray second(9, 'b');
    const QByteArray third(17, 'c');

    const auto firstWrite = ring->write(first, 1u);
    const auto secondWrite = ring->write(second, 2u);
    const auto thirdWrite = ring->write(third, 3u);
    ASSERT_TRUE(firstWrite.success);
    ASSERT_TRUE(secondWrite.success);
    ASSERT_TRUE(thirdWrite.success);
    EXPECT_EQ(firstWrite.sequenceNumber, 0u);
    EXPECT_EQ(secondWrite.sequenceNumber, 40u);
    EXPECT_EQ(thirdWrite.sequenceNumber, 88u);

    const auto atRequestedSequence = ring->readSequence(secondWrite.sequenceNumber);
    ASSERT_TRUE(atRequestedSequence.success) << atRequestedSequence.error.toStdString();
    EXPECT_EQ(atRequestedSequence.sequenceNumber, secondWrite.sequenceNumber);
    EXPECT_EQ(atRequestedSequence.flags, 2u);
    EXPECT_EQ(atRequestedSequence.data, second);

    const auto nextSequence = ring->readSequence(secondWrite.sequenceNumber + 47u);
    ASSERT_TRUE(nextSequence.success) << nextSequence.error.toStdString();
    EXPECT_EQ(nextSequence.sequenceNumber, thirdWrite.sequenceNumber);
    EXPECT_EQ(nextSequence.flags, 3u);
    EXPECT_EQ(nextSequence.data, third);
    EXPECT_TRUE(ring->isEmpty());
    EXPECT_EQ(ring->stats().totalReads, 3u)
        << "entries skipped while searching also advance the read counter";
}

TEST(SharedMemoryRingBufferContractTest,
     ReadSequenceSkipsOldEntriesAndWrapPaddingBeforeTarget)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 64));
    ASSERT_NE(ring, nullptr);
    const QByteArray payload(17, 'w');
    constexpr std::uint64_t entryBytes =
        (sizeof(ArtifactCore::IPC::RingBufferEntryHeader) + 17u + 7u) & ~7u;

    for (std::uint32_t index = 0; index < 5; ++index) {
        ASSERT_TRUE(ring->write(payload, index).success);
    }
    for (int index = 0; index < 4; ++index) {
        ASSERT_TRUE(ring->read().success);
    }

    std::uint64_t previousSequence = 4u * entryBytes;
    SharedMemoryRingBuffer::WriteResult wrappedTarget;
    std::uint32_t wrappedFlags = 0;
    for (std::uint32_t index = 5; index < 20; ++index) {
        const auto write = ring->write(payload, index);
        ASSERT_TRUE(write.success) << write.error.toStdString();
        if (write.sequenceNumber > previousSequence + entryBytes) {
            wrappedTarget = write;
            wrappedFlags = index;
            break;
        }
        previousSequence = write.sequenceNumber;
    }
    ASSERT_NE(wrappedTarget.sequenceNumber, 0u)
        << "the producer should have written across the physical ring end";
    EXPECT_GT(wrappedTarget.sequenceNumber, previousSequence + entryBytes)
        << "the target sequence includes the skipped wrap padding";

    const auto target = ring->readSequence(wrappedTarget.sequenceNumber);
    ASSERT_TRUE(target.success) << target.error.toStdString();
    EXPECT_EQ(target.sequenceNumber, wrappedTarget.sequenceNumber);
    EXPECT_EQ(target.flags, wrappedFlags);
    EXPECT_EQ(target.data, payload);
    EXPECT_TRUE(ring->isEmpty());
    EXPECT_EQ(ring->stats().totalWrites, 7u);
    EXPECT_EQ(ring->stats().totalReads, 7u)
        << "readSequence consumes the unread old entry and both newer entries";
}

TEST(SharedMemoryRingBufferContractTest, MixedOperationsMatchAnIndependentWrapAndCapacityModel)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 64));
    ASSERT_NE(ring, nullptr);

    struct ExpectedEntry {
        std::uint64_t sequence = 0;
        std::uint64_t endIndex = 0;
        QByteArray payload;
        std::uint32_t flags = 0;
    };
    const std::uint64_t capacity = ring->capacity();
    std::vector<ExpectedEntry> pending;
    std::uint64_t writeIndex = 0;
    std::uint64_t readIndex = 0;
    std::uint64_t successfulWrites = 0;
    std::uint64_t successfulReads = 0;
    std::uint64_t droppedWrites = 0;

    const auto consumeOne = [&]() {
        const auto result = ring->read();
        if (pending.empty()) {
            EXPECT_FALSE(result.success);
            return;
        }

        ASSERT_TRUE(result.success) << result.error.toStdString();
        const ExpectedEntry expected = pending.front();
        pending.erase(pending.begin());
        EXPECT_EQ(result.sequenceNumber, expected.sequence);
        EXPECT_EQ(result.flags, expected.flags);
        EXPECT_EQ(result.data, expected.payload);
        readIndex = expected.endIndex;
        ++successfulReads;
    };

    for (std::uint32_t operation = 0; operation < 700; ++operation) {
        if (operation % 7u < 4u) {
            const int payloadSize = 1 + static_cast<int>((operation * 17u) % 60u);
            const QByteArray payload(payloadSize,
                                     static_cast<char>('a' + operation % 26u));
            const std::uint32_t flags = operation ^ 0xA500u;
            const std::uint64_t entrySize =
                (sizeof(ArtifactCore::IPC::RingBufferEntryHeader) +
                 static_cast<std::uint64_t>(payloadSize) + 7u) & ~std::uint64_t(7u);
            const std::uint64_t offset = writeIndex % capacity;
            const std::uint64_t tail = capacity - offset;
            const std::uint64_t required = entrySize <= tail ? entrySize : tail + entrySize;
            const std::uint64_t used = writeIndex - readIndex;
            const bool expectedSuccess = required <= capacity - std::min(used, capacity);

            const auto result = ring->write(payload, flags);
            EXPECT_EQ(result.success, expectedSuccess) << "operation=" << operation;
            if (!expectedSuccess) {
                EXPECT_EQ(result.error, QStringLiteral("Buffer full"));
                ++droppedWrites;
            } else {
                std::uint64_t actualWrite = writeIndex;
                if (entrySize > tail) {
                    actualWrite += tail;
                }
                const std::uint64_t endIndex = actualWrite + entrySize;
                EXPECT_EQ(result.sequenceNumber, actualWrite)
                    << "operation=" << operation;
                pending.push_back(ExpectedEntry{
                    actualWrite, endIndex, payload, flags});
                writeIndex = endIndex;
                ++successfulWrites;
            }
        } else {
            consumeOne();
        }

        EXPECT_EQ(ring->availableForRead(), writeIndex - readIndex)
            << "operation=" << operation;
        EXPECT_EQ(ring->availableForWrite(), capacity - (writeIndex - readIndex))
            << "operation=" << operation;
    }

    while (!pending.empty()) {
        consumeOne();
    }
    EXPECT_TRUE(ring->isEmpty());
    const auto stats = ring->stats();
    EXPECT_EQ(stats.totalWrites, successfulWrites);
    EXPECT_EQ(stats.totalReads, successfulReads);
    EXPECT_EQ(stats.droppedWrites, droppedWrites);
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

TEST(SharedMemoryRingBufferContractTest,
     ResetAtPhysicalTailAllowsReuseAcrossTheNextWrap)
{
    auto ring = SharedMemoryRingBuffer::create(configFor(uniqueBufferName(), 512, 64));
    ASSERT_NE(ring, nullptr);
    const std::uint64_t capacity = ring->capacity();
    const std::uint64_t entryBytes =
        (sizeof(ArtifactCore::IPC::RingBufferEntryHeader) + 1u + 7u) & ~7u;
    ASSERT_GT(capacity, entryBytes * 2u);

    std::uint64_t nextWriteIndex = 0;
    std::uint64_t drainedEntries = 0;
    bool reachedTailWindow = false;
    const QByteArray advancingPayload(1, 'a');
    for (std::uint64_t attempt = 0; attempt < capacity; ++attempt) {
        const std::uint64_t offset = nextWriteIndex % capacity;
        if (offset + entryBytes <= capacity &&
            offset + entryBytes * 2u > capacity) {
            reachedTailWindow = true;
            break;
        }

        const auto write = ring->write(advancingPayload);
        ASSERT_TRUE(write.success) << write.error.toStdString();
        const auto read = ring->read();
        ASSERT_TRUE(read.success) << read.error.toStdString();
        EXPECT_EQ(read.sequenceNumber, write.sequenceNumber);
        nextWriteIndex = write.sequenceNumber + entryBytes;
        ++drainedEntries;
    }
    ASSERT_TRUE(reachedTailWindow);
    EXPECT_TRUE(nextWriteIndex % capacity + entryBytes * 2u > capacity);

    const QByteArray discardedPayload("discard-on-reset");
    const auto pending = ring->write(discardedPayload, 0x31u);
    ASSERT_TRUE(pending.success) << pending.error.toStdString();
    ring->reset();
    EXPECT_TRUE(ring->isEmpty());

    const QByteArray reusedPayload("after-reset-wrap");
    const auto reused = ring->write(reusedPayload, 0x72u);
    ASSERT_TRUE(reused.success) << reused.error.toStdString();
    EXPECT_GT(reused.sequenceNumber, pending.sequenceNumber);
    const auto readAfterReset = ring->read();
    ASSERT_TRUE(readAfterReset.success) << readAfterReset.error.toStdString();
    EXPECT_EQ(readAfterReset.sequenceNumber, reused.sequenceNumber);
    EXPECT_EQ(readAfterReset.flags, 0x72u);
    EXPECT_EQ(readAfterReset.data, reusedPayload);
    EXPECT_TRUE(ring->isEmpty());

    const auto stats = ring->stats();
    EXPECT_EQ(stats.totalWrites, drainedEntries + 2u);
    EXPECT_EQ(stats.totalReads, drainedEntries + 1u)
        << "reset discards queued data without counting it as a read";
}

TEST(RenderFarmSharedBufferContractTest,
     FrameSequenceSkipPreservesExactFloatPixelsAndFrameIdentity)
{
    RenderFarmSharedBuffer::Config config;
    config.bufferName = uniqueBufferName();
    config.totalSizeMB = 1;
    config.maxFrameWidth = 4;
    config.maxFrameHeight = 4;
    auto consumer = RenderFarmSharedBuffer::createConsumer(config);
    ASSERT_NE(consumer, nullptr);
    auto producer = RenderFarmSharedBuffer::openProducer(config.bufferName);
    ASSERT_NE(producer, nullptr);

    constexpr float firstPixels[]{
        0.125f, 0.25f, 0.5f, 1.0f,
        1.25f, 1.5f, 1.75f, 0.75f,
        -0.25f, 2.0f, 0.0f, 0.5f,
        4.0f, 3.0f, 2.0f, 1.0f};
    constexpr float targetPixels[]{
        9.5f, 8.25f, 7.125f, 0.875f,
        6.5f, 5.25f, 4.125f, 0.625f,
        3.5f, 2.25f, 1.125f, 0.375f,
        -1.0f, -2.0f, -3.0f, 1.0f};
    ImageF32x4_RGBA firstFrame;
    firstFrame.setFromBGRA32F(firstPixels, 2, 2);
    ImageF32x4_RGBA targetFrame;
    targetFrame.setFromBGRA32F(targetPixels, 2, 2);

    const auto firstWrite = producer->writeFrame(3, firstFrame, 0x11u);
    ASSERT_TRUE(firstWrite.success) << firstWrite.error.toStdString();
    const auto targetWrite = producer->writeFrame(8, targetFrame, 0x22u);
    ASSERT_TRUE(targetWrite.success) << targetWrite.error.toStdString();
    EXPECT_EQ(consumer->pendingFrameCount(), 1)
        << "the API reports whether any frames are pending, not an exact count";

    const auto received = consumer->readFrame(8);
    ASSERT_TRUE(received.success) << received.error.toStdString();
    ASSERT_NE(received.frame, nullptr);
    EXPECT_EQ(received.frameNumber, 8);
    EXPECT_GT(received.timestampNs, 0u);
    ASSERT_EQ(received.frame->width(), 2);
    ASSERT_EQ(received.frame->height(), 2);
    for (std::size_t index = 0; index < 16; ++index) {
        EXPECT_FLOAT_EQ(received.frame->rgba32fData()[index], targetPixels[index])
            << "BGRA float component=" << index;
    }
    EXPECT_TRUE(consumer->readNextFrame().error == QStringLiteral("Buffer is empty"));
}

TEST(RenderFarmSharedBufferContractTest,
     RejectsOversizedFrameWithoutPoisoningTheNextPacket)
{
    RenderFarmSharedBuffer::Config config;
    config.bufferName = uniqueBufferName();
    config.totalSizeMB = 1;
    config.maxFrameWidth = 2;
    config.maxFrameHeight = 2;
    auto consumer = RenderFarmSharedBuffer::createConsumer(config);
    ASSERT_NE(consumer, nullptr);
    auto producer = RenderFarmSharedBuffer::openProducer(config.bufferName);
    ASSERT_NE(producer, nullptr);

    constexpr float oversizedPixels[24] = {};
    ImageF32x4_RGBA oversized;
    oversized.setFromBGRA32F(oversizedPixels, 3, 2);
    const auto rejected = producer->writeFrame(4, oversized);
    EXPECT_FALSE(rejected.success);
    EXPECT_EQ(consumer->pendingFrameCount(), 0);

    constexpr float validPixels[]{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 1.0f, 1.1f, 1.2f,
        1.3f, 1.4f, 1.5f, 1.6f};
    ImageF32x4_RGBA validFrame;
    validFrame.setFromBGRA32F(validPixels, 2, 2);
    const auto validWrite = producer->writeFrame(5, validFrame);
    ASSERT_TRUE(validWrite.success) << validWrite.error.toStdString();

    const auto received = consumer->readNextFrame();
    ASSERT_TRUE(received.success) << received.error.toStdString();
    ASSERT_NE(received.frame, nullptr);
    EXPECT_EQ(received.frameNumber, 5);
    for (std::size_t index = 0; index < 16; ++index) {
        EXPECT_FLOAT_EQ(received.frame->rgba32fData()[index], validPixels[index])
            << "BGRA float component=" << index;
    }
    EXPECT_TRUE(consumer->readNextFrame().error == QStringLiteral("Buffer is empty"));
}

TEST(RenderFarmSharedBufferContractTest,
     TruncatedFramePacketIsConsumedAndNextValidFrameCanBeRead)
{
    RenderFarmSharedBuffer::Config config;
    config.bufferName = uniqueBufferName();
    config.totalSizeMB = 1;
    config.maxFrameWidth = 4;
    config.maxFrameHeight = 4;
    auto consumer = RenderFarmSharedBuffer::createConsumer(config);
    ASSERT_NE(consumer, nullptr);
    auto rawProducer = SharedMemoryRingBuffer::open(config.bufferName);
    ASSERT_NE(rawProducer, nullptr);

    const auto malformedWrite = rawProducer->write(QByteArrayLiteral("short"));
    ASSERT_TRUE(malformedWrite.success) << malformedWrite.error.toStdString();
    const auto malformedRead = consumer->readNextFrame();
    EXPECT_FALSE(malformedRead.success);
    EXPECT_EQ(malformedRead.error, QStringLiteral("Invalid frame packet"));
    EXPECT_EQ(consumer->pendingFrameCount(), 0);

    auto producer = RenderFarmSharedBuffer::openProducer(config.bufferName);
    ASSERT_NE(producer, nullptr);
    constexpr float pixels[]{0.25f, 0.5f, 0.75f, 1.0f};
    ImageF32x4_RGBA frame;
    frame.setFromBGRA32F(pixels, 1, 1);
    const auto validWrite = producer->writeFrame(12, frame);
    ASSERT_TRUE(validWrite.success) << validWrite.error.toStdString();

    const auto validRead = consumer->readNextFrame();
    ASSERT_TRUE(validRead.success) << validRead.error.toStdString();
    ASSERT_NE(validRead.frame, nullptr);
    EXPECT_EQ(validRead.frameNumber, 12);
    for (std::size_t index = 0; index < 4; ++index) {
        EXPECT_FLOAT_EQ(validRead.frame->rgba32fData()[index], pixels[index]);
    }
    EXPECT_EQ(consumer->pendingFrameCount(), 0);
}

TEST(RenderFarmSharedBufferContractTest,
     ZeroDimensionFramePacketIsRejectedAndNextValidFrameCanBeRead)
{
    struct FramePacketHeaderForTest {
        std::uint32_t magic = 0x41524651u;
        std::uint32_t width = 0;
        std::uint32_t height = 1;
        std::uint32_t flags = 0;
        std::int64_t frameNumber = 11;
    };
    static_assert(sizeof(FramePacketHeaderForTest) == 24);

    RenderFarmSharedBuffer::Config config;
    config.bufferName = uniqueBufferName();
    config.totalSizeMB = 1;
    config.maxFrameWidth = 4;
    config.maxFrameHeight = 4;
    auto consumer = RenderFarmSharedBuffer::createConsumer(config);
    ASSERT_NE(consumer, nullptr);
    auto rawProducer = SharedMemoryRingBuffer::open(config.bufferName);
    ASSERT_NE(rawProducer, nullptr);

    const FramePacketHeaderForTest malformedHeader;
    QByteArray malformedPacket(static_cast<qsizetype>(sizeof(malformedHeader)), Qt::Uninitialized);
    std::memcpy(malformedPacket.data(), &malformedHeader, sizeof(malformedHeader));
    const auto malformedWrite = rawProducer->write(malformedPacket);
    ASSERT_TRUE(malformedWrite.success) << malformedWrite.error.toStdString();

    const auto malformedRead = consumer->readNextFrame();
    EXPECT_FALSE(malformedRead.success)
        << "a frame with a zero dimension must not be reported as decoded";
    EXPECT_EQ(malformedRead.error, QStringLiteral("Invalid frame dimensions"));
    EXPECT_EQ(consumer->pendingFrameCount(), 0);

    auto producer = RenderFarmSharedBuffer::openProducer(config.bufferName);
    ASSERT_NE(producer, nullptr);
    constexpr float pixels[]{0.25f, 0.5f, 0.75f, 1.0f};
    ImageF32x4_RGBA frame;
    frame.setFromBGRA32F(pixels, 1, 1);
    const auto validWrite = producer->writeFrame(12, frame);
    ASSERT_TRUE(validWrite.success) << validWrite.error.toStdString();

    const auto validRead = consumer->readNextFrame();
    ASSERT_TRUE(validRead.success) << validRead.error.toStdString();
    ASSERT_NE(validRead.frame, nullptr);
    EXPECT_EQ(validRead.frameNumber, 12);
    ASSERT_EQ(validRead.frame->width(), 1);
    ASSERT_EQ(validRead.frame->height(), 1);
    for (std::size_t index = 0; index < 4; ++index) {
        EXPECT_FLOAT_EQ(validRead.frame->rgba32fData()[index], pixels[index]);
    }
}

TEST(RenderFarmSharedBufferContractTest,
     OverflowingFrameDimensionsAreRejectedWithoutConstructingAnImage)
{
    struct FramePacketHeaderForTest {
        std::uint32_t magic = 0x41524651u;
        std::uint32_t width = 0xC0000000u;
        std::uint32_t height = 0x40000000u;
        std::uint32_t flags = 0;
        std::int64_t frameNumber = 13;
    };
    static_assert(sizeof(FramePacketHeaderForTest) == 24);

    RenderFarmSharedBuffer::Config config;
    config.bufferName = uniqueBufferName();
    config.totalSizeMB = 1;
    config.maxFrameWidth = 4;
    config.maxFrameHeight = 4;
    auto consumer = RenderFarmSharedBuffer::createConsumer(config);
    ASSERT_NE(consumer, nullptr);
    auto rawProducer = SharedMemoryRingBuffer::open(config.bufferName);
    ASSERT_NE(rawProducer, nullptr);

    constexpr std::uint32_t overflowDimensions[][2]{
        {0xC0000000u, 0x40000000u},
        {0x40000000u, 0xC0000000u},
        {0x80000000u, 0x80000000u},
    };
    for (const auto& dimensions : overflowDimensions) {
        FramePacketHeaderForTest malformedHeader;
        malformedHeader.width = dimensions[0];
        malformedHeader.height = dimensions[1];
        QByteArray malformedPacket(static_cast<qsizetype>(sizeof(malformedHeader)), Qt::Uninitialized);
        std::memcpy(malformedPacket.data(), &malformedHeader, sizeof(malformedHeader));
        const auto malformedWrite = rawProducer->write(malformedPacket);
        ASSERT_TRUE(malformedWrite.success) << malformedWrite.error.toStdString();

        const auto malformedRead = consumer->readNextFrame();
        EXPECT_FALSE(malformedRead.success)
            << "overflowing dimensions outside the configured bounds must not decode"
            << " width=" << dimensions[0] << " height=" << dimensions[1];
        EXPECT_EQ(malformedRead.error, QStringLiteral("Invalid frame dimensions"));
        EXPECT_EQ(consumer->pendingFrameCount(), 0);
    }
}

TEST(IPCChannelContractTest,
     LocalSocketReassemblesFragmentedLengthPrefixAndRetainsCoalescedMessages)
{
    const QString serverName = uniqueBufferName();
    QLocalServer server;
    ASSERT_TRUE(server.listen(serverName)) << server.errorString().toStdString();

    IPCChannelConfig config;
    config.transport = IPCTransport::LocalSocket;
    config.name = serverName;
    config.bufferSize = 256;
    config.timeoutMs = 1000;
    auto channel = IPCChannel::create(config);
    ASSERT_NE(channel, nullptr);
    ASSERT_TRUE(channel->isConnected());
    ASSERT_TRUE(server.waitForNewConnection(1000));
    QLocalSocket* peer = server.nextPendingConnection();
    ASSERT_NE(peer, nullptr);

    const auto frameMessage = [](const QByteArray& message) {
        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream << static_cast<quint32>(message.size());
        frame.append(message);
        return frame;
    };
    const auto readPeerBytes = [peer](qsizetype expectedSize) {
        QByteArray bytes;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(1);
        while (bytes.size() < expectedSize &&
               std::chrono::steady_clock::now() < deadline) {
            if (peer->bytesAvailable() == 0 && !peer->waitForReadyRead(20)) {
                continue;
            }
            bytes.append(peer->readAll());
        }
        return bytes;
    };
    const QByteArray firstPayload("fragmented prefix payload");
    const QByteArray secondPayload("coalesced following message");
    const QByteArray firstFrame = frameMessage(firstPayload);
    const QByteArray secondFrame = frameMessage(secondPayload);

    ASSERT_EQ(peer->write(firstFrame.constData(), 2), 2);
    ASSERT_TRUE(peer->waitForBytesWritten(1000));
    EXPECT_TRUE(channel->receiveBlocking(100).isEmpty())
        << "an incomplete 2-byte length prefix is not a complete message";

    const QByteArray remaining = firstFrame.mid(2) + secondFrame;
    ASSERT_EQ(peer->write(remaining), remaining.size());
    ASSERT_TRUE(peer->waitForBytesWritten(1000));
    EXPECT_EQ(channel->receiveBlocking(1000), firstPayload);
    EXPECT_EQ(channel->receiveBlocking(0), secondPayload)
        << "a complete coalesced frame in the internal buffer needs no socket wait";
    EXPECT_EQ(channel->receive(), secondPayload)
        << "the synchronous receive path can still consume the buffered frame";
    EXPECT_EQ(channel->pendingBytes(), 0u);

    const QByteArray outboundPayload("channel-to-peer\0binary", 22);
    ASSERT_TRUE(channel->send(outboundPayload));
    const QByteArray outboundFrame = frameMessage(outboundPayload);
    EXPECT_EQ(readPeerBytes(outboundFrame.size()), outboundFrame);

    const QByteArray zeroCopyPayload("zero-copy socket route");
    ASSERT_TRUE(channel->sendZeroCopy(
        reinterpret_cast<const std::uint8_t*>(zeroCopyPayload.constData()),
        static_cast<std::size_t>(zeroCopyPayload.size())));
    const QByteArray zeroCopyFrame = frameMessage(zeroCopyPayload);
    EXPECT_EQ(readPeerBytes(zeroCopyFrame.size()), zeroCopyFrame);
}

TEST(IPCChannelContractTest,
     OversizedLocalSocketFrameDoesNotBlockFollowingValidMessage)
{
    const QString serverName = uniqueBufferName();
    QLocalServer server;
    ASSERT_TRUE(server.listen(serverName)) << server.errorString().toStdString();

    IPCChannelConfig config;
    config.transport = IPCTransport::LocalSocket;
    config.name = serverName;
    config.bufferSize = 16;
    config.timeoutMs = 1000;
    auto channel = IPCChannel::create(config);
    ASSERT_NE(channel, nullptr);
    ASSERT_TRUE(channel->isConnected());
    ASSERT_TRUE(server.waitForNewConnection(1000));
    QLocalSocket* peer = server.nextPendingConnection();
    ASSERT_NE(peer, nullptr);

    const auto frameMessage = [](const QByteArray& message) {
        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream << static_cast<quint32>(message.size());
        frame.append(message);
        return frame;
    };
    const QByteArray oversizedPayload(17, 'x');
    const QByteArray validPayload("valid");
    const QByteArray frames = frameMessage(oversizedPayload) +
                              frameMessage(validPayload);
    ASSERT_EQ(peer->write(frames), frames.size());
    ASSERT_TRUE(peer->waitForBytesWritten(1000));

    EXPECT_TRUE(channel->receiveBlocking(100).isEmpty())
        << "the over-limit payload must not be delivered";
    EXPECT_EQ(channel->receiveBlocking(1000), validPayload)
        << "discarding the complete oversized frame must preserve the next frame";
}

TEST(IPCChannelContractTest,
     EmptyLocalSocketFrameDoesNotConsumeFollowingMessageBoundary)
{
    const QString serverName = uniqueBufferName();
    QLocalServer server;
    ASSERT_TRUE(server.listen(serverName)) << server.errorString().toStdString();

    IPCChannelConfig config;
    config.transport = IPCTransport::LocalSocket;
    config.name = serverName;
    config.bufferSize = 64;
    config.timeoutMs = 1000;
    auto channel = IPCChannel::create(config);
    ASSERT_NE(channel, nullptr);
    ASSERT_TRUE(channel->isConnected());
    ASSERT_TRUE(server.waitForNewConnection(1000));
    QLocalSocket* peer = server.nextPendingConnection();
    ASSERT_NE(peer, nullptr);

    const auto frameMessage = [](const QByteArray& message) {
        QByteArray frame;
        QDataStream stream(&frame, QIODevice::WriteOnly);
        stream << static_cast<quint32>(message.size());
        frame.append(message);
        return frame;
    };
    const QByteArray nextPayload("after empty frame");
    const QByteArray frames = frameMessage(QByteArray()) + frameMessage(nextPayload);
    ASSERT_EQ(peer->write(frames), frames.size());
    ASSERT_TRUE(peer->waitForBytesWritten(1000));

    EXPECT_TRUE(channel->receive().isEmpty())
        << "a legal zero-length frame has an empty payload";
    EXPECT_EQ(channel->receive(), nextPayload)
        << "consuming an empty frame must leave the following frame intact";
    EXPECT_TRUE(channel->receive().isEmpty());
}

TEST(IPCChannelContractTest,
     ZeroCopyReceiveKeepsMessageWhenDestinationBufferIsTooSmall)
{
    const QString serverName = uniqueBufferName();
    QLocalServer server;
    ASSERT_TRUE(server.listen(serverName)) << server.errorString().toStdString();

    IPCChannelConfig config;
    config.transport = IPCTransport::LocalSocket;
    config.name = serverName;
    config.bufferSize = 64;
    config.timeoutMs = 1000;
    auto channel = IPCChannel::create(config);
    ASSERT_NE(channel, nullptr);
    ASSERT_TRUE(channel->isConnected());
    ASSERT_TRUE(server.waitForNewConnection(1000));
    QLocalSocket* peer = server.nextPendingConnection();
    ASSERT_NE(peer, nullptr);

    const QByteArray payload("binary\0payload", 14);
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream << static_cast<quint32>(payload.size());
    frame.append(payload);
    ASSERT_EQ(peer->write(frame), frame.size());
    ASSERT_TRUE(peer->waitForBytesWritten(1000));
    const auto receiveDeadline = std::chrono::steady_clock::now() +
                                 std::chrono::seconds(1);
    while (channel->pendingBytes() < static_cast<std::size_t>(frame.size()) &&
           std::chrono::steady_clock::now() < receiveDeadline) {
        std::this_thread::yield();
    }
    ASSERT_GE(channel->pendingBytes(), static_cast<std::size_t>(frame.size()));

    std::uint8_t smallBuffer[3]{};
    std::size_t received = 99;
    EXPECT_FALSE(channel->receiveZeroCopy(smallBuffer, sizeof(smallBuffer), received));
    EXPECT_EQ(received, 0u);

    std::uint8_t fullBuffer[14]{};
    ASSERT_TRUE(channel->receiveZeroCopy(fullBuffer, sizeof(fullBuffer), received));
    ASSERT_EQ(received, sizeof(fullBuffer));
    EXPECT_EQ(QByteArray(reinterpret_cast<const char*>(fullBuffer),
                         static_cast<qsizetype>(received)), payload);
}

TEST(IPCChannelContractTest,
     SharedMemoryZeroCopyRoundTripPreservesRawBinaryPayloadAcrossHandles)
{
    IPCChannelConfig config;
    config.transport = IPCTransport::SharedMemory;
    config.name = uniqueBufferName();
    config.bufferSize = 128;
    config.create = true;
    auto producer = IPCChannel::create(config);
    ASSERT_NE(producer, nullptr);
    ASSERT_TRUE(producer->isConnected());
    ASSERT_EQ(producer->transport(), IPCTransport::SharedMemory);

    config.create = false;
    auto consumer = IPCChannel::create(config);
    ASSERT_NE(consumer, nullptr);
    ASSERT_TRUE(consumer->isConnected());
    ASSERT_EQ(consumer->transport(), IPCTransport::SharedMemory);

    const std::uint8_t payload[]{0x00u, 0x11u, 0xFEu, 0x00u,
                                 0x7Fu, 0x80u, 0x42u, 0xFFu};
    ASSERT_TRUE(producer->sendZeroCopy(payload, sizeof(payload)));
    EXPECT_EQ(consumer->pendingBytes(),
              sizeof(ArtifactCore::IPC::RingBufferEntryHeader) + 8u);

    std::uint8_t receivedBytes[sizeof(payload)]{};
    std::size_t received = 99;
    ASSERT_TRUE(consumer->receiveZeroCopy(receivedBytes, sizeof(receivedBytes), received));
    EXPECT_EQ(received, sizeof(payload));
    for (std::size_t index = 0; index < sizeof(payload); ++index) {
        EXPECT_EQ(receivedBytes[index], payload[index]) << "byte=" << index;
    }
    EXPECT_EQ(consumer->pendingBytes(), 0u);
}
