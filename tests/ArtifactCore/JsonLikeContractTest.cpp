#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <concepts>

import Utils.Json.Like;

using namespace ArtifactCore;

namespace {

struct ReadOnlyJson {
    QJsonValue operator[](const QString&) const { return QJsonValue(7); }
};

struct ReadableProxyJson {
    struct ValueProxy {
        QJsonValue value;
        operator QJsonValue() const { return value; }
    };

    ValueProxy operator[](const QString&) const
    {
        return ValueProxy{QJsonValue(11)};
    }
};

struct ExplicitReadableProxyJson {
    struct ValueProxy {
        QJsonValue value;
        explicit operator QJsonValue() const { return value; }
    };

    ValueProxy operator[](const QString&) const
    {
        return ValueProxy{QJsonValue(13)};
    }
};

struct WriteOnlyJson {
    QJsonValue& operator[](const QString&) { return value; }
    QJsonValue value;
};

struct NeitherJson {
    int operator[](const QString&) const { return 0; }
};

struct NonConstReadableJson {
    QJsonValue operator[](const QString&) { return QJsonValue(3); }
};

struct WrongWritableJson {
    int& operator[](const QString&) { return value; }
    int value = 0;
};

struct ProxyWritableJson {
    struct Slot {
        QJsonValue* value;
        Slot& operator=(const QJsonValue& newValue) {
            *value = newValue;
            return *this;
        }
    };

    Slot operator[](const QString&) { return Slot{&value}; }
    QJsonValue value;
};

static_assert(JsonReadable<QJsonObject>);
static_assert(JsonWritable<QJsonObject>);
static_assert(JsonLike<QJsonObject>);
static_assert(JsonReadable<ReadOnlyJson>);
static_assert(!JsonWritable<ReadOnlyJson>);
static_assert(JsonReadable<ReadableProxyJson>);
static_assert(!JsonWritable<ReadableProxyJson>);
static_assert(!JsonReadable<ExplicitReadableProxyJson>);
static_assert(!JsonLike<ExplicitReadableProxyJson>);
static_assert(!JsonReadable<WriteOnlyJson>);
static_assert(JsonWritable<WriteOnlyJson>);
static_assert(!JsonLike<NeitherJson>);
static_assert(!JsonReadable<NonConstReadableJson>);
static_assert(!JsonWritable<WrongWritableJson>);
static_assert(JsonWritable<ProxyWritableJson>);
static_assert(!JsonReadable<ProxyWritableJson>);
static_assert(!JsonLike<ProxyWritableJson>);
static_assert(!JsonReadable<QJsonArray>);
static_assert(!JsonWritable<QJsonArray>);

} // namespace

TEST(JsonLikeContractTest, JsonObjectCanReadAndWriteValues)
{
    QJsonObject object;
    const QString key = QStringLiteral("answer");
    object[key] = 42;
    EXPECT_EQ(object[key].toInt(), 42);
}

TEST(JsonLikeContractTest, JsonObjectReadableConceptWorksThroughConstReference)
{
    const QJsonObject object{{QStringLiteral("enabled"), true}};
    const auto read = [](const JsonReadable auto& json, const QString& key) {
        return json[key].toBool();
    };
    EXPECT_TRUE(read(object, QStringLiteral("enabled")));
}

TEST(JsonLikeContractTest, ReadOnlyShapeSatisfiesOnlyReadableConcept)
{
    const ReadOnlyJson json;
    EXPECT_EQ(json[QStringLiteral("any")].toInt(), 7);
    EXPECT_TRUE((JsonReadable<ReadOnlyJson>));
    EXPECT_FALSE((JsonWritable<ReadOnlyJson>));
}

TEST(JsonLikeContractTest, ReadableConceptAcceptsConvertibleProxyValues)
{
    const ReadableProxyJson json;
    const QJsonValue value = json[QStringLiteral("answer")];

    EXPECT_TRUE((JsonReadable<ReadableProxyJson>));
    EXPECT_FALSE((JsonWritable<ReadableProxyJson>));
    EXPECT_EQ(value.toInt(), 11);
}

