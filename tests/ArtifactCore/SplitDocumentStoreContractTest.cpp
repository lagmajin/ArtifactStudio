#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QTemporaryDir>
#include <QString>

#include <array>

import Serialization.SplitDocumentStore;
import Serialization.Document;

using namespace ArtifactCore::Serialization;

namespace {
QJsonObject manifest(const QString& title)
{
    QJsonObject result;
    result.insert(QStringLiteral("schemaVersion"), 3);
    result.insert(QStringLiteral("rootDocument"), QStringLiteral("scene"));
    result.insert(QStringLiteral("title"), title);
    return result;
}

QMap<QString, QJsonObject> childDocuments(const QString& sceneId)
{
    QJsonObject scene;
    scene.insert(QStringLiteral("id"), sceneId);
    scene.insert(QStringLiteral("layers"), QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("layer-a")},
                    {QStringLiteral("opacity"), 0.625}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("layer-b")},
                    {QStringLiteral("opacity"), 1.0}},
    });

    QJsonObject timeline;
    timeline.insert(QStringLiteral("frameRate"), 24);
    timeline.insert(QStringLiteral("markers"), QJsonArray{
        QJsonObject{{QStringLiteral("name"), QStringLiteral("cut")},
                    {QStringLiteral("frame"), 48}},
    });

    return {
        {QStringLiteral("scene"), scene},
        {QStringLiteral("timeline"), timeline},
    };
}
}

TEST(SplitDocumentStoreContractTest, RoundTripsNestedChildDocumentsInJsonAndCbor)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};
    for (const auto format : formats) {
        const QString storePath = QDir::cleanPath(
            temporaryDirectory.filePath(format == SerializationFormat::Json
                                             ? QStringLiteral("json-store")
                                             : QStringLiteral("cbor-store")));
        const auto originalManifest = manifest(QStringLiteral("nested-contract"));
        const auto originalChildren = childDocuments(QStringLiteral("scene-42"));

        ASSERT_TRUE(SplitDocumentStore::save(storePath, originalManifest, originalChildren, format));

        QJsonObject loadedManifest;
        QMap<QString, QJsonObject> loadedChildren;
        ASSERT_TRUE(SplitDocumentStore::load(storePath, loadedManifest, loadedChildren));
        EXPECT_EQ(loadedManifest.value(QStringLiteral("schemaVersion")).toInt(), 3);
        EXPECT_EQ(loadedManifest.value(QStringLiteral("rootDocument")).toString(), "scene");
        EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "nested-contract");
        EXPECT_EQ(loadedManifest.value(QStringLiteral("_documents")).toArray().size(), 2);
        EXPECT_EQ(loadedChildren, originalChildren);
        EXPECT_EQ(SerializationDocument::detectFile(
                      QDir(storePath).filePath(format == SerializationFormat::Json
                                                   ? QStringLiteral("manifest.json")
                                                   : QStringLiteral("manifest.cbor"))),
                  format);
    }
}

TEST(SplitDocumentStoreContractTest,
     ResaveDoesNotReloadChildrenRemovedFromTheNewManifest)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};
    for (const auto format : formats) {
        SCOPED_TRACE(format == SerializationFormat::Json ? "JSON" : "CBOR");
        const QString storePath = temporaryDirectory.filePath(
            format == SerializationFormat::Json ? QStringLiteral("json-store")
                                                : QStringLiteral("cbor-store"));
        const auto retainedChild =
            QJsonObject{{QStringLiteral("id"), QStringLiteral("current-scene")}};
        const auto removedChild =
            QJsonObject{{QStringLiteral("frameRate"), 24},
                        {QStringLiteral("revision"), QStringLiteral("removed")}};
        const QMap<QString, QJsonObject> initialChildren{
            {QStringLiteral("scene"), retainedChild},
            {QStringLiteral("timeline"), removedChild},
        };
        ASSERT_TRUE(SplitDocumentStore::save(
            storePath, manifest(QStringLiteral("initial")), initialChildren, format));

        ASSERT_TRUE(SplitDocumentStore::save(
            storePath, manifest(QStringLiteral("resaved")),
            QMap<QString, QJsonObject>{{QStringLiteral("scene"), retainedChild}},
            format));

        QJsonObject loadedManifest;
        QMap<QString, QJsonObject> loadedChildren;
        ASSERT_TRUE(SplitDocumentStore::load(
            storePath, loadedManifest, loadedChildren));
        EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "resaved");
        ASSERT_EQ(loadedManifest.value(QStringLiteral("_documents")).toArray().size(), 1);
        EXPECT_EQ(loadedManifest.value(QStringLiteral("_documents")).toArray().front().toString(),
                  QStringLiteral("scene"));
        ASSERT_EQ(loadedChildren.size(), 1);
        EXPECT_EQ(loadedChildren.value(QStringLiteral("scene")), retainedChild);
        EXPECT_FALSE(loadedChildren.contains(QStringLiteral("timeline")));
    }
}

