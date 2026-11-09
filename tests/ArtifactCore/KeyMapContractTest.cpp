#include <gtest/gtest.h>

#include <QString>
#include <Qt>

import Input.Operator;

namespace {

using ArtifactCore::Action;
using ArtifactCore::ActionManager;
using ArtifactCore::InputBinding;
using ArtifactCore::InputEvent;
using ArtifactCore::KeyMap;

QString actionId(const char* suffix)
{
    return QStringLiteral("contract-test.keymap.") + QString::fromLatin1(suffix);
}

} // namespace

TEST(KeyMapContractTest, RejectsConflictsAndRemovesOldIndexWhenActionIsRebound)
{
    auto* actions = ActionManager::instance();
    const QString firstId = actionId("first");
    const QString secondId = actionId("second");
    Action* firstAction = actions->registerAction(firstId, QStringLiteral("First"));
    Action* secondAction = actions->registerAction(secondId, QStringLiteral("Second"));
    ASSERT_NE(firstAction, nullptr);
    ASSERT_NE(secondAction, nullptr);

    {
        KeyMap map(QStringLiteral("contract"));
        const auto control = InputEvent::ModifierKey::LCtrl;
        InputBinding* first = map.addBinding(Qt::Key_K, control, firstAction);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(map.findBinding(Qt::Key_K, control), first);
        EXPECT_TRUE(map.isKeyMapped(Qt::Key_K, control));

        EXPECT_EQ(map.addBinding(Qt::Key_K, control, secondAction), nullptr);
        EXPECT_EQ(map.findBinding(Qt::Key_K, control), first);
        EXPECT_TRUE(map.findBindingsForAction(firstId).size() == 1u);
        EXPECT_TRUE(map.findBindingsForAction(secondId).empty());

        const auto shifted = control | InputEvent::ModifierKey::LShift;
        InputBinding* shiftedBinding = map.addBinding(Qt::Key_K, shifted, secondAction);
        ASSERT_NE(shiftedBinding, nullptr);
        EXPECT_EQ(map.findBinding(Qt::Key_K, shifted), shiftedBinding);
        EXPECT_TRUE(map.isKeyMapped(Qt::Key_K, shifted));
        EXPECT_EQ(map.findBinding(Qt::Key_K, control), first);

        InputBinding* rebound = map.addBinding(
            Qt::Key_L, InputEvent::ModifierKey::LShift, firstAction);
        ASSERT_NE(rebound, nullptr);
        EXPECT_EQ(map.findBinding(Qt::Key_K, control), nullptr);
        EXPECT_FALSE(map.isKeyMapped(Qt::Key_K, control));
        EXPECT_EQ(map.findBinding(Qt::Key_L, InputEvent::ModifierKey::LShift),
                  rebound);
        EXPECT_EQ(map.findBinding(Qt::Key_K, shifted), shiftedBinding);

        int executions = 0;
        firstAction->setExecuteCallback(
            [&](const QVariantMap&) { ++executions; });
        ASSERT_TRUE(static_cast<bool>(rebound->callback()));
        rebound->callback()();
        EXPECT_EQ(executions, 1);
    }

    actions->unregisterAction(firstId);
    actions->unregisterAction(secondId);
}

TEST(KeyMapContractTest, JsonRoundTripRestoresNameContextAndRegisteredBindings)
{
    auto* actions = ActionManager::instance();
    const QString id = actionId("json");
    Action* action = actions->registerAction(id, QStringLiteral("JSON Action"),
                                             QStringLiteral("description"));
    ASSERT_NE(action, nullptr);

    {
        KeyMap source(QStringLiteral("Source map"));
        source.setContext(QStringLiteral("Timeline"));
        ASSERT_NE(source.addBinding(Qt::Key_P,
                                    InputEvent::ModifierKey::LCtrl |
                                        InputEvent::ModifierKey::LShift,
                                    action), nullptr);

        KeyMap restored(QStringLiteral("Old map"));
        restored.setContext(QStringLiteral("Old context"));
        ASSERT_TRUE(restored.fromJSON(source.toJSON()));
        EXPECT_EQ(restored.name(), QStringLiteral("Source map"));
        EXPECT_EQ(restored.context(), QStringLiteral("Timeline"));
        ASSERT_EQ(restored.allBindings().size(), 1u);
        const auto modifiers = InputEvent::ModifierKey::LCtrl |
                               InputEvent::ModifierKey::LShift;
        InputBinding* binding = restored.findBinding(Qt::Key_P, modifiers);
        ASSERT_NE(binding, nullptr);
        EXPECT_EQ(binding->actionId(), id);

        const auto originalCount = restored.allBindings().size();
        EXPECT_FALSE(restored.fromJSON(QStringLiteral("{not valid json")));
        EXPECT_FALSE(restored.fromJSON(QStringLiteral("[]")));
        EXPECT_FALSE(restored.fromJSON(QStringLiteral(
            "{\"name\":\"Broken\",\"context\":\"Broken\",\"bindings\":17}")));
        EXPECT_EQ(restored.name(), QStringLiteral("Source map"));
        EXPECT_EQ(restored.context(), QStringLiteral("Timeline"));
        EXPECT_EQ(restored.allBindings().size(), originalCount);
        EXPECT_NE(restored.findBinding(Qt::Key_P, modifiers), nullptr);
        restored.clear();
        EXPECT_TRUE(restored.allBindings().empty());
    }

    actions->unregisterAction(id);
}

