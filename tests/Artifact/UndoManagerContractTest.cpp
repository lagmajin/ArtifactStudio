#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

#include <QFile>
#include <QDir>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

import Undo.UndoManager;

namespace {

class IntegerCommand final : public Artifact::UndoCommand {
public:
    IntegerCommand(int& value, int before, int after, bool failFirstRedo = false)
        : value_(value), before_(before), after_(after),
          failRedoCall_(failFirstRedo ? 1 : 0) {}

    IntegerCommand(int& value, int before, int after, int failRedoCall)
        : value_(value), before_(before), after_(after), failRedoCall_(failRedoCall) {}

    IntegerCommand(int& value, int before, int after, int failRedoCall,
                   bool failFirstUndo)
        : value_(value), before_(before), after_(after),
          failRedoCall_(failRedoCall), failFirstUndo_(failFirstUndo) {}

    void undo() override
    {
        value_ = before_;
        ++undoCalls_;
        succeeded_ = !(failFirstUndo_ && undoCalls_ == 1);
    }

    void redo() override
    {
        value_ = after_;
        ++redoCalls_;
        succeeded_ = redoCalls_ != failRedoCall_;
    }

    bool lastOperationSucceeded() const override { return succeeded_; }
    size_t estimatedMemoryBytes() const override { return sizeof(*this); }

private:
    int& value_;
    int before_;
    int after_;
    int failRedoCall_ = 0;
    int redoCalls_ = 0;
    bool failFirstUndo_ = false;
    int undoCalls_ = 0;
    bool succeeded_ = true;
};

class PersistentIntegerCommand final : public Artifact::UndoCommand {
public:
    PersistentIntegerCommand(int& value, int before, int after)
        : value_(value), before_(before), after_(after) {}

    void undo() override { value_ = before_; }
    void redo() override { value_ = after_; }
    size_t estimatedMemoryBytes() const override { return sizeof(*this); }
    QString label() const override { return QStringLiteral("Persistent integer edit"); }
    QString commandType() const override { return QStringLiteral("TestPersistentInteger"); }
    bool canSerialize() const override { return true; }
    QJsonObject serialize() const override
    {
        return {{QStringLiteral("before"), before_},
                {QStringLiteral("after"), after_}};
    }
    bool deserialize(const QJsonObject& data) override
    {
        const auto before = data.value(QStringLiteral("before"));
        const auto after = data.value(QStringLiteral("after"));
        if (!before.isDouble() || !after.isDouble()) return false;
        before_ = before.toInt();
        after_ = after.toInt();
        return true;
    }

private:
    int& value_;
    int before_;
    int after_;
};

} // namespace

TEST(UndoManagerContractTest, PushUndoAndRedoMoveOneCommandBetweenStacks)
{
    Artifact::UndoManager manager;
    int value = 0;

    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 7)));
    EXPECT_EQ(value, 7);
    EXPECT_TRUE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 1u);

    manager.undo();
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_TRUE(manager.canRedo());
    EXPECT_EQ(manager.redoCount(), 1u);

    manager.redo();
    EXPECT_EQ(value, 7);
    EXPECT_TRUE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
}

TEST(UndoManagerContractTest, NewPushAfterUndoDiscardsTheOldRedoBranch)
{
    Artifact::UndoManager manager;
    int value = 0;

    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 1)));
    manager.undo();
    ASSERT_TRUE(manager.canRedo());

    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 2)));
    EXPECT_EQ(value, 2);
    EXPECT_FALSE(manager.canRedo());
    EXPECT_EQ(manager.redoCount(), 0u);

    manager.undo();
    EXPECT_EQ(value, 0);
    manager.redo();
    EXPECT_EQ(value, 2);
}

TEST(UndoManagerContractTest, FailedInitialRedoIsCompensatedAndNotRecorded)
{
    Artifact::UndoManager manager;
    int value = 3;

    EXPECT_FALSE(manager.push(
        std::make_unique<IntegerCommand>(value, 3, 9, true)));
    EXPECT_EQ(value, 3);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 0u);
}