TEST(SplitDocumentStoreContractTest,
     RejectsChildDocumentsEncodedInAFormatDifferentFromTheirManifest)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    for (const auto manifestFormat :
         {SerializationFormat::Json, SerializationFormat::Cbor}) {
        SCOPED_TRACE(manifestFormat == SerializationFormat::Json ? "JSON manifest"
                                                                 : "CBOR manifest");
        const SerializationFormat childFormat =
            manifestFormat == SerializationFormat::Json
                ? SerializationFormat::Cbor
                : SerializationFormat::Json;
        const QString storePath = temporaryDirectory.filePath(
            manifestFormat == SerializationFormat::Json
                ? QStringLiteral("json-manifest-mixed-child")
                : QStringLiteral("cbor-manifest-mixed-child"));
        const auto storedManifest = manifest(QStringLiteral("format-coherence"));
        const auto storedChildren = childDocuments(QStringLiteral("scene-format"));
        ASSERT_TRUE(SplitDocumentStore::save(
            storePath, storedManifest, storedChildren, manifestFormat));

        const QString childPath = QDir(storePath).filePath(
            QStringLiteral("documents/timeline") +
            (manifestFormat == SerializationFormat::Json
                 ? QStringLiteral(".json")
                 : QStringLiteral(".cbor")));
        ASSERT_TRUE(SerializationDocument::writeFile(
            childPath, storedChildren.value(QStringLiteral("timeline")), childFormat));

        QJsonObject loadedManifest;
        QMap<QString, QJsonObject> loadedChildren;
        EXPECT_FALSE(SplitDocumentStore::load(
            storePath, loadedManifest, loadedChildren));
    }
}

TEST(SplitDocumentStoreContractTest, FailedRootReferenceSaveKeepsLastGoodStoreLoadable)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    const auto originalManifest = manifest(QStringLiteral("last-good"));
    const auto originalChildren = childDocuments(QStringLiteral("scene-original"));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, originalManifest, originalChildren, SerializationFormat::Json));

    QJsonObject invalidManifest;
    invalidManifest.insert(QStringLiteral("rootDocument"), QStringLiteral("missing-scene"));
    const QMap<QString, QJsonObject> replacementChildren{
        {QStringLiteral("scene"), QJsonObject{{QStringLiteral("id"), QStringLiteral("replacement")}}},
    };
    EXPECT_FALSE(SplitDocumentStore::save(
        storePath, invalidManifest, replacementChildren, SerializationFormat::Cbor));

    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedChildren;
    ASSERT_TRUE(SplitDocumentStore::load(storePath, loadedManifest, loadedChildren));
    EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "last-good");
    EXPECT_EQ(loadedChildren, originalChildren);
    EXPECT_FALSE(QFileInfo::exists(QDir(storePath).filePath(QStringLiteral("manifest.cbor"))));
}