TEST(KeyMapContractTest,
     MalformedBindingEntryDoesNotPartiallyReplaceExistingMap)
{
    auto* actions = ActionManager::instance();
    const QString id = actionId("malformed-binding-transaction");
    Action* action = actions->registerAction(id, QStringLiteral("Transactional"));
    ASSERT_NE(action, nullptr);

    {
        KeyMap map(QStringLiteral("Existing map"));
        map.setContext(QStringLiteral("Timeline"));
        InputBinding* original = map.addBinding(
            Qt::Key_K, InputEvent::ModifierKey::LCtrl, action,
            QStringLiteral("original"));
        ASSERT_NE(original, nullptr);

        const QString malformed = QStringLiteral(
            "{\"name\":\"Replacement\","
            "\"context\":\"Composition\","
            "\"bindings\":["
            "{\"key\":76,\"modifiers\":0,\"actionId\":\"%1\","
            "\"description\":\"new\"},"
            "{\"key\":77,\"modifiers\":0,\"actionId\":17,"
            "\"description\":\"invalid\"}]}")
            .arg(id);

        EXPECT_FALSE(map.fromJSON(malformed));
        EXPECT_EQ(map.name(), QStringLiteral("Existing map"));
        EXPECT_EQ(map.context(), QStringLiteral("Timeline"));
        ASSERT_EQ(map.allBindings().size(), 1u);
        EXPECT_EQ(map.findBinding(Qt::Key_K, InputEvent::ModifierKey::LCtrl),
                  original);
        EXPECT_EQ(map.findBinding(Qt::Key_L, InputEvent::ModifierKey::NoModifier),
                  nullptr);
    }

    actions->unregisterAction(id);
}

TEST(KeyMapContractTest, LeftAndRightModifierBindingsRemainDistinct)
{
    auto* actions = ActionManager::instance();
    const QString leftId = actionId("left-control");
    const QString rightId = actionId("right-control");
    Action* leftAction = actions->registerAction(leftId, QStringLiteral("Left Control"));
    Action* rightAction = actions->registerAction(rightId, QStringLiteral("Right Control"));
    ASSERT_NE(leftAction, nullptr);
    ASSERT_NE(rightAction, nullptr);

    {
        KeyMap map(QStringLiteral("modifier-side-contract"));
        InputBinding* left = map.addBinding(
            Qt::Key_K, InputEvent::ModifierKey::LCtrl, leftAction);
        InputBinding* right = map.addBinding(
            Qt::Key_K, InputEvent::ModifierKey::RCtrl, rightAction);

        ASSERT_NE(left, nullptr);
        ASSERT_NE(right, nullptr);
        EXPECT_NE(left, right);
        EXPECT_EQ(map.findBinding(Qt::Key_K, InputEvent::ModifierKey::LCtrl), left);
        EXPECT_EQ(map.findBinding(Qt::Key_K, InputEvent::ModifierKey::RCtrl), right);
        EXPECT_EQ(map.findBinding(
                      Qt::Key_K,
                      InputEvent::ModifierKey::LCtrl | InputEvent::ModifierKey::RCtrl),
                  nullptr);
        EXPECT_TRUE(map.isKeyMapped(Qt::Key_K, InputEvent::ModifierKey::LCtrl));
        EXPECT_TRUE(map.isKeyMapped(Qt::Key_K, InputEvent::ModifierKey::RCtrl));
    }

    actions->unregisterAction(leftId);
    actions->unregisterAction(rightId);
}

TEST(KeyMapContractTest, ReaddingSameActionReplacesItsBindingWithoutDuplicateIndexes)
{
    auto* actions = ActionManager::instance();
    const QString id = actionId("same-action-rebind");
    Action* action = actions->registerAction(id, QStringLiteral("Rebind Action"));
    ASSERT_NE(action, nullptr);

    {
        KeyMap map(QStringLiteral("same-action-rebind-map"));
        const auto modifiers = InputEvent::ModifierKey::LCtrl;
        ASSERT_NE(map.addBinding(Qt::Key_K, modifiers, action,
                                 QStringLiteral("old description")), nullptr);

        InputBinding* replacement = map.addBinding(
            Qt::Key_L, modifiers, action, QStringLiteral("new description"));

        ASSERT_NE(replacement, nullptr);
        ASSERT_EQ(map.allBindings().size(), 1u);
        ASSERT_EQ(map.findBindingsForAction(id).size(), 1u);
        EXPECT_EQ(map.findBinding(Qt::Key_K, modifiers), nullptr);
        EXPECT_EQ(map.findBinding(Qt::Key_L, modifiers), replacement);
        EXPECT_EQ(map.findBindingsForAction(id).front(), replacement);
        EXPECT_EQ(replacement->description(), QStringLiteral("new description"));
    }

    actions->unregisterAction(id);
}
