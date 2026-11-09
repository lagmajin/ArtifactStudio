#include <gtest/gtest.h>

#include <QString>
#include <QVariantMap>

#include <algorithm>
#include <vector>

import Event.Bus;
import Input.Operator;
import InputEvent;

namespace {

using ArtifactCore::ActionManager;
using ArtifactCore::ActionManagerChangeKind;
using ArtifactCore::ActionManagerChangedEvent;

QString uniqueActionId(const char* suffix)
{
    return QStringLiteral("contract-test.action.") + QString::fromLatin1(suffix);
}

} // namespace

TEST(ActionManagerContractTest, RegistrationIsIdempotentAndCategoryLookupIsConsistent)
{
    auto* manager = ActionManager::instance();
    ASSERT_NE(manager, nullptr);
    const QString id = uniqueActionId("registration");

    auto* action = manager->registerAction(id, QStringLiteral("Original"),
                                           QStringLiteral("description"),
                                           QStringLiteral("Contract"));
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(manager->getAction(id), action);
    EXPECT_EQ(action->label(), QStringLiteral("Original"));

    auto* duplicate = manager->registerAction(id, QStringLiteral("Ignored"),
                                              QStringLiteral("ignored"),
                                              QStringLiteral("Other"));
    EXPECT_EQ(duplicate, action);
    EXPECT_EQ(action->label(), QStringLiteral("Original"));
    EXPECT_EQ(manager->getActionsByCategory(QStringLiteral("Contract")).size(), 1u);
    EXPECT_TRUE(manager->getActionsByCategory(QStringLiteral("Other")).empty());
    manager->unregisterAction(id);
    EXPECT_EQ(manager->getAction(id), nullptr);
}

TEST(ActionManagerContractTest, ExecutionMergesStoredPropertiesWithCallOverridesAndPublishesEvent)
{
    auto* manager = ActionManager::instance();
    const QString id = uniqueActionId("execute");
    auto* action = manager->registerAction(id, QStringLiteral("Execute"), {},
                                           QStringLiteral("Contract"));
    ASSERT_NE(action, nullptr);
    action->setProperty(QStringLiteral("mode"), QStringLiteral("stored"));
    action->setProperty(QStringLiteral("value"), 1);

    QVariantMap observed;
    action->setExecuteCallback([&](const QVariantMap& params) { observed = params; });
    QVariantMap confirmed;
    action->setConfirmCallback([&](const QVariantMap& params) {
        confirmed = params;
        return true;
    });

    std::vector<ActionManagerChangedEvent> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<ActionManagerChangedEvent>(
        [&](const auto& event) { changes.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    manager->executeAction(id, QVariantMap{{QStringLiteral("value"), 7}});
    EXPECT_EQ(observed.value(QStringLiteral("mode")).toString(),
              QStringLiteral("stored"));
    EXPECT_EQ(observed.value(QStringLiteral("value")).toInt(), 7);
    EXPECT_EQ(action->property(QStringLiteral("value")).toInt(), 1);
    EXPECT_EQ(confirmed, observed);

    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes.front().kind, ActionManagerChangeKind::ActionExecuted);
    EXPECT_EQ(changes.front().actionId, id);
    EXPECT_EQ(changes.front().params.value(QStringLiteral("value")).toInt(), 7);
    manager->unregisterAction(id);
}

TEST(ActionManagerContractTest, ConfirmationCanRejectExecutionAndMissingActionDefaultsToAllowed)
{
    auto* manager = ActionManager::instance();
    const QString id = uniqueActionId("confirmation");
    auto* action = manager->registerAction(id, QStringLiteral("Confirm"));
    ASSERT_NE(action, nullptr);

    int executions = 0;
    action->setExecuteCallback([&](const QVariantMap&) { ++executions; });
    action->setConfirmCallback([](const QVariantMap&) { return false; });

    std::vector<ActionManagerChangedEvent> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<ActionManagerChangedEvent>(
        [&](const auto& event) {
            if (event.actionId == id) changes.push_back(event);
        });
    ASSERT_TRUE(subscription.connected());

    manager->executeAction(id);
    EXPECT_EQ(executions, 0);
    EXPECT_TRUE(changes.empty());
    EXPECT_FALSE(manager->confirmAction(id));
    EXPECT_TRUE(manager->confirmAction(uniqueActionId("missing")));

    manager->unregisterAction(id);
}

TEST(ActionManagerContractTest, UnregisterPublishesOnceAndRepeatedUnregisterIsNoOp)
{
    auto* manager = ActionManager::instance();
    const QString id = uniqueActionId("unregister-idempotent");
    ASSERT_NE(manager->registerAction(id, QStringLiteral("Remove me")), nullptr);

    std::vector<ActionManagerChangedEvent> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<ActionManagerChangedEvent>(
        [&](const auto& event) {
            if (event.actionId == id) changes.push_back(event);
        });
    ASSERT_TRUE(subscription.connected());

    manager->unregisterAction(id);
    manager->unregisterAction(id);

    EXPECT_EQ(manager->getAction(id), nullptr);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes.front().kind, ActionManagerChangeKind::ActionUnregistered);
    EXPECT_EQ(changes.front().actionId, id);
}

