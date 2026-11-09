#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

import Serialization.Envelope;
import Serialization.ISerializable;
import Serialization.Registry;
import Serialization.SchemaMigration;

using namespace ArtifactCore::Serialization;

namespace {
const QString kPayloadType = QStringLiteral("SerializationEnvelopeContractTest.Payload");

class EnvelopePayload final : public ISerializable {
public:
    explicit EnvelopePayload(QString typeName = kPayloadType)
        : typeName_(std::move(typeName))
    {
    }

    QJsonObject serialize() const override { return data; }

    bool deserialize(const QJsonObject& input) override
    {
        if (!input.value(QStringLiteral("value")).isDouble()) return false;
        data = input;
        return true;
    }

    QString typeName() const override { return typeName_; }
    int schemaVersion() const override { return 2; }

    QJsonObject data;

private:
    QString typeName_;
};

QJsonObject makeInputEnvelope(const QJsonValue& type, const QJsonValue& version,
                              const QJsonValue& data)
{
    return {
        {QStringLiteral("_type"), type},
        {QStringLiteral("_schemaVersion"), version},
        {QStringLiteral("data"), data},
    };
}

void registerPayloadFactory()
{
    SerializationRegistry::instance().registerType(kPayloadType, [] {
        return std::make_unique<EnvelopePayload>();
    });
}
}

TEST(SerializationEnvelopeContractTest, ValidatesEnvelopeShapeAndIntegerVersionWithoutMutatingOutput)
{
    SerializationEnvelope parsed{
        QStringLiteral("sentinel"), 77,
        QJsonObject{{QStringLiteral("keep"), QStringLiteral("unchanged")}},
    };
    const SerializationEnvelope original = parsed;
    const std::vector<QJsonObject> invalidEnvelopes{
        QJsonObject{{QStringLiteral("_schemaVersion"), 1},
                    {QStringLiteral("data"), QJsonObject{}}},
        makeInputEnvelope(QStringLiteral(""), 1, QJsonObject{}),
        makeInputEnvelope(42, 1, QJsonObject{}),
        makeInputEnvelope(kPayloadType, QStringLiteral("1"), QJsonObject{}),
        makeInputEnvelope(kPayloadType, -1, QJsonObject{}),
        makeInputEnvelope(kPayloadType, 1.5, QJsonObject{}),
        makeInputEnvelope(kPayloadType,
                          static_cast<double>(std::numeric_limits<int>::max()) + 1.0,
                          QJsonObject{}),
        makeInputEnvelope(kPayloadType, 1, QJsonArray{1, 2}),
    };

    for (const auto& invalid : invalidEnvelopes) {
        EXPECT_FALSE(readEnvelope(invalid, parsed));
        EXPECT_EQ(parsed.typeName, original.typeName);
        EXPECT_EQ(parsed.schemaVersion, original.schemaVersion);
        EXPECT_EQ(parsed.data, original.data);
    }

    SerializationEnvelope maxVersion;
    ASSERT_TRUE(readEnvelope(makeInputEnvelope(
        kPayloadType, std::numeric_limits<int>::max(), QJsonObject{}), maxVersion));
    EXPECT_EQ(maxVersion.schemaVersion, std::numeric_limits<int>::max());
}

TEST(SerializationEnvelopeContractTest, DeserializesAfterApplyingTheRegisteredMigrationChain)
{
    registerPayloadFactory();
    auto& migrations = SchemaMigrationRegistry::instance();
    migrations.registerMigration(kPayloadType, 0, 1, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("value"), output.take(QStringLiteral("legacyValue")));
        return output;
    });
    migrations.registerMigration(kPayloadType, 1, 2, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("value"), output.value(QStringLiteral("value")).toInt() + 5);
        return output;
    });

    const QJsonObject input = makeInputEnvelope(
        kPayloadType, 0,
        QJsonObject{{QStringLiteral("legacyValue"), 37},
                    {QStringLiteral("extensionData"), QStringLiteral("preserve-me")}});
    auto deserialized = deserializeEnvelope(input);

    ASSERT_NE(deserialized, nullptr);
    auto* payload = dynamic_cast<EnvelopePayload*>(deserialized.get());
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->data.value(QStringLiteral("value")).toInt(), 42);
    EXPECT_FALSE(payload->data.contains(QStringLiteral("legacyValue")));
    EXPECT_EQ(payload->data.value(QStringLiteral("extensionData")).toString(), "preserve-me");

    const QJsonObject saved = makeEnvelope(*payload);
    EXPECT_EQ(saved.value(QStringLiteral("_type")).toString(), kPayloadType);
    EXPECT_EQ(saved.value(QStringLiteral("_schemaVersion")).toInt(), 2);
    EXPECT_EQ(saved.value(QStringLiteral("data")).toObject(), payload->data);
}

