#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

import Serialization.JsonAdapter;
import Serialization.Envelope;
import Serialization.Registry;

using namespace ArtifactCore::Serialization;

namespace {
struct JsonObjectRecord {
    int id = 0;
    QString label;

    QJsonObject toJson() const
    {
        return {{QStringLiteral("id"), id}, {QStringLiteral("label"), label}};
    }

    static JsonObjectRecord fromJson(const QJsonObject& object)
    {
        return {object.value(QStringLiteral("id")).toInt(),
                object.value(QStringLiteral("label")).toString()};
    }
};

struct StaticJsonObjectRecord {
    QString value;

    QJsonObject toJson() const
    {
        return {{QStringLiteral("value"), value}};
    }

    static StaticJsonObjectRecord fromJsonStatic(const QJsonObject& object)
    {
        return {object.value(QStringLiteral("value")).toString()};
    }
};

struct JsonArrayRecord {
    QStringList values;

    QJsonArray toJson() const
    {
        QJsonArray result;
        for (const auto& value : values) result.append(value);
        return result;
    }

    static JsonArrayRecord fromJson(const QJsonArray& array)
    {
        JsonArrayRecord result;
        for (const auto& value : array) result.values.append(value.toString());
        return result;
    }
};

struct FactorySnapshotRecord final : ISerializable {
    explicit FactorySnapshotRecord(QString value) : value(std::move(value)) {}

    QJsonObject serialize() const override
    {
        return {{QStringLiteral("value"), value}};
    }

    bool deserialize(const QJsonObject& object) override
    {
        if (!object.value(QStringLiteral("value")).isString()) return false;
        value = object.value(QStringLiteral("value")).toString();
        return true;
    }

    QString typeName() const override
    {
        return QStringLiteral("JsonSerializableAdapterContractTest.FactorySnapshot");
    }

    int schemaVersion() const override { return 1; }

    QString value;
};
}

TEST(JsonSerializableAdapterContractTest, RegisteredObjectAdapterRoundTripsThroughEnvelope)
{
    const QString typeName = QStringLiteral("JsonSerializableAdapterContractTest.Object");
    registerJsonSerializableType<JsonObjectRecord>(typeName, 4);

    auto source = SerializationRegistry::instance().create(typeName);
    ASSERT_NE(source, nullptr);
    auto* sourceAdapter = dynamic_cast<JsonSerializableAdapter<JsonObjectRecord>*>(source.get());
    ASSERT_NE(sourceAdapter, nullptr);
    EXPECT_EQ(sourceAdapter->typeName(), typeName);
    EXPECT_EQ(sourceAdapter->schemaVersion(), 4);
    sourceAdapter->value() = {19, QStringLiteral("record-a")};

    const QJsonObject envelope = makeEnvelope(*sourceAdapter);
    auto decoded = deserializeEnvelope(envelope);
    ASSERT_NE(decoded, nullptr);
    auto* decodedAdapter = dynamic_cast<JsonSerializableAdapter<JsonObjectRecord>*>(decoded.get());
    ASSERT_NE(decodedAdapter, nullptr);
    EXPECT_EQ(decodedAdapter->value().id, 19);
    EXPECT_EQ(decodedAdapter->value().label, QStringLiteral("record-a"));
    EXPECT_EQ(decodedAdapter->serialize(), sourceAdapter->serialize());
}

