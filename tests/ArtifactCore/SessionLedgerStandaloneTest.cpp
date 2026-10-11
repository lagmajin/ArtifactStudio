#include <gtest/gtest.h>

#include <QString>
#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

import Core.Diagnostics.SessionLedger;

using ArtifactCore::RecoveryPoint;
using ArtifactCore::SessionEntryKind;
using ArtifactCore::SessionLedger;
using ArtifactCore::SessionLedgerEntry;

namespace {

auto writeJson(const QString& path, const QJsonObject& object) -> bool
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) >= 0;
}

} // namespace

TEST(SessionLedgerStandaloneTest, NewLedgerHasSessionIdentityAndStartTime)
{
    const SessionLedger ledger;
    EXPECT_FALSE(ledger.sessionId().isEmpty());
    EXPECT_GT(ledger.startTimeMs(), 0);
    EXPECT_TRUE(ledger.entries().empty());
    EXPECT_TRUE(ledger.recoveryPoints().empty());
}

TEST(SessionLedgerStandaloneTest, EntryKindsHaveStableSerializedNames)
{
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::ProjectOpened), QStringLiteral("project.opened"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::ProjectClosed), QStringLiteral("project.closed"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::ProjectSaved), QStringLiteral("project.saved"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::RenderStarted), QStringLiteral("render.started"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::RenderCompleted), QStringLiteral("render.completed"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::RenderFailed), QStringLiteral("render.failed"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::Crash), QStringLiteral("crash"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::RecoveryPoint), QStringLiteral("recovery.point"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(SessionEntryKind::SettingsChanged), QStringLiteral("settings.changed"));
    EXPECT_EQ(ArtifactCore::sessionEntryKindToString(static_cast<SessionEntryKind>(999)), QStringLiteral("unknown"));
}

TEST(SessionLedgerStandaloneTest, ProjectOpenRecordCapturesProjectIdentity)
{
    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("project-1"), QStringLiteral("Demo"));

    ASSERT_EQ(ledger.entries().size(), 1u);
    const auto& entry = ledger.entries().front();
    EXPECT_EQ(entry.kind, SessionEntryKind::ProjectOpened);
    EXPECT_EQ(entry.projectId, QStringLiteral("project-1"));
    EXPECT_EQ(entry.projectName, QStringLiteral("Demo"));
    EXPECT_GE(entry.timestampMs, ledger.startTimeMs());
    EXPECT_FALSE(entry.isRecoverable);
}

TEST(SessionLedgerStandaloneTest, ProjectCloseAndSaveRecordsCaptureExpectedFields)
{
    SessionLedger ledger;
    ledger.recordProjectClosed(QStringLiteral("project-closed"));
    ledger.recordProjectSaved(QStringLiteral("project-saved"), QStringLiteral("Saved"));

    ASSERT_EQ(ledger.entries().size(), 2u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::ProjectClosed);
    EXPECT_EQ(ledger.entries()[0].projectId, QStringLiteral("project-closed"));
    EXPECT_EQ(ledger.entries()[1].kind, SessionEntryKind::ProjectSaved);
    EXPECT_EQ(ledger.entries()[1].projectId, QStringLiteral("project-saved"));
    EXPECT_EQ(ledger.entries()[1].projectName, QStringLiteral("Saved"));
}

TEST(SessionLedgerStandaloneTest, RenderRecordsKeepJobAndOutputDetails)
{
    SessionLedger ledger;
    ledger.recordRenderStarted(4, QStringLiteral("Preview"));
    ledger.recordRenderCompleted(4, QStringLiteral("out/frame.exr"));

    ASSERT_EQ(ledger.entries().size(), 2u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::RenderStarted);
    EXPECT_EQ(ledger.entries()[0].jobIndex, 4);
    EXPECT_EQ(ledger.entries()[0].detail, QStringLiteral("Preview"));
    EXPECT_EQ(ledger.entries()[1].kind, SessionEntryKind::RenderCompleted);
    EXPECT_EQ(ledger.entries()[1].jobIndex, 4);
    EXPECT_EQ(ledger.entries()[1].detail, QStringLiteral("out/frame.exr"));
    EXPECT_FALSE(ledger.entries()[1].isRecoverable);
}

