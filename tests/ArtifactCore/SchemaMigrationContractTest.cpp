#include <gtest/gtest.h>

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
