#include <gtest/gtest.h>

#include <array>
#include <memory>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QTemporaryDir>

import Serialization.ProjectSerializer;
import Serialization.ISerializable;
import Serialization.Document;
import Serialization.Registry;
import Serialization.SchemaMigration;

using namespace ArtifactCore::Serialization;

namespace {
const QString kNoteType = QStringLiteral("ProjectSerializerContractTest.Note");

class PersistedNote final : public ISerializable {
public:
    QJsonObject serialize() const override
    {
        return {{QStringLiteral("title"), title},
                {QStringLiteral("body"), body},
                {QStringLiteral("revision"), revision}};
    }

    bool deserialize(const QJsonObject& object) override
    {
        if (!object.value(QStringLiteral("title")).isString() ||
            !object.value(QStringLiteral("body")).isString() ||
            !object.value(QStringLiteral("revision")).isDouble()) {
            return false;
        }
        title = object.value(QStringLiteral("title")).toString();
        body = object.value(QStringLiteral("body")).toString();
        revision = object.value(QStringLiteral("revision")).toInt();
        return true;
    }

    QString typeName() const override { return kNoteType; }
    int schemaVersion() const override { return 3; }

    QString title;
    QString body;
    int revision = 0;
};

void registerNoteType()
{
    SerializationRegistry::instance().registerType(kNoteType, [] {
        return std::make_unique<PersistedNote>();
    });
}
}

TEST(ProjectSerializerContractTest, DetectsAndLoadsSingleDocumentJsonAndCbor)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QJsonObject original{
        {QStringLiteral("projectId"), QStringLiteral("project-17")},
        {QStringLiteral("settings"), QJsonObject{
             {QStringLiteral("frameRate"), 24},
             {QStringLiteral("colorSpace"), QStringLiteral("ACEScg")}}},
    };
    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};

    for (const auto format : formats) {
        const QString path = temporaryDirectory.filePath(
            format == SerializationFormat::Json ? QStringLiteral("project-json.bin")
                                                : QStringLiteral("project-cbor.bin"));
        ASSERT_TRUE(ProjectSerializer::save(path, original, format));
        EXPECT_EQ(ProjectSerializer::detectFormat(path), format);

        QJsonObject loaded;
        ASSERT_TRUE(ProjectSerializer::load(path, loaded));
        EXPECT_EQ(loaded, original);
    }
}

TEST(ProjectSerializerContractTest, ArtifactHelpersUseCborAndDoNotMisreadJson)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString artifactPath = temporaryDirectory.filePath(QStringLiteral("project.artifact"));
    const QJsonObject artifact{{QStringLiteral("projectId"), QStringLiteral("artifact-cbor")}};
    ASSERT_TRUE(ProjectSerializer::saveArtifact(artifactPath, artifact));
    EXPECT_EQ(ProjectSerializer::detectFormat(artifactPath), SerializationFormat::Cbor);

    QJsonObject loadedArtifact;
    QString error;
    ASSERT_TRUE(ProjectSerializer::loadArtifact(artifactPath, loadedArtifact, &error));
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(loadedArtifact, artifact);

    const QString jsonPath = temporaryDirectory.filePath(QStringLiteral("project.json"));
    ASSERT_TRUE(ProjectSerializer::save(jsonPath, artifact, SerializationFormat::Json));
    QJsonObject incorrectlyLoaded;
    error.clear();
    EXPECT_FALSE(ProjectSerializer::loadArtifact(jsonPath, incorrectlyLoaded, &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(ProjectSerializerContractTest, SerializableValueSurvivesEnvelopeFileRoundTrip)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    registerNoteType();

    PersistedNote original;
    original.title = QStringLiteral("handoff");
    original.body = QStringLiteral("keep scene and timeline in sync");
    original.revision = 12;
    const QString path = temporaryDirectory.filePath(QStringLiteral("note.cbor"));
    ASSERT_TRUE(ProjectSerializer::saveSerializable(path, original, SerializationFormat::Cbor));

    auto loaded = ProjectSerializer::loadSerializable(path);
    ASSERT_NE(loaded, nullptr);
    auto* note = dynamic_cast<PersistedNote*>(loaded.get());
    ASSERT_NE(note, nullptr);
    EXPECT_EQ(note->title, original.title);
    EXPECT_EQ(note->body, original.body);
    EXPECT_EQ(note->revision, original.revision);
}

TEST(ProjectSerializerContractTest,
     LoadsLegacySerializableEnvelopeThroughDocumentFormatAndMigrationChain)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    registerNoteType();
    auto& migrations = SchemaMigrationRegistry::instance();
    migrations.registerMigration(kNoteType, 1, 2, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("title"), output.take(QStringLiteral("subject")));
        output.insert(QStringLiteral("body"), output.take(QStringLiteral("text")));
        return output;
    });
    migrations.registerMigration(kNoteType, 2, 3, [](const QJsonObject& input) {
        QJsonObject output = input;
        output.insert(QStringLiteral("revision"),
                      output.value(QStringLiteral("revision")).toInt() + 1);
        return output;
    });

    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};
    for (const auto format : formats) {
        const QString path = temporaryDirectory.filePath(
            format == SerializationFormat::Json ? QStringLiteral("legacy.json")
                                                : QStringLiteral("legacy.cbor"));
        const QJsonObject legacyEnvelope{
            {QStringLiteral("_type"), kNoteType},
            {QStringLiteral("_schemaVersion"), 1},
            {QStringLiteral("data"), QJsonObject{
                 {QStringLiteral("subject"), QStringLiteral("handoff")},
                 {QStringLiteral("text"), QStringLiteral("migrate at file boundary")},
                 {QStringLiteral("revision"), 12},
                 {QStringLiteral("extension"), QStringLiteral("preserve-me")}}},
        };
        ASSERT_TRUE(ProjectSerializer::save(path, legacyEnvelope, format));
        EXPECT_EQ(ProjectSerializer::detectFormat(path), format);

        auto loaded = ProjectSerializer::loadSerializable(path);
        ASSERT_NE(loaded, nullptr);
        auto* note = dynamic_cast<PersistedNote*>(loaded.get());
        ASSERT_NE(note, nullptr);
        EXPECT_EQ(note->title, QStringLiteral("handoff"));
        EXPECT_EQ(note->body, QStringLiteral("migrate at file boundary"));
        EXPECT_EQ(note->revision, 13);
        const QJsonObject migratedData = note->serialize();
        EXPECT_FALSE(migratedData.contains(QStringLiteral("subject")));
        EXPECT_FALSE(migratedData.contains(QStringLiteral("text")));
        EXPECT_EQ(migratedData.value(QStringLiteral("extension")).toString(),
                  QStringLiteral("preserve-me"));
    }
}