TEST(ActionManagerContractTest,
     ExecutedEventCanSynchronouslyUnregisterItsAction)
{
    auto* manager = ActionManager::instance();
    const QString id = uniqueActionId("unregister-during-executed-event");
    auto* action = manager->registerAction(id, QStringLiteral("Self removing"));
    ASSERT_NE(action, nullptr);

    int executionCount = 0;
    action->setExecuteCallback([&](const QVariantMap&) { ++executionCount; });

    std::vector<ActionManagerChangeKind> changes;
    auto subscription = ArtifactCore::globalEventBus().subscribe<ActionManagerChangedEvent>(
        [&](const auto& event) {
            if (event.actionId != id) return;
            changes.push_back(event.kind);
            if (event.kind == ActionManagerChangeKind::ActionExecuted) {
                manager->unregisterAction(id);
            }
        });
    ASSERT_TRUE(subscription.connected());

    manager->executeAction(id);

    EXPECT_EQ(executionCount, 1);
    EXPECT_EQ(manager->getAction(id), nullptr);
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(std::count(changes.begin(), changes.end(),
                         ActionManagerChangeKind::ActionExecuted), 1);
    EXPECT_EQ(std::count(changes.begin(), changes.end(),
                         ActionManagerChangeKind::ActionUnregistered), 1);
}

TEST(ActionManagerContractTest, CallbackCanReenterExecutionAndEventsRemainPostOrder)
{
    auto* manager = ActionManager::instance();
    const QString id = uniqueActionId("reentrant-execution");
    auto* action = manager->registerAction(id, QStringLiteral("Reentrant"));
    ASSERT_NE(action, nullptr);

    std::vector<int> confirmations;
    std::vector<int> executions;
    std::vector<int> publishedValues;
    action->setConfirmCallback([&](const QVariantMap& params) {
        const int value = params.value(QStringLiteral("value")).toInt();
        confirmations.push_back(value);
        return true;
    });
    action->setExecuteCallback([&](const QVariantMap& params) {
        const int value = params.value(QStringLiteral("value")).toInt();
        executions.push_back(value);
        if (value == 1) {
            manager->executeAction(id, QVariantMap{{QStringLiteral("value"), 2}});
        }
    });

    auto subscription = ArtifactCore::globalEventBus().subscribe<ActionManagerChangedEvent>(
        [&](const auto& event) {
            if (event.actionId == id && event.kind == ActionManagerChangeKind::ActionExecuted) {
                publishedValues.push_back(event.params.value(QStringLiteral("value")).toInt());
            }
        });
    ASSERT_TRUE(subscription.connected());

    manager->executeAction(id, QVariantMap{{QStringLiteral("value"), 1}});

    EXPECT_EQ(confirmations, (std::vector<int>{1, 2}));
    EXPECT_EQ(executions, (std::vector<int>{1, 2}));
    EXPECT_EQ(publishedValues, (std::vector<int>{2, 1}));
    manager->unregisterAction(id);
}