TEST(SessionLedgerStandaloneTest, RenderFailureIsMarkedRecoverable)
{
    SessionLedger ledger;
    ledger.recordRenderFailed(8, QStringLiteral("device lost"));

    ASSERT_EQ(ledger.entries().size(), 1u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::RenderFailed);
    EXPECT_EQ(ledger.entries()[0].jobIndex, 8);
    EXPECT_EQ(ledger.entries()[0].detail, QStringLiteral("device lost"));
    EXPECT_TRUE(ledger.entries()[0].isRecoverable);
    EXPECT_EQ(ledger.recoverableEntries().size(), 1u);
}

TEST(SessionLedgerStandaloneTest, CrashRecordIsNotAutomaticallyRecoverable)
{
    SessionLedger ledger;
    ledger.recordCrash(QStringLiteral("unexpected exit"));

    ASSERT_EQ(ledger.entries().size(), 1u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::Crash);
    EXPECT_EQ(ledger.entries()[0].detail, QStringLiteral("unexpected exit"));
    EXPECT_FALSE(ledger.entries()[0].isRecoverable);
    EXPECT_TRUE(ledger.recoverableEntries().empty());
}

TEST(SessionLedgerStandaloneTest, RecoverableEntriesFilterOutOrdinaryEvents)
{
    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("p"), QStringLiteral("P"));
    ledger.recordRenderFailed(1, QStringLiteral("failed"));
    SessionLedgerEntry custom;
    custom.kind = SessionEntryKind::RecoveryPoint;
    custom.isRecoverable = true;
    ledger.addEntry(custom);

    const auto recoverable = ledger.recoverableEntries();
    ASSERT_EQ(recoverable.size(), 2u);
    EXPECT_EQ(recoverable[0].kind, SessionEntryKind::RenderFailed);
    EXPECT_EQ(recoverable[1].kind, SessionEntryKind::RecoveryPoint);
}

TEST(SessionLedgerStandaloneTest, RecoveryPointsAreStoredInInsertionOrder)
{
    SessionLedger ledger;
    RecoveryPoint first;
    first.id = QStringLiteral("rp-1");
    first.snapshotPath = QStringLiteral("one.snapshot");
    RecoveryPoint second;
    second.id = QStringLiteral("rp-2");
    second.isAutosave = true;
    ledger.addRecoveryPoint(first);
    ledger.addRecoveryPoint(second);

    ASSERT_EQ(ledger.recoveryPoints().size(), 2u);
    EXPECT_EQ(ledger.recoveryPoints()[0].id, QStringLiteral("rp-1"));
    EXPECT_EQ(ledger.recoveryPoints()[0].snapshotPath, QStringLiteral("one.snapshot"));
    EXPECT_EQ(ledger.recoveryPoints()[1].id, QStringLiteral("rp-2"));
    EXPECT_TRUE(ledger.recoveryPoints()[1].isAutosave);
}

TEST(SessionLedgerStandaloneTest, JsonContainsSchemaSessionEventsAndRecoveryPoints)
{
    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("p-1"), QStringLiteral("Project"));
    RecoveryPoint point;
    point.id = QStringLiteral("rp-1");
    point.timestampMs = 1234;
    point.projectId = QStringLiteral("p-1");
    point.projectName = QStringLiteral("Project");
    point.snapshotPath = QStringLiteral("autosave.snapshot");
    point.isAutosave = true;
    ledger.addRecoveryPoint(point);

    const auto json = ledger.toJson();
    EXPECT_EQ(json.value(QStringLiteral("schemaVersion")).toInt(), 1);
    EXPECT_EQ(json.value(QStringLiteral("sessionId")).toString(), ledger.sessionId());
    ASSERT_EQ(json.value(QStringLiteral("entries")).toArray().size(), 1);
    EXPECT_EQ(json.value(QStringLiteral("entries")).toArray()[0].toObject()
                  .value(QStringLiteral("kind")).toString(), QStringLiteral("project.opened"));
    ASSERT_EQ(json.value(QStringLiteral("recoveryPoints")).toArray().size(), 1);
    EXPECT_TRUE(json.value(QStringLiteral("recoveryPoints")).toArray()[0].toObject()
                    .value(QStringLiteral("isAutosave")).toBool());
}

