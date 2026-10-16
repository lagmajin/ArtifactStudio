#include <gtest/gtest.h>

#include <memory>

#include <QString>

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
    budget.maxEntryCount = 0;
    manager.setBudget(budget);
    int value = 4;

    EXPECT_FALSE(manager.push(std::make_unique<IntegerCommand>(value, 4, 10)));
    EXPECT_EQ(value, 4);
    EXPECT_FALSE(manager.canUndo());
    EXPECT_EQ(manager.undoCount(), 0u);
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