TEST(SplitDocumentStoreContractTest, ResavePrefersNewCborManifestWhenOldJsonManifestRemains)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("old-json")),
        childDocuments(QStringLiteral("scene-old")), SerializationFormat::Json));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("new-cbor")),
        childDocuments(QStringLiteral("scene-new")), SerializationFormat::Cbor));

    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedChildren;
    ASSERT_TRUE(SplitDocumentStore::load(storePath, loadedManifest, loadedChildren));
    EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "new-cbor");
    EXPECT_EQ(loadedChildren.value(QStringLiteral("scene")).value(QStringLiteral("id")).toString(),
              "scene-new");
}

TEST(SplitDocumentStoreContractTest, ResaveAsJsonDoesNotLoadStaleCborManifest)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("old-cbor")),
        childDocuments(QStringLiteral("scene-old")), SerializationFormat::Cbor));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("new-json")),
        childDocuments(QStringLiteral("scene-new")), SerializationFormat::Json));

    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedChildren;
    ASSERT_TRUE(SplitDocumentStore::load(storePath, loadedManifest, loadedChildren));
    EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "new-json");
    EXPECT_EQ(loadedChildren.value(QStringLiteral("scene")).value(QStringLiteral("id")).toString(),
              "scene-new");
}

TEST(SplitDocumentStoreContractTest, RejectsTraversalDocumentNamesWithoutReplacingGoodManifest)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    const auto originalManifest = manifest(QStringLiteral("safe-store"));
    const auto originalChildren = childDocuments(QStringLiteral("scene-safe"));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, originalManifest, originalChildren, SerializationFormat::Json));

    const std::array unsafeNames{
        QStringLiteral("../escape"), QStringLiteral("nested/escape"),
        QStringLiteral("nested\\escape"), QStringLiteral("sub..name"),
    };
    for (const auto& unsafeName : unsafeNames) {
        const QMap<QString, QJsonObject> maliciousChildren{
            {unsafeName, QJsonObject{{QStringLiteral("payload"), QStringLiteral("outside")}}},
        };
        EXPECT_FALSE(SplitDocumentStore::save(
            storePath, QJsonObject{{QStringLiteral("title"), QStringLiteral("overwrite")}},
            maliciousChildren, SerializationFormat::Json)) << unsafeName.toStdString();

        QJsonObject loadedManifest;
        QMap<QString, QJsonObject> loadedChildren;
        ASSERT_TRUE(SplitDocumentStore::load(storePath, loadedManifest, loadedChildren));
        EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "safe-store");
        EXPECT_EQ(loadedChildren, originalChildren);
    }
    EXPECT_FALSE(QFileInfo::exists(temporaryDirectory.filePath(QStringLiteral("escape.json"))));
}

TEST(SplitDocumentStoreContractTest,
     SaveDoesNotFollowDocumentsSymlinkOutsideTheStore)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    const QString outsidePath = temporaryDirectory.filePath(QStringLiteral("outside"));
    ASSERT_TRUE(QDir().mkpath(storePath));
    ASSERT_TRUE(QDir().mkpath(outsidePath));

    const QString documentsLink = QDir(storePath).filePath(QStringLiteral("documents"));
    if (!QFile::link(outsidePath, documentsLink) ||
        !QFileInfo(documentsLink).isSymLink()) {
        GTEST_SKIP() << "directory symlink creation is unavailable on this platform";
    }

    const QMap<QString, QJsonObject> children{
        {QStringLiteral("scene"), QJsonObject{{QStringLiteral("id"), QStringLiteral("outside-write")}}},
    };
    const bool saved = SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("symlink-boundary")), children,
        SerializationFormat::Json);

    EXPECT_FALSE(saved)
        << "a child directory symlink must not redirect writes outside the store";
    EXPECT_FALSE(QFileInfo::exists(
        QDir(outsidePath).filePath(QStringLiteral("scene.json"))));
    EXPECT_FALSE(QFileInfo::exists(
        QDir(storePath).filePath(QStringLiteral("manifest.json"))));
}

