#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <QJsonObject>
#include <QString>

import Serialization.SchemaMigration;

using ArtifactCore::Serialization::SchemaMigrationRegistry;

TEST(SchemaMigrationContractTest, AppliesChainedMigrationsInOrderAndKeepsUnknownFields)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.Chain");
    registry.registerMigration(typeName, 0, 1, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("renamed"), output.take(QStringLiteral("legacy")));
        return output;
    });
    registry.registerMigration(typeName, 1, 2, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("normalized"),
                      output.value(QStringLiteral("renamed")).toString().trimmed());
        return output;
    });

    QJsonObject data{{QStringLiteral("legacy"), QStringLiteral("  value  ")},
                      {QStringLiteral("extension"), 42}};
    ASSERT_TRUE(registry.hasMigrationPath(typeName, 0, 2));
    ASSERT_TRUE(registry.migrate(typeName, 0, 2, data));

    EXPECT_FALSE(data.contains(QStringLiteral("legacy")));
    EXPECT_EQ(data.value(QStringLiteral("renamed")).toString(),
              QStringLiteral("  value  "));
    EXPECT_EQ(data.value(QStringLiteral("normalized")).toString(),
              QStringLiteral("value"));
    EXPECT_EQ(data.value(QStringLiteral("extension")).toInt(), 42);
}

TEST(SchemaMigrationContractTest, IgnoresInvalidMigrationRegistrations)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const auto identity = [](const QJsonObject& input) { return input; };
    registry.registerMigration(QString(), 0, 1, identity);
    registry.registerMigration(QStringLiteral("SchemaMigrationContractTest.Invalid"),
                               -1, 1, identity);
    registry.registerMigration(QStringLiteral("SchemaMigrationContractTest.Invalid"),
                               1, 1, identity);
    registry.registerMigration(QStringLiteral("SchemaMigrationContractTest.Invalid"),
                               2, 1, identity);
    registry.registerMigration(QStringLiteral("SchemaMigrationContractTest.Invalid"),
                               0, 1, {});

    EXPECT_FALSE(registry.hasMigrationPath(QString(), 0, 1));
    EXPECT_FALSE(registry.hasMigrationPath(
        QStringLiteral("SchemaMigrationContractTest.Invalid"), 0, 1));
}

TEST(SchemaMigrationContractTest, MissingOrReversePathLeavesInputUnchanged)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.Direction");
    registry.registerMigration(typeName, 1, 2, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("migrated"), true);
        return output;
    });

    const QJsonObject original{{QStringLiteral("payload"), QStringLiteral("stable")}};
    QJsonObject data = original;
    EXPECT_FALSE(registry.migrate(typeName, 0, 2, data));
    EXPECT_EQ(data, original);
    EXPECT_FALSE(registry.migrate(typeName, 2, 1, data));
    EXPECT_EQ(data, original);
}

TEST(SchemaMigrationContractTest, NegativeVersionRequestsAreRejectedWithoutMutation)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.NegativeVersion");
    registry.registerMigration(typeName, 0, 1, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("migrated"), true);
        return output;
    });

    const QJsonObject original{{QStringLiteral("payload"), QStringLiteral("stable")}};
    QJsonObject data = original;
    EXPECT_FALSE(registry.hasMigrationPath(typeName, -1, 1));
    EXPECT_FALSE(registry.hasMigrationPath(typeName, 0, -1));
    EXPECT_FALSE(registry.migrate(typeName, -1, 1, data));
    EXPECT_EQ(data, original);
    EXPECT_FALSE(registry.migrate(typeName, 0, -1, data));
    EXPECT_EQ(data, original);
}

TEST(SchemaMigrationContractTest, SameVersionRequestSucceedsAsNoOp)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.Identity");
    const QJsonObject original{{QStringLiteral("payload"), QStringLiteral("unchanged")},
                               {QStringLiteral("revision"), 3}};
    QJsonObject data = original;

    EXPECT_TRUE(registry.hasMigrationPath(typeName, 3, 3));
    EXPECT_TRUE(registry.migrate(typeName, 3, 3, data));
    EXPECT_EQ(data, original);
}