TEST(SerializationEnvelopeContractTest, RejectsUnknownFutureAndUnmigratableEnvelopeTypes)
{
    registerPayloadFactory();
    const QString noMigrationType = QStringLiteral("SerializationEnvelopeContractTest.NoMigration");
    SerializationRegistry::instance().registerType(noMigrationType, [noMigrationType] {
        return std::make_unique<EnvelopePayload>(noMigrationType);
    });

    EXPECT_EQ(deserializeEnvelope(makeInputEnvelope(
                  QStringLiteral("SerializationEnvelopeContractTest.Unknown"), 0,
                  QJsonObject{})), nullptr);
    EXPECT_EQ(deserializeEnvelope(makeInputEnvelope(
                  kPayloadType, 3, QJsonObject{{QStringLiteral("value"), 1}})), nullptr);
    EXPECT_EQ(deserializeEnvelope(makeInputEnvelope(
                  noMigrationType, 0, QJsonObject{{QStringLiteral("value"), 1}})), nullptr);
    EXPECT_EQ(deserializeEnvelope(makeInputEnvelope(
                  kPayloadType, 2, QJsonObject{{QStringLiteral("invalid"), true}})), nullptr);
}

TEST(SerializationEnvelopeContractTest, RejectsFutureVersionBeforeRunningMigrationCallbacks)
{
    registerPayloadFactory();
    int migrationCalls = 0;
    SchemaMigrationRegistry::instance().registerMigration(
        kPayloadType, 2, 3, [&migrationCalls](const QJsonObject& input) {
            ++migrationCalls;
            return input;
        });

    EXPECT_EQ(deserializeEnvelope(makeInputEnvelope(
                  kPayloadType, 3, QJsonObject{{QStringLiteral("value"), 9}})), nullptr);
    EXPECT_EQ(migrationCalls, 0);
}

TEST(SerializationEnvelopeContractTest,
     MigrationPathQueriesDoNotRunTransformsAndUnreachableMigrationPreservesInput)
{
    const QString typeName = QStringLiteral("SerializationEnvelopeContractTest.PathQuery");
    auto& registry = SchemaMigrationRegistry::instance();
    int transformCalls = 0;
    registry.registerMigration(typeName, 0, 1, [&transformCalls](const QJsonObject& input) {
        ++transformCalls;
        QJsonObject output = input;
        output.insert(QStringLiteral("first"), true);
        return output;
    });
    registry.registerMigration(typeName, 1, 3, [&transformCalls](const QJsonObject& input) {
        ++transformCalls;
        QJsonObject output = input;
        output.insert(QStringLiteral("third"), true);
        return output;
    });

    const QJsonObject original{{QStringLiteral("seed"), 17}};
    QJsonObject unreachableInput = original;
    EXPECT_FALSE(registry.hasMigrationPath(typeName, 0, 2));
    EXPECT_FALSE(registry.migrate(typeName, 0, 2, unreachableInput));
    EXPECT_EQ(unreachableInput, original);
    EXPECT_EQ(transformCalls, 0);

    EXPECT_TRUE(registry.hasMigrationPath(typeName, 0, 3));
    EXPECT_EQ(transformCalls, 0);

    QJsonObject migratableInput = original;
    ASSERT_TRUE(registry.migrate(typeName, 0, 3, migratableInput));
    EXPECT_EQ(transformCalls, 2);
    EXPECT_EQ(migratableInput.value(QStringLiteral("seed")).toInt(), 17);
    EXPECT_TRUE(migratableInput.value(QStringLiteral("first")).toBool());
    EXPECT_TRUE(migratableInput.value(QStringLiteral("third")).toBool());
}