TEST(UndoManagerContractTest,
     FailedInitialMacroPushCompensatesPartialChildAndAllEarlierEdits)
{
    Artifact::UndoManager manager;
    int value = 0;
    auto macro = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Three edits with a partially failing final edit"));
    macro->addChild(std::make_unique<IntegerCommand>(value, 0, 2));
    macro->addChild(std::make_unique<IntegerCommand>(value, 2, 5));
    // The failed redo mutates to 9 before reporting failure. The macro must
    // compensate that partial edit, then unwind both earlier children.
    macro->addChild(std::make_unique<IntegerCommand>(value, 5, 9, 1));

    EXPECT_FALSE(manager.push(std::move(macro)));
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 0u);
}

TEST(UndoManagerContractTest,
     FailedMacroDeserializationPreservesExistingCommandTree)
{
    int value = 0;
    auto macro = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Existing macro"));
    macro->addChild(std::make_unique<PersistentIntegerCommand>(value, 0, 1));
    const QJsonObject originalSerialization = macro->serialize();
    ASSERT_FALSE(originalSerialization.isEmpty());

    Artifact::UndoManager::instance()->registerCommandFactory(
        QStringLiteral("TestPersistentInteger"), [&value](const QJsonObject&) {
            return std::make_unique<PersistentIntegerCommand>(value, 0, 0);
        });
    const QJsonObject validChild{
        {QStringLiteral("type"), QStringLiteral("TestPersistentInteger")},
        {QStringLiteral("data"), QJsonObject{
            {QStringLiteral("before"), 5}, {QStringLiteral("after"), 8}}}};
    const QJsonObject unknownChild{
        {QStringLiteral("type"), QStringLiteral("UnknownCommand")},
        {QStringLiteral("data"), QJsonObject{}}};
    const QJsonObject partiallyInvalid{
        {QStringLiteral("label"), QStringLiteral("Replacement macro")},
        {QStringLiteral("children"), QJsonArray{validChild, unknownChild}}};

    EXPECT_FALSE(macro->deserialize(partiallyInvalid));
    EXPECT_EQ(macro->label(), QStringLiteral("Existing macro"));
    EXPECT_EQ(macro->serialize(), originalSerialization);

    macro->redo();
    EXPECT_TRUE(macro->lastOperationSucceeded());
    EXPECT_EQ(value, 1);
}

TEST(UndoManagerContractTest,
     SessionHistoryReloadRestoresUndoableBranchWithoutReapplyingCurrentState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString historyPath = directory.filePath(QStringLiteral("undo-history.json"));

    int savedProjectValue = 0;
    {
        Artifact::UndoManager source;
        ASSERT_TRUE(source.push(std::make_unique<PersistentIntegerCommand>(
            savedProjectValue, 0, 1)));
        ASSERT_TRUE(source.push(std::make_unique<PersistentIntegerCommand>(
            savedProjectValue, 1, 2)));
        ASSERT_EQ(savedProjectValue, 2);
        source.undo();
        ASSERT_EQ(savedProjectValue, 1);
        ASSERT_EQ(source.undoCount(), 1u);
        ASSERT_EQ(source.redoCount(), 1u);
        ASSERT_TRUE(source.saveSessionHistory(historyPath));
    }

    // Project persistence restores the current value independently; history
    // loading must reconstruct commands without replaying them on that value.
    int reloadedProjectValue = 1;
    Artifact::UndoManager restored;
    restored.registerCommandFactory(QStringLiteral("TestPersistentInteger"),
        [&reloadedProjectValue](const QJsonObject&) {
            return std::make_unique<PersistentIntegerCommand>(
                reloadedProjectValue, 0, 0);
        });
    ASSERT_TRUE(restored.loadSessionHistory(historyPath));
    EXPECT_EQ(reloadedProjectValue, 1);
    EXPECT_EQ(restored.undoCount(), 1u);
    EXPECT_EQ(restored.redoCount(), 0u);

    restored.undo();
    EXPECT_EQ(reloadedProjectValue, 0);
    EXPECT_TRUE(restored.canRedo());
    restored.redo();
    EXPECT_EQ(reloadedProjectValue, 1);
    EXPECT_TRUE(restored.canUndo());
}

