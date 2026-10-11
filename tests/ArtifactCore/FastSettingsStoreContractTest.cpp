#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <array>
#include <thread>
#include <utility>

import Core.FastSettingsStore;

using ArtifactCore::FastSettingsStore;

TEST(FastSettingsStoreContractTest, OpeningMissingFileStartsEmptyWithoutCreatingIt)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("nested/settings.cbor"));
    FastSettingsStore store(path);

    EXPECT_EQ(store.filePath(), path);
    EXPECT_EQ(store.size(), 0);
    EXPECT_TRUE(store.keys().isEmpty());
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_FALSE(store.contains(QStringLiteral("missing")));
}

TEST(FastSettingsStoreContractTest, OpeningMalformedCborReportsFailure)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("malformed.cbor"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(QByteArray::fromHex("ff00ff00")), 4);
    file.close();

    FastSettingsStore store;
    EXPECT_FALSE(store.open(path));
    EXPECT_EQ(store.filePath(), path);
    EXPECT_EQ(store.size(), 0);
    EXPECT_FALSE(store.isDirty());
}

TEST(FastSettingsStoreContractTest, OpeningEmptyFileStartsAsCleanEmptyStore)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("empty.cbor"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    FastSettingsStore store;

    EXPECT_TRUE(store.open(path));
    EXPECT_EQ(store.filePath(), path);
    EXPECT_EQ(store.size(), 0);
    EXPECT_TRUE(store.keys().isEmpty());
    EXPECT_FALSE(store.isDirty());
}

TEST(FastSettingsStoreContractTest, FailedReloadPreservesPreviouslyLoadedValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setValue(QStringLiteral("value"), QStringLiteral("known-good"));
    ASSERT_TRUE(store.sync());

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_EQ(file.write(QByteArray::fromHex("ff00ff00")), 4);
    file.close();

    EXPECT_FALSE(store.reload(true));
    EXPECT_EQ(store.valueString(QStringLiteral("value")), QStringLiteral("known-good"));
}

TEST(FastSettingsStoreContractTest, FailedSyncRetainsDirtyValuesForRetry)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FastSettingsStore store(directory.path());
    store.setValue(QStringLiteral("pending"), QStringLiteral("retry-me"));

    EXPECT_FALSE(store.sync());
    EXPECT_TRUE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 1);
    EXPECT_EQ(store.valueString(QStringLiteral("pending")), QStringLiteral("retry-me"));
}

TEST(FastSettingsStoreContractTest, FailedSyncCanRetryAfterParentPathIsRepaired)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString parentPath = directory.filePath(QStringLiteral("blocked-parent"));
    QFile blocker(parentPath);
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    ASSERT_EQ(blocker.write("file"), 4);
    blocker.close();
    const QString path = parentPath + QStringLiteral("/settings.cbor");

    FastSettingsStore store(path);
    store.setValue(QStringLiteral("pending"), QStringLiteral("retry-me"));
    EXPECT_FALSE(store.sync());
    ASSERT_TRUE(store.isDirty());
    ASSERT_EQ(store.pendingOperations(), 1);

    ASSERT_TRUE(QFile::remove(parentPath));
    ASSERT_TRUE(QDir().mkpath(parentPath));
    ASSERT_TRUE(store.sync());

    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
    FastSettingsStore loaded(path);
    EXPECT_EQ(loaded.valueString(QStringLiteral("pending")),
              QStringLiteral("retry-me"));
}

TEST(FastSettingsStoreContractTest, OpenSwitchesFileAndReplacesCachedValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first.cbor"));
    const QString secondPath = directory.filePath(QStringLiteral("second.cbor"));
    FastSettingsStore writer(secondPath);
    writer.setValue(QStringLiteral("second-only"), 2);
    ASSERT_TRUE(writer.sync());

    FastSettingsStore store;
    ASSERT_TRUE(store.open(firstPath));
    store.setValue(QStringLiteral("first-only"), 1);
    ASSERT_TRUE(store.open(secondPath));

    EXPECT_EQ(store.filePath(), secondPath);
    EXPECT_FALSE(store.contains(QStringLiteral("first-only")));
    EXPECT_EQ(store.valueInt64(QStringLiteral("second-only")), 2);
}

TEST(FastSettingsStoreContractTest, TypedAccessorsReturnStoredValuesAndCallerDefaults)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FastSettingsStore store(directory.filePath(QStringLiteral("settings.cbor")));
    store.setValue(QStringLiteral("enabled"), true);
    store.setValue(QStringLiteral("count"), QVariant::fromValue<qlonglong>(9223372036854770000LL));
    store.setValue(QStringLiteral("label"), QStringLiteral("portrait"));

    EXPECT_TRUE(store.valueBool(QStringLiteral("enabled")));
    EXPECT_EQ(store.valueInt64(QStringLiteral("count")), 9223372036854770000LL);
    EXPECT_EQ(store.valueString(QStringLiteral("label")), QStringLiteral("portrait"));
    EXPECT_TRUE(store.valueBool(QStringLiteral("absent"), true));
    EXPECT_EQ(store.valueInt64(QStringLiteral("absent"), -17), -17);
    EXPECT_EQ(store.valueString(QStringLiteral("absent"), QStringLiteral("fallback")),
              QStringLiteral("fallback"));
}