TEST(SchemaMigrationContractTest, ReRegisteringMigrationEdgeReplacesCallback)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.ReplaceEdge");
    registry.registerMigration(typeName, 2, 5, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("revision"), 1);
        return output;
    });
    registry.registerMigration(typeName, 2, 5, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("revision"), 2);
        return output;
    });

    QJsonObject data;
    ASSERT_TRUE(registry.migrate(typeName, 2, 5, data));
    EXPECT_EQ(data.value(QStringLiteral("revision")).toInt(), 2);
}

TEST(SchemaMigrationContractTest, InvalidReregistrationPreservesExistingEdge)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.InvalidReplacement");
    registry.registerMigration(typeName, 1, 3, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("revision"), 7);
        return output;
    });

    registry.registerMigration(typeName, 1, 3, {});
    registry.registerMigration(typeName, -1, 3, [](const QJsonObject& input) {
        return input;
    });
    registry.registerMigration(typeName, 3, 3, [](const QJsonObject& input) {
        return input;
    });

    QJsonObject data;
    ASSERT_TRUE(registry.migrate(typeName, 1, 3, data));
    EXPECT_EQ(data.value(QStringLiteral("revision")).toInt(), 7);
    EXPECT_EQ(registry.availableVersions(typeName), (std::vector<int>{1, 3}));
}

TEST(SchemaMigrationContractTest, ChoosesTheShortestAvailableForwardPath)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.Shortest");
    registry.registerMigration(typeName, 0, 1, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("path"), QStringLiteral("long"));
        return output;
    });
    registry.registerMigration(typeName, 1, 2, [](const QJsonObject& input) {
        return input;
    });
    registry.registerMigration(typeName, 2, 4, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("steps"), 3);
        return output;
    });
    registry.registerMigration(typeName, 0, 3, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("path"), QStringLiteral("short"));
        return output;
    });
    registry.registerMigration(typeName, 3, 4, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("steps"), 2);
        return output;
    });

    QJsonObject data;
    ASSERT_TRUE(registry.migrate(typeName, 0, 4, data));
    EXPECT_EQ(data.value(QStringLiteral("path")).toString(),
              QStringLiteral("short"));
    EXPECT_EQ(data.value(QStringLiteral("steps")).toInt(), 2);
}

TEST(SchemaMigrationContractTest, AvailableVersionsAreSortedAndDeduplicated)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.Versions");
    const auto identity = [](const QJsonObject& input) { return input; };
    registry.registerMigration(typeName, 4, 9, identity);
    registry.registerMigration(typeName, 1, 4, identity);
    registry.registerMigration(typeName, 1, 7, identity);

    EXPECT_EQ(registry.availableVersions(typeName), (std::vector<int>{1, 4, 7, 9}));
    EXPECT_EQ(registry.availableVersions(QStringLiteral("  ") + typeName + QStringLiteral("  ")),
              (std::vector<int>{1, 4, 7, 9}));
}

TEST(SchemaMigrationContractTest, FailedCallbackLeavesInputUnchanged)
{
    auto& registry = SchemaMigrationRegistry::instance();
    const QString typeName = QStringLiteral("SchemaMigrationContractTest.ExceptionAtomicity");
    registry.registerMigration(typeName, 0, 1, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("stage"), 1);
        return output;
    });
    registry.registerMigration(typeName, 1, 2, [](const QJsonObject&) -> QJsonObject {
        throw std::runtime_error("migration callback failed");
    });

    const QJsonObject original{{QStringLiteral("payload"), QStringLiteral("stable")}};
    QJsonObject data = original;

    EXPECT_THROW(registry.migrate(typeName, 0, 2, data), std::runtime_error);
    EXPECT_EQ(data, original);
}