TEST(UndoManagerContractTest,
     FailedSessionHistoryLoadKeepsExistingValueBranchesAndVersion)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString historyPath = directory.filePath(QStringLiteral("invalid-history.json"));

    const QJsonObject validEntry{
        {QStringLiteral("type"), QStringLiteral("TestPersistentInteger")},
        {QStringLiteral("label"), QStringLiteral("Persistent integer edit")},
        {QStringLiteral("data"), QJsonObject{
            {QStringLiteral("before"), 10}, {QStringLiteral("after"), 20}}}};
    const QJsonObject invalidEntry{
        {QStringLiteral("type"), QStringLiteral("TestPersistentInteger")},
        {QStringLiteral("label"), QStringLiteral("Persistent integer edit")},
        {QStringLiteral("data"), QJsonObject{
            {QStringLiteral("before"), QStringLiteral("not-an-integer")},
            {QStringLiteral("after"), 30}}}};
    const QJsonObject history{
        {QStringLiteral("version"), 1},
        {QStringLiteral("savedVersion"), 0},
        {QStringLiteral("currentVersion"), 2},
        {QStringLiteral("entries"), QJsonArray{validEntry, invalidEntry}}};
    QFile historyFile(historyPath);
    ASSERT_TRUE(historyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray payload = QJsonDocument(history).toJson(QJsonDocument::Compact);
    ASSERT_EQ(historyFile.write(payload), payload.size());
    historyFile.close();

    Artifact::UndoManager manager;
    int value = 0;
    manager.registerCommandFactory(QStringLiteral("TestPersistentInteger"),
        [&value](const QJsonObject&) {
            return std::make_unique<PersistentIntegerCommand>(value, 0, 0);
        });
    ASSERT_TRUE(manager.push(std::make_unique<PersistentIntegerCommand>(value, 0, 1)));
    ASSERT_TRUE(manager.push(std::make_unique<PersistentIntegerCommand>(value, 1, 2)));
    manager.undo();
    manager.markAsSaved();
    ASSERT_EQ(value, 1);
    ASSERT_EQ(manager.undoCount(), 1u);
    ASSERT_EQ(manager.redoCount(), 1u);
    const std::int64_t originalVersion = manager.currentVersion();
    const bool originalDirtyState = manager.hasUnsavedChanges();

    EXPECT_FALSE(manager.loadSessionHistory(historyPath));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 1u);
    EXPECT_EQ(manager.currentVersion(), originalVersion);
    EXPECT_EQ(manager.hasUnsavedChanges(), originalDirtyState);

    manager.undo();
    EXPECT_EQ(value, 0);
    manager.redo();
    EXPECT_EQ(value, 1);
}

TEST(UndoManagerContractTest,
     CorruptOffloadedCommandCanBeRetriedWithoutLosingUndoState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    Artifact::UndoManager manager;
    int value = 0;
    manager.registerCommandFactory(QStringLiteral("TestPersistentInteger"),
        [&value](const QJsonObject&) {
            return std::make_unique<PersistentIntegerCommand>(value, 0, 0);
        });
    manager.setOffloadDirectory(directory.path());
    ASSERT_TRUE(manager.push(std::make_unique<PersistentIntegerCommand>(value, 0, 1)));
    ASSERT_TRUE(manager.push(std::make_unique<PersistentIntegerCommand>(value, 1, 2)));
    manager.setOffloadPolicy(Artifact::UndoManager::OffloadPolicy::Always);
    ASSERT_EQ(value, 2);

    const QStringList offloadFiles = QDir(directory.path()).entryList(
        QStringList{QStringLiteral("undo_*.json")}, QDir::Files);
    ASSERT_EQ(offloadFiles.size(), 1);
    const QString offloadPath = QDir(directory.path()).filePath(offloadFiles.front());
    QFile offloadFile(offloadPath);
    ASSERT_TRUE(offloadFile.open(QIODevice::ReadOnly));
    const QByteArray validPayload = offloadFile.readAll();
    offloadFile.close();
    ASSERT_FALSE(validPayload.isEmpty());

    manager.undo(); // The in-memory newest command moves to redo.
    ASSERT_EQ(value, 1);
    ASSERT_EQ(manager.undoCount(), 1u);
    ASSERT_EQ(manager.redoCount(), 1u);
    manager.markAsSaved();
    const std::int64_t versionBeforeFailedUndo = manager.currentVersion();

    ASSERT_TRUE(offloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray corruptPayload = QByteArrayLiteral("{");
    ASSERT_EQ(offloadFile.write(corruptPayload), corruptPayload.size());
    offloadFile.close();

    manager.undo();

    EXPECT_EQ(value, 1);
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 1u);
    EXPECT_EQ(manager.currentVersion(), versionBeforeFailedUndo);
    EXPECT_FALSE(manager.hasUnsavedChanges());

    ASSERT_TRUE(offloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_EQ(offloadFile.write(validPayload), validPayload.size());
    offloadFile.close();
    manager.undo();
    EXPECT_EQ(value, 0);
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 2u);

    manager.redo();
    EXPECT_EQ(value, 1);
    manager.redo();
    EXPECT_EQ(value, 2);
}