TEST(JsonLikeContractTest, ReadableConceptRejectsExplicitOnlyValueConversions)
{
    const ExplicitReadableProxyJson json;
    const QJsonValue manuallyConverted =
        static_cast<QJsonValue>(json[QStringLiteral("answer")]);

    EXPECT_EQ(manuallyConverted.toInt(), 13);
    EXPECT_FALSE((JsonReadable<ExplicitReadableProxyJson>));
    EXPECT_FALSE((JsonLike<ExplicitReadableProxyJson>));
}

TEST(JsonLikeContractTest, WriteOnlyShapeSatisfiesOnlyWritableConcept)
{
    WriteOnlyJson json;
    json[QStringLiteral("any")] = QStringLiteral("value");
    EXPECT_EQ(json.value.toString(), QStringLiteral("value"));
    EXPECT_FALSE((JsonReadable<WriteOnlyJson>));
    EXPECT_TRUE((JsonWritable<WriteOnlyJson>));
}

TEST(JsonLikeContractTest, MissingAndExplicitNullValuesRemainDistinct)
{
    QJsonObject object;
    object.insert(QStringLiteral("explicit-null"), QJsonValue(QJsonValue::Null));

    EXPECT_FALSE(object.contains(QStringLiteral("missing")));
    EXPECT_TRUE(object.value(QStringLiteral("missing")).isUndefined());
    EXPECT_EQ(object.value(QStringLiteral("missing")).toInt(-5), -5);
    EXPECT_TRUE(object.contains(QStringLiteral("explicit-null")));
    EXPECT_TRUE(object.value(QStringLiteral("explicit-null")).isNull());
}

TEST(JsonLikeContractTest, QJsonObjectWritesNestedObjectsAndReadsBack)
{
    QJsonObject object;
    QJsonObject nested;
    nested.insert(QStringLiteral("frame"), 24);
    object[QStringLiteral("metadata")] = QJsonValue(nested);

    const QJsonValue metadata = object.value(QStringLiteral("metadata"));
    ASSERT_TRUE(metadata.isObject());
    EXPECT_EQ(metadata.toObject().value(QStringLiteral("frame")).toInt(), 24);
}

TEST(JsonLikeContractTest, WritableProxyCanAcceptJsonValueAssignment)
{
    ProxyWritableJson json;
    json[QStringLiteral("answer")] = QJsonValue(42);
    EXPECT_EQ(json.value.toInt(), 42);
    EXPECT_TRUE((JsonWritable<ProxyWritableJson>));
    EXPECT_FALSE((JsonReadable<ProxyWritableJson>));
}

TEST(JsonLikeContractTest, ConceptConstrainedWriterStoresNullAndArrayValues)
{
    QJsonObject object;
    const auto write = [](JsonWritable auto& json, const QString& key,
                          const QJsonValue& value) {
        json[key] = value;
    };

    write(object, QStringLiteral("nothing"), QJsonValue(QJsonValue::Null));
    write(object, QStringLiteral("items"), QJsonArray{1, 2, 3});

    EXPECT_TRUE(object.value(QStringLiteral("nothing")).isNull());
    const QJsonValue items = object.value(QStringLiteral("items"));
    ASSERT_TRUE(items.isArray());
    EXPECT_EQ(items.toArray().size(), 3);
    EXPECT_EQ(items.toArray().at(2).toInt(), 3);
}

TEST(JsonLikeContractTest, JsonLikeConstraintSupportsWriteAndReadRoundTrip)
{
    const auto setAndRead = [](JsonLike auto& json, const QString& key,
                               const QJsonValue& value) {
        json[key] = value;
        return QJsonValue(json[key]);
    };
    QJsonObject object;

    const QJsonValue restored = setAndRead(
        object, QStringLiteral("label"), QJsonValue(QStringLiteral("camera")));

    EXPECT_TRUE((JsonLike<QJsonObject>));
    EXPECT_EQ(restored.toString(), QStringLiteral("camera"));
    EXPECT_EQ(object.value(QStringLiteral("label")).toString(),
              QStringLiteral("camera"));
}