TEST(SerializationEnvelopeContractTest,
     MigrationChoosesShortestPathWhenSeveralRoutesReachTargetVersion)
{
    const QString typeName =
        QStringLiteral("SerializationEnvelopeContractTest.CompetingPaths");
    auto& registry = SchemaMigrationRegistry::instance();
    std::vector<QString> executedSteps;
    const auto step = [&executedSteps](const QString& name) {
        return [&executedSteps, name](const QJsonObject& input) {
            executedSteps.push_back(name);
            QJsonObject output = input;
            output.insert(QStringLiteral("selectedPath"), name);
            return output;
        };
    };

    registry.registerMigration(typeName, 0, 1, step(QStringLiteral("long-1")));
    registry.registerMigration(typeName, 1, 2, step(QStringLiteral("long-2")));
    registry.registerMigration(typeName, 2, 4, step(QStringLiteral("long-3")));
    registry.registerMigration(typeName, 0, 3, step(QStringLiteral("short-1")));
    registry.registerMigration(typeName, 3, 4, step(QStringLiteral("short-2")));

    QJsonObject data{{QStringLiteral("seed"), 23}};
    ASSERT_TRUE(registry.migrate(typeName, 0, 4, data));

    EXPECT_EQ(executedSteps,
              (std::vector<QString>{QStringLiteral("short-1"),
                                    QStringLiteral("short-2")}));
    EXPECT_EQ(data.value(QStringLiteral("seed")).toInt(), 23);
    EXPECT_EQ(data.value(QStringLiteral("selectedPath")).toString(),
              QStringLiteral("short-2"));
}

TEST(SerializationEnvelopeContractTest,
     MigrationCallbackCanRegisterAnotherMigrationWithoutDeadlocking)
{
    const QString typeName =
        QStringLiteral("SerializationEnvelopeContractTest.ReentrantRegistry");
    auto& registry = SchemaMigrationRegistry::instance();
    registry.registerMigration(typeName, 0, 1,
        [&registry, typeName](const QJsonObject& input) {
            registry.registerMigration(typeName, 1, 2,
                [](const QJsonObject& nextInput) {
                    QJsonObject output = nextInput;
                    output.insert(QStringLiteral("secondStep"), true);
                    return output;
                });
            QJsonObject output = input;
            output.insert(QStringLiteral("firstStep"), true);
            return output;
        });

    QJsonObject data{{QStringLiteral("seed"), 11}};
    ASSERT_TRUE(registry.migrate(typeName, 0, 1, data));

    EXPECT_TRUE(data.value(QStringLiteral("firstStep")).toBool());
    EXPECT_TRUE(registry.hasMigrationPath(typeName, 1, 2));
    ASSERT_TRUE(registry.migrate(typeName, 1, 2, data));
    EXPECT_TRUE(data.value(QStringLiteral("secondStep")).toBool());
}

TEST(SerializationEnvelopeContractTest,
     MigrationPathSearchHandlesCyclesWithoutRepeatingTransforms)
{
    const QString typeName =
        QStringLiteral("SerializationEnvelopeContractTest.CyclicGraph");
    auto& registry = SchemaMigrationRegistry::instance();
    std::vector<QString> executedSteps;
    const auto step = [&executedSteps](const QString& name) {
        return [&executedSteps, name](const QJsonObject& input) {
            executedSteps.push_back(name);
            QJsonObject output = input;
            output.insert(QStringLiteral("lastStep"), name);
            return output;
        };
    };

    registry.registerMigration(typeName, 0, 1, step(QStringLiteral("forward")));
    registry.registerMigration(typeName, 1, 0, step(QStringLiteral("cycle")));
    registry.registerMigration(typeName, 1, 2, step(QStringLiteral("target")));

    EXPECT_TRUE(registry.hasMigrationPath(typeName, 0, 2));
    EXPECT_TRUE(executedSteps.empty());

    QJsonObject data{{QStringLiteral("seed"), 31}};
    ASSERT_TRUE(registry.migrate(typeName, 0, 2, data));
    EXPECT_EQ(executedSteps,
              (std::vector<QString>{QStringLiteral("forward"),
                                    QStringLiteral("target")}));
    EXPECT_EQ(data.value(QStringLiteral("seed")).toInt(), 31);
    EXPECT_EQ(data.value(QStringLiteral("lastStep")).toString(),
              QStringLiteral("target"));
}
