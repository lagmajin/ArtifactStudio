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

        InputBinding* rebound = map.addBinding(
            Qt::Key_L, InputEvent::ModifierKey::LShift, firstAction);
        ASSERT_NE(rebound, nullptr);
        EXPECT_EQ(map.findBinding(Qt::Key_K, control), nullptr);
        EXPECT_FALSE(map.isKeyMapped(Qt::Key_K, control));
        EXPECT_EQ(map.findBinding(Qt::Key_L, InputEvent::ModifierKey::LShift),
                  rebound);

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
        EXPECT_EQ(restored.allBindings().size(), originalCount);
        EXPECT_NE(restored.findBinding(Qt::Key_P, modifiers), nullptr);
        restored.clear();
        EXPECT_TRUE(restored.allBindings().empty());
    }

    actions->unregisterAction(id);
}