TEST(JsonSerializableAdapterContractTest,
     RegistrationsForOneValueTypeKeepIndependentEnvelopeMetadata)
{
    const QString firstType =
        QStringLiteral("JsonSerializableAdapterContractTest.ObjectAlias.First");
    const QString secondType =
        QStringLiteral("JsonSerializableAdapterContractTest.ObjectAlias.Second");
    registerJsonSerializableType<JsonObjectRecord>(firstType, 2);
    auto first = SerializationRegistry::instance().create(firstType);
    ASSERT_NE(first, nullptr);
    const QJsonObject firstEnvelope = makeEnvelope(*first);

    registerJsonSerializableType<JsonObjectRecord>(secondType, 7);
    auto second = SerializationRegistry::instance().create(secondType);
    ASSERT_NE(second, nullptr);

    // Registering another alias after an instance exists must not rewrite the
    // first instance's envelope identity or schema version.
    EXPECT_EQ(first->typeName(), firstType);
    EXPECT_EQ(first->schemaVersion(), 2);
    EXPECT_EQ(second->typeName(), secondType);
    EXPECT_EQ(second->schemaVersion(), 7);

    const QJsonObject secondEnvelope = makeEnvelope(*second);
    EXPECT_EQ(firstEnvelope.value(QStringLiteral("_type")).toString(), firstType);
    EXPECT_EQ(firstEnvelope.value(QStringLiteral("_schemaVersion")).toInt(), 2);
    EXPECT_EQ(secondEnvelope.value(QStringLiteral("_type")).toString(), secondType);
    EXPECT_EQ(secondEnvelope.value(QStringLiteral("_schemaVersion")).toInt(), 7);
}

TEST(JsonSerializableAdapterContractTest,
     FactoryCanRegisterAnotherTypeReentrantly)
{
    const QString factoryType =
        QStringLiteral("JsonSerializableAdapterContractTest.ReentrantFactory");
    const QString nestedType =
        QStringLiteral("JsonSerializableAdapterContractTest.RegisteredByFactory");
    auto& registry = SerializationRegistry::instance();
    registerJsonSerializableType<StaticJsonObjectRecord>(factoryType, 4);
    registry.registerType(factoryType, [nestedType] {
        registerJsonSerializableType<JsonObjectRecord>(nestedType, 6);
        return std::make_unique<JsonSerializableAdapter<StaticJsonObjectRecord>>();
    });

    auto outer = registry.create(factoryType);
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->typeName(), factoryType);
    EXPECT_EQ(outer->schemaVersion(), 4);
    EXPECT_TRUE(registry.isRegistered(nestedType));

    auto nested = registry.create(nestedType);
    ASSERT_NE(nested, nullptr);
    EXPECT_EQ(nested->typeName(), nestedType);
    EXPECT_EQ(nested->schemaVersion(), 6);
}

TEST(JsonSerializableAdapterContractTest, SupportsStaticObjectAndArrayConversionForms)
{
    const QString staticType = QStringLiteral("JsonSerializableAdapterContractTest.StaticObject");
    registerJsonSerializableType<StaticJsonObjectRecord>(staticType, 2);
    auto staticValue = SerializationRegistry::instance().create(staticType);
    ASSERT_NE(staticValue, nullptr);
    ASSERT_TRUE(staticValue->deserialize(
        QJsonObject{{QStringLiteral("value"), QStringLiteral("static-value")}}));
    auto* staticAdapter = dynamic_cast<JsonSerializableAdapter<StaticJsonObjectRecord>*>(staticValue.get());
    ASSERT_NE(staticAdapter, nullptr);
    EXPECT_EQ(staticAdapter->value().value, QStringLiteral("static-value"));

    const QString arrayType = QStringLiteral("JsonSerializableAdapterContractTest.Array");
    registerJsonArraySerializableType<JsonArrayRecord>(arrayType, 7);
    auto arrayValue = SerializationRegistry::instance().create(arrayType);
    ASSERT_NE(arrayValue, nullptr);
    EXPECT_FALSE(arrayValue->deserialize(QJsonObject{
        {QStringLiteral("items"), QJsonObject{{QStringLiteral("wrong"), true}}}}));
    ASSERT_TRUE(arrayValue->deserialize(QJsonObject{
        {QStringLiteral("items"), QJsonArray{QStringLiteral("alpha"), QStringLiteral("beta")}}}));

    auto* arrayAdapter = dynamic_cast<JsonArraySerializableAdapter<JsonArrayRecord>*>(arrayValue.get());
    ASSERT_NE(arrayAdapter, nullptr);
    EXPECT_EQ(arrayAdapter->typeName(), arrayType);
    EXPECT_EQ(arrayAdapter->schemaVersion(), 7);
    EXPECT_EQ(arrayAdapter->value().values, (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));
    EXPECT_EQ(arrayAdapter->serialize().value(QStringLiteral("items")).toArray(),
              QJsonArray{QStringLiteral("alpha"), QStringLiteral("beta")});
}