TEST(SplitDocumentStoreContractTest,
     LoadDoesNotFollowDocumentsSymlinkOutsideTheStore)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    const QString outsidePath = temporaryDirectory.filePath(QStringLiteral("outside"));
    ASSERT_TRUE(QDir().mkpath(storePath));
    ASSERT_TRUE(QDir().mkpath(outsidePath));

    const QString documentsLink = QDir(storePath).filePath(QStringLiteral("documents"));
    if (!QFile::link(outsidePath, documentsLink) ||
        !QFileInfo(documentsLink).isSymLink()) {
        GTEST_SKIP() << "directory symlink creation is unavailable on this platform";
    }

    QJsonObject storeManifest = manifest(QStringLiteral("symlink-boundary-load"));
    storeManifest.insert(QStringLiteral("_documents"), QJsonArray{QStringLiteral("scene")});
    ASSERT_TRUE(SerializationDocument::writeFile(
        QDir(storePath).filePath(QStringLiteral("manifest.json")), storeManifest,
        SerializationFormat::Json));
    ASSERT_TRUE(SerializationDocument::writeFile(
        QDir(outsidePath).filePath(QStringLiteral("scene.json")),
        QJsonObject{{QStringLiteral("id"), QStringLiteral("outside-child")}},
        SerializationFormat::Json));

    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedChildren;
    EXPECT_FALSE(SplitDocumentStore::load(
        storePath, loadedManifest, loadedChildren))
        << "a child reached through a directory symlink must not be imported";
}

TEST(SplitDocumentStoreContractTest,
     RejectsDocumentNamesThatCollideOnCaseInsensitiveFilesystems)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString probePath =
        temporaryDirectory.filePath(QStringLiteral("CaseSensitivityProbe"));
    QFile probeFile(probePath);
    ASSERT_TRUE(probeFile.open(QIODevice::WriteOnly));
    probeFile.close();
    const bool caseInsensitive = QFileInfo::exists(
        temporaryDirectory.filePath(QStringLiteral("casesensitivityprobe")));

    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    const QMap<QString, QJsonObject> documents{
        {QStringLiteral("Scene"), QJsonObject{{QStringLiteral("id"), QStringLiteral("upper")}}},
        {QStringLiteral("scene"), QJsonObject{{QStringLiteral("id"), QStringLiteral("lower")}}},
    };
    const bool saved = SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("case-collision")), documents,
        SerializationFormat::Json);

    if (caseInsensitive) {
        EXPECT_FALSE(saved)
            << "distinct logical document names must not alias the same file path";
        return;
    }

    ASSERT_TRUE(saved);
    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedDocuments;
    ASSERT_TRUE(SplitDocumentStore::load(
        storePath, loadedManifest, loadedDocuments));
    EXPECT_EQ(loadedDocuments, documents);
}