TEST(UndoManagerContractTest,
     ActionRecordingReplayIsAtomicAndProducesOneUndoableMacro)
{
    Artifact::UndoManager source;
    int sourceValue = 0;
    ASSERT_TRUE(source.beginActionRecording(QStringLiteral("Five integer edits")));
    for (int index = 0; index < 5; ++index) {
        ASSERT_TRUE(source.push(std::make_unique<PersistentIntegerCommand>(
            sourceValue, index, index + 1)));
    }
    const QJsonObject recording = source.endActionRecording();
    ASSERT_FALSE(recording.isEmpty());
    ASSERT_EQ(recording.value(QStringLiteral("commands")).toArray().size(), 5);
    ASSERT_EQ(sourceValue, 5);

    Artifact::UndoManager replay;
    int replayValue = 0;
    replay.registerCommandFactory(QStringLiteral("TestPersistentInteger"),
        [&replayValue](const QJsonObject&) {
            return std::make_unique<PersistentIntegerCommand>(replayValue, 0, 0);
        });
    ASSERT_TRUE(replay.replayActionRecording(recording));
    EXPECT_EQ(replayValue, 5);
    EXPECT_EQ(replay.undoCount(), 1u);
    EXPECT_EQ(replay.redoCount(), 0u);

    QJsonObject invalidRecording = recording;
    QJsonArray invalidCommands = recording.value(QStringLiteral("commands")).toArray();
    QJsonObject invalidLastCommand =
        invalidCommands.at(invalidCommands.size() - 1).toObject();
    invalidLastCommand.insert(QStringLiteral("type"), QStringLiteral("UnknownCommand"));
    invalidCommands.replace(invalidCommands.size() - 1, invalidLastCommand);
    invalidRecording.insert(QStringLiteral("commands"), invalidCommands);
    const std::int64_t versionBeforeRejectedReplay = replay.currentVersion();

    EXPECT_FALSE(replay.replayActionRecording(invalidRecording));
    EXPECT_EQ(replayValue, 5);
    EXPECT_EQ(replay.undoCount(), 1u);
    EXPECT_EQ(replay.redoCount(), 0u);
    EXPECT_EQ(replay.currentVersion(), versionBeforeRejectedReplay);

    replay.undo();
    EXPECT_EQ(replayValue, 0);
    replay.redo();
    EXPECT_EQ(replayValue, 5);
}