TEST(JsonSerializableAdapterContractTest,
     FailedArrayDeserializationPreservesCurrentValueAndAllowsRecovery)
{
    const QString typeName =
        QStringLiteral("JsonSerializableAdapterContractTest.ArrayAtomicity");
    registerJsonArraySerializableType<JsonArrayRecord>(typeName, 3);
    auto value = SerializationRegistry::instance().create(typeName);
    ASSERT_NE(value, nullptr);

    ASSERT_TRUE(value->deserialize(QJsonObject{
        {QStringLiteral("items"),
         QJsonArray{QStringLiteral("before"), QStringLiteral("stable")}}}));
    const QJsonObject stableSerialized = value->serialize();

    EXPECT_FALSE(value->deserialize(QJsonObject{
        {QStringLiteral("items"),
         QJsonObject{{QStringLiteral("wrong"), QStringLiteral("shape")}}}}));
    EXPECT_EQ(value->serialize(), stableSerialized);

    ASSERT_TRUE(value->deserialize(QJsonObject{
        {QStringLiteral("items"), QJsonArray{QStringLiteral("recovered")}}}));
    EXPECT_EQ(value->serialize().value(QStringLiteral("items")).toArray(),
              QJsonArray{QStringLiteral("recovered")});
}

TEST(JsonSerializableAdapterContractTest,
     ConcurrentFactoryReplacementAffectsOnlySubsequentCreateCalls)
{
    const QString typeName =
        QStringLiteral("JsonSerializableAdapterContractTest.FactorySnapshot");
    auto& registry = SerializationRegistry::instance();
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    bool firstFactoryEntered = false;
    bool releaseFirstFactory = false;
    bool replacementRegistered = false;

    registry.registerType(typeName, [&] {
        std::unique_lock lock(gateMutex);
        firstFactoryEntered = true;
        gateChanged.notify_all();
        gateChanged.wait(lock, [&] { return releaseFirstFactory; });
        return std::make_unique<FactorySnapshotRecord>(QStringLiteral("original"));
    });

    std::unique_ptr<ISerializable> inFlightValue;
    std::thread createThread([&] {
        inFlightValue = registry.create(typeName);
    });

    bool entered = false;
    {
        std::unique_lock lock(gateMutex);
        entered = gateChanged.wait_for(lock, std::chrono::seconds(5),
            [&] { return firstFactoryEntered; });
    }
    std::thread registrationThread;
    bool replacementCompletedBeforeRelease = false;
    if (entered) {
        registrationThread = std::thread([&] {
            registry.registerType(typeName, [] {
                return std::make_unique<FactorySnapshotRecord>(
                    QStringLiteral("replacement"));
            });
            {
                std::lock_guard lock(gateMutex);
                replacementRegistered = true;
            }
            gateChanged.notify_all();
        });
        std::unique_lock lock(gateMutex);
        replacementCompletedBeforeRelease = gateChanged.wait_for(
            lock, std::chrono::seconds(5), [&] { return replacementRegistered; });
    }
    {
        std::lock_guard lock(gateMutex);
        releaseFirstFactory = true;
    }
    gateChanged.notify_all();
    createThread.join();
    if (registrationThread.joinable()) registrationThread.join();

    EXPECT_TRUE(entered) << "factory should block after registry lookup completes";
    EXPECT_TRUE(replacementCompletedBeforeRelease)
        << "replacement should not wait for user factory execution to finish";
    ASSERT_NE(inFlightValue, nullptr);
    auto* inFlightRecord = dynamic_cast<FactorySnapshotRecord*>(inFlightValue.get());
    ASSERT_NE(inFlightRecord, nullptr);
    EXPECT_EQ(inFlightRecord->value, QStringLiteral("original"));

    auto subsequentValue = registry.create(typeName);
    ASSERT_NE(subsequentValue, nullptr);
    auto* subsequentRecord = dynamic_cast<FactorySnapshotRecord*>(subsequentValue.get());
    ASSERT_NE(subsequentRecord, nullptr);
    EXPECT_EQ(subsequentRecord->value, QStringLiteral("replacement"));
}