TEST(SplitDocumentStoreContractTest,
     LoadRejectsCaseAliasesOnlyWhenTheyShareAChildFile)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString probePath =
        temporaryDirectory.filePath(QStringLiteral("LoadCaseProbe"));
    QFile probeFile(probePath);
    ASSERT_TRUE(probeFile.open(QIODevice::WriteOnly));
    probeFile.close();
    const bool caseInsensitive = QFileInfo::exists(
        temporaryDirectory.filePath(QStringLiteral("loadcaseprobe")));

    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};
    for (const auto format : formats) {
        SCOPED_TRACE(format == SerializationFormat::Json ? "JSON" : "CBOR");
        const QString storePath = temporaryDirectory.filePath(
            format == SerializationFormat::Json ? QStringLiteral("json-case-load")
                                                : QStringLiteral("cbor-case-load"));
        QJsonObject rootManifest{
            {QStringLiteral("schemaVersion"), 3},
            {QStringLiteral("rootDocument"), QStringLiteral("Scene")},
            {QStringLiteral("title"), QStringLiteral("case-alias-load")},
        };
        const QMap<QString, QJsonObject> firstDocument{
            {QStringLiteral("Scene"),
             QJsonObject{{QStringLiteral("id"), QStringLiteral("upper")}}},
        };
        ASSERT_TRUE(SplitDocumentStore::save(
            storePath, rootManifest, firstDocument, format));

        if (!caseInsensitive) {
            const QString childPath = QDir(storePath).filePath(
                QStringLiteral("documents/scene") +
                (format == SerializationFormat::Json ? QStringLiteral(".json")
                                                     : QStringLiteral(".cbor")));
            ASSERT_TRUE(SerializationDocument::writeFile(
                childPath, QJsonObject{{QStringLiteral("id"),
                                        QStringLiteral("lower")}}, format));
        }

        const QString manifestPath = QDir(storePath).filePath(
            format == SerializationFormat::Json ? QStringLiteral("manifest.json")
                                                 : QStringLiteral("manifest.cbor"));
        ASSERT_TRUE(SerializationDocument::readFile(
            manifestPath, rootManifest, format));
        rootManifest.insert(QStringLiteral("_documents"),
                            QJsonArray{QStringLiteral("Scene"),
                                       QStringLiteral("scene")});
        ASSERT_TRUE(SerializationDocument::writeFile(
            manifestPath, rootManifest, format));

        QJsonObject loadedManifest;
        QMap<QString, QJsonObject> loadedDocuments;
        const bool loaded = SplitDocumentStore::load(
            storePath, loadedManifest, loadedDocuments);
        if (caseInsensitive) {
            EXPECT_FALSE(loaded)
                << "distinct manifest names must not resolve to one physical child";
        } else {
            ASSERT_TRUE(loaded);
            ASSERT_EQ(loadedDocuments.size(), 2);
            EXPECT_EQ(loadedDocuments.value(QStringLiteral("Scene"))
                          .value(QStringLiteral("id")).toString(), "upper");
            EXPECT_EQ(loadedDocuments.value(QStringLiteral("scene"))
                          .value(QStringLiteral("id")).toString(), "lower");
        }
    }
}

TEST(SplitDocumentStoreContractTest, FailedLateUnsafeChildDoesNotPartiallyReplaceExistingStore)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    const auto originalManifest = manifest(QStringLiteral("last-good"));
    const auto originalChildren = childDocuments(QStringLiteral("scene-original"));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, originalManifest, originalChildren, SerializationFormat::Json));

    const QMap<QString, QJsonObject> replacementChildren{
        {QStringLiteral("scene"), QJsonObject{{QStringLiteral("id"), QStringLiteral("replacement")}}},
        {QStringLiteral("zz..unsafe"), QJsonObject{{QStringLiteral("payload"), QStringLiteral("reject")}}},
    };
    EXPECT_FALSE(SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("replacement")), replacementChildren,
        SerializationFormat::Json));

    QJsonObject loadedManifest;
    QMap<QString, QJsonObject> loadedChildren;
    ASSERT_TRUE(SplitDocumentStore::load(storePath, loadedManifest, loadedChildren));
    EXPECT_EQ(loadedManifest.value(QStringLiteral("title")).toString(), "last-good");
    EXPECT_EQ(loadedChildren, originalChildren);
}

TEST(SplitDocumentStoreContractTest,
     FailedChildLoadPreservesCallerOutputsForJsonAndCbor)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};
    for (const auto format : formats) {
        SCOPED_TRACE(format == SerializationFormat::Json ? "JSON" : "CBOR");
        const QString storePath = temporaryDirectory.filePath(
            format == SerializationFormat::Json ? QStringLiteral("json-store")
                                                : QStringLiteral("cbor-store"));
        const auto storedManifest = manifest(QStringLiteral("partial-load"));
        const auto storedChildren = childDocuments(QStringLiteral("scene-partial"));
        ASSERT_TRUE(SplitDocumentStore::save(
            storePath, storedManifest, storedChildren, format));

        const QString suffix = format == SerializationFormat::Json
            ? QStringLiteral(".json") : QStringLiteral(".cbor");
        const QString missingChildPath = QDir(storePath).filePath(
            QStringLiteral("documents/timeline") + suffix);
        ASSERT_TRUE(QFile::exists(missingChildPath));
        ASSERT_TRUE(QFile::remove(missingChildPath));

        QJsonObject outputManifest{{QStringLiteral("caller"), QStringLiteral("preserve")}};
        const QJsonObject originalManifest = outputManifest;
        QMap<QString, QJsonObject> outputChildren{
            {QStringLiteral("caller-child"),
             QJsonObject{{QStringLiteral("value"), QStringLiteral("preserve")}}},
        };
        const QMap<QString, QJsonObject> originalChildren = outputChildren;

        EXPECT_FALSE(SplitDocumentStore::load(
            storePath, outputManifest, outputChildren));
        EXPECT_EQ(outputManifest, originalManifest);
        EXPECT_EQ(outputChildren, originalChildren);
    }
}