TEST(FastSettingsStoreContractTest, SyncAndReloadPreserveNestedVariantValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("nested.cbor"));
    QVariantMap nested;
    nested.insert(QStringLiteral("enabled"), true);
    nested.insert(QStringLiteral("labels"), QStringList{
        QStringLiteral("primary"), QStringLiteral("素材")});

    FastSettingsStore writer(path);
    writer.setValue(QStringLiteral("profile"), nested);
    ASSERT_TRUE(writer.sync());

    FastSettingsStore reader(path);
    const QVariantMap restored = reader.value(QStringLiteral("profile")).toMap();
    EXPECT_TRUE(restored.value(QStringLiteral("enabled")).toBool());
    EXPECT_EQ(restored.value(QStringLiteral("labels")).toStringList(),
              (QStringList{QStringLiteral("primary"), QStringLiteral("素材")}));
}

TEST(FastSettingsStoreContractTest, EmptyKeysAreIgnoredWithoutMarkingDirty)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FastSettingsStore store(directory.filePath(QStringLiteral("settings.cbor")));

    store.setValue(QString(), 7);
    store.remove(QString());

    EXPECT_EQ(store.size(), 0);
    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
}

TEST(FastSettingsStoreContractTest, RemovingMissingKeyDoesNotCountAsMutation)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FastSettingsStore store(directory.filePath(QStringLiteral("settings.cbor")));
    store.setValue(QStringLiteral("present"), 1);
    ASSERT_TRUE(store.sync());

    store.remove(QStringLiteral("absent"));

    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
    store.remove(QStringLiteral("present"));
    EXPECT_TRUE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 1);
}

TEST(FastSettingsStoreContractTest, RepeatedKeyUpdatesCountOperationsAndPersistLatestValue)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("updates.cbor"));
    FastSettingsStore store(path);
    store.setAutoSyncThreshold(100);

    store.setValue(QStringLiteral("value"), 1);
    store.setValue(QStringLiteral("value"), 2);

    EXPECT_EQ(store.pendingOperations(), 2);
    EXPECT_EQ(store.valueInt64(QStringLiteral("value")), 2);
    ASSERT_TRUE(store.sync());
    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);

    FastSettingsStore loaded(path);
    EXPECT_EQ(loaded.valueInt64(QStringLiteral("value")), 2);
}

TEST(FastSettingsStoreContractTest, RemoveAndSyncPersistsDeletion)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setValue(QStringLiteral("temporary"), QStringLiteral("value"));
    ASSERT_TRUE(store.sync());

    store.remove(QStringLiteral("temporary"));
    ASSERT_TRUE(store.isDirty());
    ASSERT_TRUE(store.sync());

    FastSettingsStore loaded(path);
    EXPECT_FALSE(loaded.contains(QStringLiteral("temporary")));
    EXPECT_EQ(loaded.size(), 0);
}

TEST(FastSettingsStoreContractTest, ConcurrentWritersRetainEveryDistinctEntry)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FastSettingsStore store(directory.filePath(QStringLiteral("settings.cbor")));
    store.setAutoSyncThreshold(1024);

    std::array<std::thread, 4> workers;
    for (int worker = 0; worker < static_cast<int>(workers.size()); ++worker) {
        workers[static_cast<std::size_t>(worker)] = std::thread([&store, worker] {
            for (int item = 0; item < 64; ++item) {
                const QString key = QStringLiteral("worker-%1-item-%2")
                                        .arg(worker).arg(item);
                store.setValue(key, worker * 1000 + item);
            }
        });
    }
    for (auto& worker : workers) worker.join();

    ASSERT_EQ(store.size(), 256);
    for (int worker = 0; worker < static_cast<int>(workers.size()); ++worker) {
        for (int item = 0; item < 64; ++item) {
            const QString key = QStringLiteral("worker-%1-item-%2")
                                    .arg(worker).arg(item);
            EXPECT_EQ(store.valueInt64(key), worker * 1000 + item);
        }
    }
}

TEST(FastSettingsStoreContractTest, NestedBatchSyncsOnlyAfterOutermostEnd)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);

    store.beginBatch();
    store.beginBatch();
    store.setValue(QStringLiteral("value"), 42);
    EXPECT_FALSE(QFile::exists(path));
    store.endBatch();
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_TRUE(store.isDirty());

    store.endBatch();

    EXPECT_TRUE(QFile::exists(path));
    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
}