TEST(UndoManagerContractTest,
     RejectedCollaborativePushRestoresStateAndPreservesRedoBranch)
{
    const auto verifyRejectedPush = [](const QString& rejectedAction,
                                       const QStringList& expectedActions) {
        Artifact::UndoManager manager;
        int value = 0;
        ASSERT_TRUE(manager.push(
            std::make_unique<IntegerCommand>(value, 0, 1)));
        ASSERT_TRUE(manager.push(
            std::make_unique<IntegerCommand>(value, 1, 2)));
        manager.undo();
        ASSERT_EQ(value, 1);
        ASSERT_TRUE(manager.canRedo());
        const std::int64_t versionBeforeRejectedPush = manager.currentVersion();

        QStringList observedActions;
        manager.setCollaborationEditCallback(
            [&](const Artifact::UndoCommand&, const QString& action) {
                observedActions.append(action);
                Artifact::UndoManager::CollaborationEditDispatch dispatch;
                dispatch.accepted = action != rejectedAction;
                return dispatch;
            });

        EXPECT_FALSE(manager.push(
            std::make_unique<IntegerCommand>(value, 1, 3)));
        EXPECT_EQ(value, 1);
        EXPECT_EQ(manager.undoCount(), 1u);
        EXPECT_EQ(manager.redoCount(), 1u);
        EXPECT_TRUE(manager.canUndo());
        EXPECT_TRUE(manager.canRedo());
        EXPECT_EQ(manager.currentVersion(), versionBeforeRejectedPush);
        EXPECT_FALSE(manager.hasPendingCollaborativeOperation());
        EXPECT_EQ(observedActions, expectedActions);

        manager.setCollaborationEditCallback({});
        manager.redo();
        EXPECT_EQ(value, 2);
        EXPECT_EQ(manager.undoCount(), 2u);
        EXPECT_EQ(manager.redoCount(), 0u);
    };

    verifyRejectedPush(QStringLiteral("preflight.push"),
                       {QStringLiteral("preflight.push")});
    verifyRejectedPush(QStringLiteral("apply"),
                       {QStringLiteral("preflight.push"), QStringLiteral("apply")});
}

TEST(UndoManagerContractTest,
     RejectingPendingCollaborativePushRestoresPriorRedoBranchAndVersion)
{
    Artifact::UndoManager manager;
    int value = 0;
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 1)));
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 1, 2)));
    manager.undo();
    ASSERT_EQ(value, 1);
    ASSERT_TRUE(manager.canRedo());
    manager.markAsSaved();
    const std::int64_t savedVersion = manager.currentVersion();

    manager.setCollaborationEditCallback([](
        const Artifact::UndoCommand&, const QString& action) {
        Artifact::UndoManager::CollaborationEditDispatch dispatch;
        dispatch.accepted = true;
        if (action == QStringLiteral("apply")) {
            dispatch.clientId = QStringLiteral("peer-7");
            dispatch.sequence = 42;
        }
        return dispatch;
    });

    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 1, 3)));
    ASSERT_EQ(value, 3);
    ASSERT_TRUE(manager.hasPendingCollaborativeOperation());
    QString pendingClient;
    qint64 pendingSequence = -1;
    ASSERT_TRUE(manager.pendingCollaborativeOperationIdentity(
        pendingClient, pendingSequence));
    EXPECT_EQ(pendingClient, QStringLiteral("peer-7"));
    EXPECT_EQ(pendingSequence, 42);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());

    EXPECT_FALSE(manager.rejectCollaborativeOperation(QStringLiteral("other-peer"), 42));
    EXPECT_FALSE(manager.rejectCollaborativeOperation(QStringLiteral("peer-7"), 41));
    EXPECT_TRUE(manager.hasPendingCollaborativeOperation());
    EXPECT_EQ(value, 3);

    ASSERT_TRUE(manager.rejectCollaborativeOperation(QStringLiteral("peer-7"), 42));
    EXPECT_FALSE(manager.hasPendingCollaborativeOperation());
    EXPECT_EQ(value, 1);
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 1u);
    EXPECT_EQ(manager.currentVersion(), savedVersion);
    EXPECT_FALSE(manager.hasUnsavedChanges());

    manager.redo();
    EXPECT_EQ(value, 2)
        << "rejected optimistic edit must restore the previous redo branch";
    EXPECT_FALSE(manager.hasPendingCollaborativeOperation());
}