TEST(SessionLedgerStandaloneTest, SaveAndLoadRoundTripPreservesAllPersistedFields)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.json"));
    SessionLedger source;
    source.recordProjectOpened(QStringLiteral("project-7"), QStringLiteral("Round Trip"));
    source.recordRenderFailed(9, QStringLiteral("encoder error"));
    RecoveryPoint point;
    point.id = QStringLiteral("recovery-7");
    point.timestampMs = 987654;
    point.projectId = QStringLiteral("project-7");
    point.projectName = QStringLiteral("Round Trip");
    point.snapshotPath = QStringLiteral("snapshot.bin");
    point.isAutosave = true;
    source.addRecoveryPoint(point);

    ASSERT_TRUE(source.saveToFile(path));
    SessionLedger restored;
    ASSERT_TRUE(restored.loadFromFile(path));
    EXPECT_EQ(restored.sessionId(), source.sessionId());
    EXPECT_EQ(restored.startTimeMs(), source.startTimeMs());
    ASSERT_EQ(restored.entries().size(), 2u);
    EXPECT_EQ(restored.entries()[0].projectName, QStringLiteral("Round Trip"));
    EXPECT_TRUE(restored.entries()[1].isRecoverable);
    ASSERT_EQ(restored.recoveryPoints().size(), 1u);
    EXPECT_EQ(restored.recoveryPoints()[0].id, QStringLiteral("recovery-7"));
    EXPECT_EQ(restored.recoveryPoints()[0].timestampMs, 987654);
    EXPECT_TRUE(restored.recoveryPoints()[0].isAutosave);
}

TEST(SessionLedgerStandaloneTest, EmptyOrUnwritableSavePathReturnsFalse)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SessionLedger ledger;
    EXPECT_FALSE(ledger.saveToFile(QString()));
    EXPECT_FALSE(ledger.saveToFile(directory.filePath(QStringLiteral("missing/session.json"))));
}

TEST(SessionLedgerStandaloneTest, EmptyOrMissingLoadPathReturnsFalse)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SessionLedger ledger;
    EXPECT_FALSE(ledger.loadFromFile(QString()));
    EXPECT_FALSE(ledger.loadFromFile(directory.filePath(QStringLiteral("missing.json"))));
}

TEST(SessionLedgerStandaloneTest, InvalidJsonDoesNotReplaceExistingLedgerState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("invalid.json"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(QByteArray("{")), 1);
    file.close();

    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("keep"), QStringLiteral("Keep"));
    const QString originalId = ledger.sessionId();
    EXPECT_FALSE(ledger.loadFromFile(path));
    EXPECT_EQ(ledger.sessionId(), originalId);
    ASSERT_EQ(ledger.entries().size(), 1u);
    EXPECT_EQ(ledger.entries()[0].projectId, QStringLiteral("keep"));
}

TEST(SessionLedgerStandaloneTest, UnsupportedSchemaVersionIsRejectedWithoutMutation)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 2);
    json.insert(QStringLiteral("sessionId"), QStringLiteral("other"));
    const QString path = directory.filePath(QStringLiteral("future.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("keep"), QStringLiteral("Keep"));
    const QString originalId = ledger.sessionId();
    EXPECT_FALSE(ledger.loadFromFile(path));
    EXPECT_EQ(ledger.sessionId(), originalId);
    ASSERT_EQ(ledger.entries().size(), 1u);
    EXPECT_EQ(ledger.entries()[0].projectId, QStringLiteral("keep"));
}

TEST(SessionLedgerStandaloneTest, MissingSessionIdIsRejected)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("entries"), QJsonArray{});
    const QString path = directory.filePath(QStringLiteral("no-id.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    EXPECT_FALSE(ledger.loadFromFile(path));
    EXPECT_FALSE(ledger.sessionId().isEmpty());
    EXPECT_TRUE(ledger.entries().empty());
}

TEST(SessionLedgerStandaloneTest, EmptySessionIdIsRejected)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("sessionId"), QString());
    const QString path = directory.filePath(QStringLiteral("empty-id.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    EXPECT_FALSE(ledger.loadFromFile(path));
    EXPECT_FALSE(ledger.sessionId().isEmpty());
}

TEST(SessionLedgerStandaloneTest, NonObjectJsonRootIsRejected)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("array-root.json"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(QByteArray("[]")), 2);
    file.close();

    SessionLedger ledger;
    EXPECT_FALSE(ledger.loadFromFile(path));
}