TEST(FastSettingsStoreContractTest, EndBatchCanDeferSyncUntilExplicitFlush)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);

    store.beginBatch();
    store.setValue(QStringLiteral("value"), 9);
    store.endBatch(false);
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_TRUE(store.isDirty());

    ASSERT_TRUE(store.sync());

    EXPECT_TRUE(QFile::exists(path));
    EXPECT_FALSE(store.isDirty());
}

TEST(FastSettingsStoreContractTest, AutoSyncThresholdFlushesOnConfiguredOperation)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setAutoSyncThreshold(2);
    store.setValue(QStringLiteral("first"), 1);
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_EQ(store.pendingOperations(), 1);

    store.setValue(QStringLiteral("second"), 2);

    EXPECT_TRUE(QFile::exists(path));
    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
    FastSettingsStore loaded(path);
    EXPECT_EQ(loaded.valueInt64(QStringLiteral("first")), 1);
    EXPECT_EQ(loaded.valueInt64(QStringLiteral("second")), 2);
}

TEST(FastSettingsStoreContractTest, NonpositiveAutoSyncThresholdClampsToOne)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setAutoSyncThreshold(0);
    EXPECT_EQ(store.autoSyncThreshold(), 1);

    store.setAutoSyncThreshold(-4);
    EXPECT_EQ(store.autoSyncThreshold(), 1);
    store.setValue(QStringLiteral("value"), 1);

    EXPECT_TRUE(QFile::exists(path));
    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
}

TEST(FastSettingsStoreContractTest, CleanReloadReadsUpdatesWrittenByAnotherStore)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore observer(path);
    observer.setValue(QStringLiteral("value"), QStringLiteral("initial"));
    ASSERT_TRUE(observer.sync());

    FastSettingsStore writer(path);
    writer.setValue(QStringLiteral("value"), QStringLiteral("external"));
    ASSERT_TRUE(writer.sync());
    ASSERT_EQ(observer.valueString(QStringLiteral("value")), QStringLiteral("initial"));

    ASSERT_TRUE(observer.reload());
    EXPECT_EQ(observer.valueString(QStringLiteral("value")), QStringLiteral("external"));
    EXPECT_FALSE(observer.isDirty());
}

TEST(FastSettingsStoreContractTest, InvalidNumericValueUsesInt64Fallback)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FastSettingsStore store(directory.filePath(QStringLiteral("settings.cbor")));
    store.setValue(QStringLiteral("count"), QStringLiteral("not-a-number"));

    EXPECT_EQ(store.valueInt64(QStringLiteral("count"), -23), -23);
}

TEST(FastSettingsStoreContractTest, ReloadPreservesDirtyChangesUnlessDiscardIsRequested)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setValue(QStringLiteral("value"), QStringLiteral("disk"));
    ASSERT_TRUE(store.sync());
    store.setValue(QStringLiteral("value"), QStringLiteral("local"));

    EXPECT_FALSE(store.reload());
    EXPECT_EQ(store.valueString(QStringLiteral("value")), QStringLiteral("local"));
    EXPECT_TRUE(store.isDirty());

    ASSERT_TRUE(store.reload(true));
    EXPECT_EQ(store.valueString(QStringLiteral("value")), QStringLiteral("disk"));
    EXPECT_FALSE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 0);
}

TEST(FastSettingsStoreContractTest, ClearPersistsAnEmptyStoreOutsideBatch)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setValue(QStringLiteral("value"), 1);
    ASSERT_TRUE(store.sync());

    store.clear();

    EXPECT_EQ(store.size(), 0);
    EXPECT_FALSE(store.isDirty());
    FastSettingsStore loaded(path);
    EXPECT_EQ(loaded.size(), 0);
}

TEST(FastSettingsStoreContractTest, ClearInsideBatchWaitsForOutermostEnd)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    FastSettingsStore store(path);
    store.setValue(QStringLiteral("value"), 1);
    ASSERT_TRUE(store.sync());

    store.beginBatch();
    store.clear();
    EXPECT_TRUE(store.isDirty());
    EXPECT_EQ(store.pendingOperations(), 1);
    FastSettingsStore beforeEnd(path);
    EXPECT_EQ(beforeEnd.size(), 1);
    store.endBatch();

    FastSettingsStore afterEnd(path);
    EXPECT_EQ(afterEnd.size(), 0);
    EXPECT_FALSE(store.isDirty());
}

TEST(FastSettingsStoreContractTest, DestructorFlushesPendingChanges)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    {
        FastSettingsStore store(path);
        store.setValue(QStringLiteral("value"), QStringLiteral("persisted"));
        ASSERT_TRUE(store.isDirty());
    }

    ASSERT_TRUE(QFile::exists(path));
    FastSettingsStore loaded(path);
    EXPECT_EQ(loaded.valueString(QStringLiteral("value")), QStringLiteral("persisted"));
}