TEST(UndoManagerContractTest,
     RejectingPendingCollaborativeUndoAndRedoRestoresBothStackTransitions)
{
    Artifact::UndoManager manager;
    int value = 0;
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 1)));
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 1, 2)));
    manager.undo();
    ASSERT_EQ(value, 1);
    manager.markAsSaved();
    const std::int64_t savedVersion = manager.currentVersion();

    manager.setCollaborationEditCallback([](
        const Artifact::UndoCommand&, const QString& action) {
        Artifact::UndoManager::CollaborationEditDispatch dispatch;
        dispatch.accepted = true;
        if (action == QStringLiteral("redo")) {
            dispatch.clientId = QStringLiteral("peer-9");
            dispatch.sequence = 90;
        } else if (action == QStringLiteral("undo")) {
            dispatch.clientId = QStringLiteral("peer-9");
            dispatch.sequence = 91;
        }
        return dispatch;
    });

    manager.redo();
    ASSERT_EQ(value, 2);
    ASSERT_TRUE(manager.hasPendingCollaborativeOperation());
    ASSERT_TRUE(manager.rejectCollaborativeOperation(QStringLiteral("peer-9"), 90));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 1u);
    EXPECT_EQ(manager.currentVersion(), savedVersion);
    EXPECT_FALSE(manager.hasUnsavedChanges());

    manager.undo();
    ASSERT_EQ(value, 0);
    ASSERT_TRUE(manager.hasPendingCollaborativeOperation());
    ASSERT_TRUE(manager.rejectCollaborativeOperation(QStringLiteral("peer-9"), 91));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 1u);
    EXPECT_EQ(manager.currentVersion(), savedVersion);
    EXPECT_FALSE(manager.hasUnsavedChanges());

    manager.setCollaborationEditCallback({});
    manager.redo();
    EXPECT_EQ(value, 2)
        << "rejecting the undo must preserve the unrelated redo branch";
    EXPECT_FALSE(manager.hasPendingCollaborativeOperation());
}

TEST(UndoManagerContractTest, FailedUndoRestoresAppliedValueAndKeepsUndoEntry)
{
    Artifact::UndoManager manager;
    int value = 0;
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 6, 0, true)));
    ASSERT_EQ(value, 6);

    manager.undo();

    EXPECT_EQ(value, 6);
    EXPECT_TRUE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 0u);
}

TEST(UndoManagerContractTest, FailedRedoRestoresUndoneValueAndKeepsRedoEntry)
{
    Artifact::UndoManager manager;
    int value = 0;
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 6, 2)));
    manager.undo();
    ASSERT_EQ(value, 0);
    ASSERT_TRUE(manager.canRedo());

    manager.redo();

    EXPECT_EQ(value, 0);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_TRUE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 1u);
}

TEST(UndoManagerContractTest, BudgetRejectsCommandBeforeExecutingIt)
{
    Artifact::UndoManager manager;
    Artifact::UndoManager::UndoBudget budget;
    budget.maxSingleEntryBytes = 0;
    manager.setBudget(budget);
    int value = 4;

    EXPECT_FALSE(manager.push(std::make_unique<IntegerCommand>(value, 4, 10)));
    EXPECT_EQ(value, 4);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_EQ(manager.undoCount(), 0u);
}

TEST(UndoManagerContractTest, EntryCountBudgetEvictsOldestUndoAndKeepsNewestUndoable)
{
    Artifact::UndoManager manager;
    Artifact::UndoManager::UndoBudget budget;
    budget.maxEntryCount = 1;
    manager.setBudget(budget);
    int value = 0;

    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 1)));
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 1, 2)));
    ASSERT_EQ(value, 2);
    ASSERT_EQ(manager.undoCount(), 1u);

    manager.undo();

    EXPECT_EQ(value, 1);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_TRUE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 1u);

    manager.redo();

    EXPECT_EQ(value, 2);
    EXPECT_TRUE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
}

