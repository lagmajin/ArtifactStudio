#include <gtest/gtest.h>

#include <QString>
#include <QVariantMap>

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
    manager->executeAction(id);
    EXPECT_EQ(executions, 0);
    EXPECT_FALSE(manager->confirmAction(id));
    EXPECT_TRUE(manager->confirmAction(uniqueActionId("missing")));

    manager->unregisterAction(id);
}