TEST(SessionLedgerStandaloneTest, MissingEntryFieldsUseTheirDocumentedDefaults)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject entry;
    entry.insert(QStringLiteral("kind"), QStringLiteral("render.started"));
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("sessionId"), QStringLiteral("defaults"));
    json.insert(QStringLiteral("entries"), QJsonArray{entry});
    const QString path = directory.filePath(QStringLiteral("defaults.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    ASSERT_TRUE(ledger.loadFromFile(path));
    ASSERT_EQ(ledger.entries().size(), 1u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::RenderStarted);
    EXPECT_EQ(ledger.entries()[0].timestampMs, 0);
    EXPECT_EQ(ledger.entries()[0].jobIndex, -1);
    EXPECT_TRUE(ledger.entries()[0].detail.isEmpty());
    EXPECT_FALSE(ledger.entries()[0].isRecoverable);
}

TEST(SessionLedgerStandaloneTest, SuccessfulLoadWithMissingArraysReplacesOldContents)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("sessionId"), QStringLiteral("empty-content"));
    const QString path = directory.filePath(QStringLiteral("empty-content.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("old"), QStringLiteral("Old"));
    RecoveryPoint oldPoint;
    oldPoint.id = QStringLiteral("old-recovery");
    ledger.addRecoveryPoint(oldPoint);
    ASSERT_TRUE(ledger.loadFromFile(path));

    EXPECT_EQ(ledger.sessionId(), QStringLiteral("empty-content"));
    EXPECT_EQ(ledger.startTimeMs(), 0);
    EXPECT_TRUE(ledger.entries().empty());
    EXPECT_TRUE(ledger.recoveryPoints().empty());
}

TEST(SessionLedgerStandaloneTest, LoadPreservesSerializedEntryOrderAndTimestamps)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject first;
    first.insert(QStringLiteral("kind"), QStringLiteral("project.opened"));
    first.insert(QStringLiteral("timestampMs"), 101);
    QJsonObject second;
    second.insert(QStringLiteral("kind"), QStringLiteral("render.completed"));
    second.insert(QStringLiteral("timestampMs"), 202);
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("sessionId"), QStringLiteral("ordered"));
    json.insert(QStringLiteral("entries"), QJsonArray{first, second});
    const QString path = directory.filePath(QStringLiteral("ordered.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    ASSERT_TRUE(ledger.loadFromFile(path));
    ASSERT_EQ(ledger.entries().size(), 2u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::ProjectOpened);
    EXPECT_EQ(ledger.entries()[0].timestampMs, 101);
    EXPECT_EQ(ledger.entries()[1].kind, SessionEntryKind::RenderCompleted);
    EXPECT_EQ(ledger.entries()[1].timestampMs, 202);
}

TEST(SessionLedgerStandaloneTest, UnknownSerializedEntryKindFallsBackToProjectOpened)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QJsonObject entry;
    entry.insert(QStringLiteral("kind"), QStringLiteral("future.event"));
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), 1);
    json.insert(QStringLiteral("sessionId"), QStringLiteral("session"));
    json.insert(QStringLiteral("entries"), QJsonArray{entry});
    const QString path = directory.filePath(QStringLiteral("unknown-kind.json"));
    ASSERT_TRUE(writeJson(path, json));

    SessionLedger ledger;
    ASSERT_TRUE(ledger.loadFromFile(path));
    ASSERT_EQ(ledger.entries().size(), 1u);
    EXPECT_EQ(ledger.entries()[0].kind, SessionEntryKind::ProjectOpened);
}

TEST(SessionLedgerStandaloneTest, ClearRemovesEntriesAndRecoveryPointsButKeepsSession)
{
    SessionLedger ledger;
    ledger.recordProjectOpened(QStringLiteral("p"), QStringLiteral("Project"));
    RecoveryPoint point;
    point.id = QStringLiteral("rp");
    ledger.addRecoveryPoint(point);
    const QString sessionId = ledger.sessionId();
    const qint64 startTime = ledger.startTimeMs();

    ledger.clear();
    EXPECT_TRUE(ledger.entries().empty());
    EXPECT_TRUE(ledger.recoveryPoints().empty());
    EXPECT_EQ(ledger.sessionId(), sessionId);
    EXPECT_EQ(ledger.startTimeMs(), startTime);
}