TEST(SplitDocumentStoreContractTest,
     RejectsDuplicateChildReferencesInJsonAndCborManifests)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const std::array formats{SerializationFormat::Json, SerializationFormat::Cbor};
    for (const auto format : formats) {
        const QString storePath = temporaryDirectory.filePath(
            format == SerializationFormat::Json ? QStringLiteral("json-store")
                                                : QStringLiteral("cbor-store"));
        ASSERT_TRUE(SplitDocumentStore::save(
            storePath, manifest(QStringLiteral("duplicate-reference")),
            childDocuments(QStringLiteral("scene-duplicate")), format));

        const QString manifestPath = QDir(storePath).filePath(
            format == SerializationFormat::Json ? QStringLiteral("manifest.json")
                                                : QStringLiteral("manifest.cbor"));
        QJsonObject editedManifest;
        ASSERT_TRUE(SerializationDocument::readFile(manifestPath, editedManifest, format));
        editedManifest.insert(QStringLiteral("_documents"),
                              QJsonArray{QStringLiteral("scene"),
                                         QStringLiteral("scene")});
        ASSERT_TRUE(SerializationDocument::writeFile(
            manifestPath, editedManifest, format));

        QJsonObject loadedManifest;
        QMap<QString, QJsonObject> loadedChildren;
        EXPECT_FALSE(SplitDocumentStore::load(
            storePath, loadedManifest, loadedChildren))
            << (format == SerializationFormat::Json ? "JSON" : "CBOR");
    }
}

TEST(SplitDocumentStoreContractTest, RejectsInvalidRootDocumentReferencesOnLoad)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString storePath = temporaryDirectory.filePath(QStringLiteral("store"));
    ASSERT_TRUE(SplitDocumentStore::save(
        storePath, manifest(QStringLiteral("root-reference")), childDocuments("scene"),
        SerializationFormat::Json));

    const QString manifestPath = QDir(storePath).filePath(QStringLiteral("manifest.json"));
    const QJsonValue invalidReferences[] = {
        QJsonValue(QStringLiteral("missing-root")),
        QJsonValue(QStringLiteral("../outside")),
        QJsonValue(17),
    };
    for (const QJsonValue& invalidReference : invalidReferences) {
        QJsonObject editedManifest;
        ASSERT_TRUE(SerializationDocument::readFile(
            manifestPath, editedManifest, SerializationFormat::Json));
        editedManifest.insert(QStringLiteral("rootDocument"), invalidReference);
        ASSERT_TRUE(SerializationDocument::writeFile(
            manifestPath, editedManifest, SerializationFormat::Json));

        QJsonObject outputManifest{{QStringLiteral("caller"), QStringLiteral("preserve")}};
        QMap<QString, QJsonObject> outputChildren{
            {QStringLiteral("caller-child"), QJsonObject{{QStringLiteral("value"), 9}}},
        };
        EXPECT_FALSE(SplitDocumentStore::load(storePath, outputManifest, outputChildren));

        // Restore a valid manifest so the next case starts from the same store.
        editedManifest.insert(QStringLiteral("rootDocument"), QStringLiteral("scene"));
        ASSERT_TRUE(SerializationDocument::writeFile(
            manifestPath, editedManifest, SerializationFormat::Json));
    }
}