TEST(ProjectSerializerContractTest, FailedFileLoadReportsErrorAndPreservesCallerObject)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString missingPath = temporaryDirectory.filePath(QStringLiteral("missing.project"));
    QJsonObject output{{QStringLiteral("sentinel"), QStringLiteral("preserve")}};
    const QJsonObject original = output;
    QString error;

    EXPECT_FALSE(ProjectSerializer::load(missingPath, output, &error));
    EXPECT_EQ(output, original);
    EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(ProjectSerializer::detectFormat(missingPath), SerializationFormat::Unknown);
    EXPECT_FALSE(ProjectSerializer::isSplitDirectory(missingPath));
}

TEST(ProjectSerializerContractTest,
     OversizedDocumentIsRejectedBeforeReplacingCallerOutput)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("oversized.project"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_TRUE(file.resize(SerializationDocument::kMaxDocumentBytes + 1));
    file.close();

    QJsonObject output{{QStringLiteral("sentinel"), QStringLiteral("keep")}};
    const QJsonObject original = output;
    QString error;
    EXPECT_FALSE(ProjectSerializer::load(path, output, &error));
    EXPECT_EQ(output, original);
    EXPECT_EQ(error, QStringLiteral("Serialized document is too large"));
}

TEST(ProjectSerializerContractTest,
     CorruptJsonAndCborPreserveCallerOutputAndReportDecodeFailure)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const struct {
        const char* fileName;
        QByteArray bytes;
    } cases[] = {
        {"broken.json", QByteArrayLiteral("{\"unfinished\":" )},
        // A CBOR map header declaring 24 entries with no following data.
        {"broken.cbor", QByteArray::fromHex(QByteArrayLiteral("b818"))},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const QString path = temporaryDirectory.filePath(
            QString::fromLatin1(testCase.fileName));
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ASSERT_EQ(file.write(testCase.bytes), testCase.bytes.size());
        file.close();

        QJsonObject output{{QStringLiteral("sentinel"), QStringLiteral("keep")}};
        const QJsonObject original = output;
        QString error;
        EXPECT_FALSE(ProjectSerializer::load(path, output, &error));
        EXPECT_EQ(output, original);
        EXPECT_EQ(error, QStringLiteral("Invalid serialized document"));
    }
}

TEST(ProjectSerializerContractTest,
     SplitFacadeDetectsStoreAndPreservesCallerOutputsWhenChildIsMissing)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("split-project"));
    EXPECT_FALSE(ProjectSerializer::isSplitDirectory(storePath));

    const QJsonObject originalManifest{
        {QStringLiteral("rootDocument"), QStringLiteral("scene")},
        {QStringLiteral("title"), QStringLiteral("split-facade")},
    };
    const QMap<QString, QJsonObject> originalDocuments{
        {QStringLiteral("scene"), QJsonObject{
             {QStringLiteral("id"), QStringLiteral("scene-1")},
             {QStringLiteral("layers"), QJsonArray{QStringLiteral("layer-1")}},
         }},
        {QStringLiteral("timeline"), QJsonObject{
             {QStringLiteral("frameRate"), 24},
             {QStringLiteral("workAreaEnd"), 120},
         }},
    };

    ASSERT_TRUE(ProjectSerializer::saveSplit(
        storePath, originalManifest, originalDocuments, SerializationFormat::Json));
    EXPECT_TRUE(ProjectSerializer::isSplitDirectory(storePath));

    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedDocuments;
    ASSERT_TRUE(ProjectSerializer::loadSplit(storePath, loadedManifest, loadedDocuments));
    EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(),
              QStringLiteral("split-facade"));
    EXPECT_EQ(loadedDocuments, originalDocuments);

    const QString missingChildPath = QDir(storePath).filePath(
        QStringLiteral("documents/timeline.json"));
    ASSERT_TRUE(QFile::exists(missingChildPath));
    ASSERT_TRUE(QFile::remove(missingChildPath));

    QJsonObject callerManifest{{QStringLiteral("caller"), QStringLiteral("keep")}};
    const QJsonObject originalCallerManifest = callerManifest;
    QMap<QString, QJsonObject> callerDocuments{
        {QStringLiteral("caller-child"),
         QJsonObject{{QStringLiteral("sentinel"), 7}}},
    };
    const QMap<QString, QJsonObject> originalCallerDocuments = callerDocuments;
    EXPECT_FALSE(ProjectSerializer::loadSplit(
        storePath, callerManifest, callerDocuments));
    EXPECT_EQ(callerManifest, originalCallerManifest);
    EXPECT_EQ(callerDocuments, originalCallerDocuments);
}