TEST(UndoManagerContractTest, MemoryBudgetEvictsOldestUndoAndKeepsNewestUndoable)
{
    Artifact::UndoManager manager;
    Artifact::UndoManager::UndoBudget budget;
    budget.maxMemoryBytes = sizeof(IntegerCommand);
    budget.maxSingleEntryBytes = sizeof(IntegerCommand);
    manager.setBudget(budget);
    int value = 0;

    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 0, 1)));
    ASSERT_TRUE(manager.push(std::make_unique<IntegerCommand>(value, 1, 2)));
    ASSERT_EQ(value, 2);
    ASSERT_EQ(manager.undoCount(), 1u);
    EXPECT_LE(manager.memoryPressure(), 1.0f);

    manager.undo();

    EXPECT_EQ(value, 1);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_TRUE(manager.canRedo());

    manager.redo();

    EXPECT_EQ(value, 2);
    EXPECT_TRUE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
}

TEST(UndoManagerContractTest, MacroKeepsMultipleEditsAsOneUndoTransaction)
{
    Artifact::UndoManager manager;
    int value = 0;
    auto macro = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Two edits"));
    macro->addChild(std::make_unique<IntegerCommand>(value, 0, 3));
    macro->addChild(std::make_unique<IntegerCommand>(value, 3, 8));

    ASSERT_TRUE(manager.push(std::move(macro)));
    EXPECT_EQ(value, 8);
    EXPECT_EQ(manager.undoCount(), 1u);

    manager.undo();
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(manager.canRedo());

    manager.redo();
    EXPECT_EQ(value, 8);
    EXPECT_TRUE(manager.canUndo());
}

TEST(UndoManagerContractTest, FailedMacroRedoRollsBackEarlierChildrenAndKeepsRedoEntry)
{
    Artifact::UndoManager manager;
    int value = 0;
    auto macro = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Partially failing edits"));
    macro->addChild(std::make_unique<IntegerCommand>(value, 0, 3));
    // The first redo applies value 8 and then reports failure. MacroUndoCommand
    // must compensate that partial child and the already applied first child.
    macro->addChild(std::make_unique<IntegerCommand>(value, 3, 8, 2));

    ASSERT_TRUE(manager.push(std::move(macro)));
    ASSERT_EQ(value, 8);
    manager.undo();
    ASSERT_EQ(value, 0);
    ASSERT_TRUE(manager.canRedo());

    manager.redo();

    EXPECT_EQ(value, 0);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_TRUE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 1u);
}

TEST(UndoManagerContractTest, FailedMacroUndoRestoresEarlierUndoneChildrenAndKeepsUndoEntry)
{
    Artifact::UndoManager manager;
    int value = 0;
    auto macro = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Partially failing undo"));
    macro->addChild(std::make_unique<IntegerCommand>(value, 0, 3, 0, true));
    macro->addChild(std::make_unique<IntegerCommand>(value, 3, 8));

    ASSERT_TRUE(manager.push(std::move(macro)));
    ASSERT_EQ(value, 8);

    manager.undo();

    EXPECT_EQ(value, 8);
    EXPECT_TRUE(manager.canUndo());
    EXPECT_FALSE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 1u);
    EXPECT_EQ(manager.redoCount(), 0u);
}

TEST(UndoManagerContractTest,
     NestedMacroFailureCompensatesEachLevelAndPreservesRedoBranch)
{
    Artifact::UndoManager manager;
    int value = 0;
    auto outer = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Outer transaction"));
    outer->addChild(std::make_unique<IntegerCommand>(value, 0, 3));

    auto inner = std::make_unique<Artifact::MacroUndoCommand>(
        QStringLiteral("Inner transaction"));
    inner->addChild(std::make_unique<IntegerCommand>(value, 3, 8));
    // The first redo succeeds during push; the second applies value 13 and
    // reports failure during redo after the outer macro has been undone.
    inner->addChild(std::make_unique<IntegerCommand>(value, 8, 13, 2));
    outer->addChild(std::move(inner));

    ASSERT_TRUE(manager.push(std::move(outer)));
    ASSERT_EQ(value, 13);
    manager.undo();
    ASSERT_EQ(value, 0);
    ASSERT_TRUE(manager.canRedo());

    manager.redo();

    EXPECT_EQ(value, 0);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_TRUE(manager.canRedo());
    EXPECT_EQ(manager.undoCount(), 0u);
    EXPECT_EQ(manager.redoCount(), 1u);
}
