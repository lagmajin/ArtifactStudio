#include <gtest/gtest.h>

#include <string_view>
#include <string>
#include <utility>

#include <QByteArray>
#include <QCborValue>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>

import Configuration.LayeredConfigStore;
import Configuration.ConfigLayer;
import Configuration.ConfigSchema;
import Core.FastSettingsStore;

using namespace ArtifactCore;

using ArtifactCore::FastSettingsStore;

namespace {
constexpr auto kPriorityKey = "Tests/LayeredConfigStore/Priority";
constexpr auto kSystemDefaultKey = "Tests/LayeredConfigStore/SystemDefault";
constexpr auto kFeatureKey = "Render/Concurrency/Enabled";
constexpr auto kDepthKey = "Render/Concurrency/Depth";
constexpr auto kReloadFailureKey = "Tests/LayeredConfigStore/ReloadFailure";
constexpr auto kMultiReloadUserKey = "Tests/LayeredConfigStore/MultiReloadUser";
constexpr auto kMultiReloadProjectKey = "Tests/LayeredConfigStore/MultiReloadProject";
constexpr auto kPersistentPrecedenceKey = "Tests/LayeredConfigStore/PersistentPrecedence";
constexpr auto kImportOverrideKey = "Tests/LayeredConfigStore/ImportOverride";
constexpr auto kImportAdditionKey = "Tests/LayeredConfigStore/ImportAddition";
constexpr auto kImportLocalOnlyKey = "Tests/LayeredConfigStore/ImportLocalOnly";

class LayeredConfigRestore {
public:
    LayeredConfigRestore(LayeredConfigStore& store, QStringList cleanupKeys)
        : store_(store)
        , previousUserLoaded_(store.isLoaded(ConfigLayer::User))
        , previousUserPath_(store.layerPath(ConfigLayer::User))
        , previousProjectLoaded_(store.isLoaded(ConfigLayer::Project))
        , previousProjectPath_(store.layerPath(ConfigLayer::Project))
        , cleanupKeys_(std::move(cleanupKeys))
    {}

    ~LayeredConfigRestore()
    {
        store_.setValidator({});
        for (const QString& key : cleanupKeys_) {
            const QByteArray utf8Key = key.toUtf8();
            const std::string_view keyView(utf8Key.constData(),
                                           static_cast<size_t>(utf8Key.size()));
            store_.removeValue(ConfigLayer::System, keyView);
            store_.removeValue(ConfigLayer::Session, keyView);
        }
        if (previousUserLoaded_ && !previousUserPath_.isEmpty()) {
            store_.loadLayer(ConfigLayer::User, previousUserPath_);
        } else if (store_.isLoaded(ConfigLayer::User)) {
            store_.unloadLayer(ConfigLayer::User);
        }
        if (previousProjectLoaded_ && !previousProjectPath_.isEmpty()) {
            store_.loadLayer(ConfigLayer::Project, previousProjectPath_);
        } else if (store_.isLoaded(ConfigLayer::Project)) {
            store_.unloadLayer(ConfigLayer::Project);
        }
    }

private:
    LayeredConfigStore& store_;
    bool previousUserLoaded_;
    QString previousUserPath_;
    bool previousProjectLoaded_;
    QString previousProjectPath_;
    QStringList cleanupKeys_;
};
}

TEST(LayeredConfigStoreContractTest,
     LayerPrecedenceSystemImportValidationAndCborTransferComposeCorrectly)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    auto& store = LayeredConfigStore::instance();
    LayeredConfigRestore restore(store, {
        QString::fromLatin1(kPriorityKey),
        QString::fromLatin1(kSystemDefaultKey),
        QString::fromLatin1(kFeatureKey),
        QString::fromLatin1(kDepthKey),
    });
    store.setValidator({});
    ASSERT_TRUE(store.isLoaded(ConfigLayer::System));

    auto& schema = ConfigSchema::instance();
    ConfigProperty prioritySchema;
    prioritySchema.key = kPriorityKey;
    prioritySchema.type = QVariant::Int;
    prioritySchema.defaultValue = QVariant(1);
    prioritySchema.minValue = QVariant(0);
    prioritySchema.maxValue = QVariant(10);
    prioritySchema.allowedValues = {QVariant(1), QVariant(2), QVariant(3),
                                    QVariant(4), QVariant(8)};
    ASSERT_TRUE(schema.registerProperty(prioritySchema));

    ConfigProperty defaultSchema;
    defaultSchema.key = kSystemDefaultKey;
    defaultSchema.type = QVariant::Int;
    defaultSchema.defaultValue = QVariant(17);
    defaultSchema.minValue = QVariant(0);
    defaultSchema.maxValue = QVariant(100);
    ASSERT_TRUE(schema.registerProperty(defaultSchema));
    EXPECT_FALSE(schema.registerProperty(prioritySchema));
    EXPECT_TRUE(schema.validate(kPriorityKey, QVariant(8)).valid);
    EXPECT_FALSE(schema.validate(kPriorityKey, QVariant(-1)).valid);
    EXPECT_FALSE(schema.validate(kPriorityKey, QVariant(11)).valid);
    EXPECT_FALSE(schema.validate(kPriorityKey, QVariant(9)).valid)
        << "an in-range value outside allowedValues must be rejected";
    EXPECT_FALSE(schema.validate(kPriorityKey, QVariant(QStringLiteral("8"))).valid);
    EXPECT_FALSE(schema.validate("Tests/LayeredConfigStore/Unknown", QVariant(1)).valid);
    EXPECT_TRUE(schema.applyDefaultsToLayer(ConfigLayer::System));
    EXPECT_TRUE(schema.applyDefaultsToLayer(ConfigLayer::System));
    EXPECT_EQ(store.value(kPriorityKey).toInt(), 1);
    EXPECT_EQ(store.value(kSystemDefaultKey).toInt(), 17);

    store.setValidator([&schema](std::string_view key, const QVariant& value) {
        return schema.find(key) == nullptr || schema.validate(key, value).valid;
    });

    const QString userPath = directory.filePath(QStringLiteral("user.cbor"));
    const QString projectPath = directory.filePath(QStringLiteral("project.cbor"));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::User, userPath));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::Project, projectPath));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::Session));

    ASSERT_TRUE(store.setValue(ConfigLayer::User, kPriorityKey, QVariant(2)));
    ASSERT_TRUE(store.setValue(ConfigLayer::Project, kPriorityKey, QVariant(3)));
    ASSERT_TRUE(store.setValue(ConfigLayer::Session, kPriorityKey, QVariant(4)));
    EXPECT_EQ(store.value(kPriorityKey).toInt(), 4);
    EXPECT_EQ(store.sourceLayer(kPriorityKey), ConfigLayer::Session);

    ASSERT_TRUE(store.removeValue(ConfigLayer::Session, kPriorityKey));
    EXPECT_EQ(store.value(kPriorityKey).toInt(), 3);
    EXPECT_EQ(store.sourceLayer(kPriorityKey), ConfigLayer::Project);
    ASSERT_TRUE(store.removeValue(ConfigLayer::Project, kPriorityKey));
    EXPECT_EQ(store.value(kPriorityKey).toInt(), 2);
    EXPECT_EQ(store.sourceLayer(kPriorityKey), ConfigLayer::User);
    ASSERT_TRUE(store.removeValue(ConfigLayer::User, kPriorityKey));
    EXPECT_EQ(store.value(kPriorityKey).toInt(), 1);
    EXPECT_EQ(store.sourceLayer(kPriorityKey), ConfigLayer::System);

    EXPECT_TRUE(store.setDefaultValue(kSystemDefaultKey, QVariant(99)));
    EXPECT_EQ(store.value(kSystemDefaultKey).toInt(), 17);

    const QString startupPath = directory.filePath(QStringLiteral("startup.json"));
    QFile startupFile(startupPath);
    ASSERT_TRUE(startupFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QJsonObject startupObject{
        {QStringLiteral("Render"), QJsonObject{
             {QStringLiteral("Concurrency"), QJsonObject{
                  {QStringLiteral("Enabled"), true},
                  {QStringLiteral("Depth"), 6},
                  {QStringLiteral("_comment"), QStringLiteral("ignored")}}}}},
        {QStringLiteral("_notes"), QJsonObject{
             {QStringLiteral("owner"), QStringLiteral("documentation only")}}},
    };
    const QByteArray startupJson = QJsonDocument(startupObject).toJson(QJsonDocument::Compact);
    ASSERT_EQ(startupFile.write(startupJson), startupJson.size());
    ASSERT_TRUE(startupFile.flush());
    startupFile.close();

    EXPECT_EQ(store.importSystemJson(startupPath), 2);
    EXPECT_TRUE(store.valueBool(kFeatureKey));
    EXPECT_EQ(store.valueInt64(kDepthKey), 6);
    EXPECT_FALSE(store.value(QStringLiteral("Render/Concurrency/_comment")).isValid());
    EXPECT_FALSE(store.value(QStringLiteral("_notes/owner")).isValid());
    EXPECT_EQ(store.sourceLayer(kFeatureKey), ConfigLayer::System);

    ASSERT_TRUE(store.setValue(kPriorityKey, QVariant(8)));
    EXPECT_EQ(store.sourceLayer(kPriorityKey), ConfigLayer::Session);
    EXPECT_FALSE(store.setValue(kPriorityKey, QVariant(99)));
    EXPECT_FALSE(store.setValue(kPriorityKey, QVariant(QStringLiteral("wrong type"))));
    EXPECT_EQ(store.value(kPriorityKey).toInt(), 8);

    const QString exportedPath = directory.filePath(QStringLiteral("system-export.cbor"));
    ASSERT_TRUE(store.exportLayer(ConfigLayer::System, exportedPath));
    ASSERT_TRUE(store.importLayer(exportedPath, ConfigLayer::Project));
    EXPECT_EQ(store.sourceLayer(kFeatureKey), ConfigLayer::Project);
    EXPECT_TRUE(store.valueBool(kFeatureKey));
    EXPECT_EQ(store.sourceLayer(kPriorityKey), ConfigLayer::Session);
}

TEST(LayeredConfigStoreContractTest,
     FailedLayerReloadPreservesLoadedPathAndVisibleValue)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    auto& store = LayeredConfigStore::instance();
    LayeredConfigRestore restore(store, {
        QString::fromLatin1(kReloadFailureKey),
    });
    store.setValidator({});
    const QString validPath = directory.filePath(QStringLiteral("user.cbor"));
    const QString malformedPath = directory.filePath(QStringLiteral("not-a-map.cbor"));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::User, validPath));
    ASSERT_TRUE(store.setValue(ConfigLayer::User, kReloadFailureKey,
                               QVariant(QStringLiteral("preserve-me"))));
    ASSERT_EQ(store.value(QString::fromLatin1(kReloadFailureKey)).toString(),
              QStringLiteral("preserve-me"));
    ASSERT_EQ(store.sourceLayer(kReloadFailureKey), ConfigLayer::User);

    QFile malformed(malformedPath);
    ASSERT_TRUE(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray scalarCbor = QCborValue(qint64{42}).toCbor();
    ASSERT_EQ(malformed.write(scalarCbor), scalarCbor.size());
    malformed.close();

    EXPECT_FALSE(store.loadLayer(ConfigLayer::User, malformedPath));
    EXPECT_TRUE(store.isLoaded(ConfigLayer::User));
    EXPECT_EQ(store.layerPath(ConfigLayer::User), validPath);
    EXPECT_EQ(store.value(QString::fromLatin1(kReloadFailureKey)).toString(),
              QStringLiteral("preserve-me"));
    EXPECT_EQ(store.sourceLayer(kReloadFailureKey), ConfigLayer::User);
}

TEST(LayeredConfigStoreContractTest,
     ReloadContinuesAcrossLayersWhenOneLayerHasLocalChanges)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    auto& store = LayeredConfigStore::instance();
    LayeredConfigRestore restore(store, {
        QString::fromLatin1(kMultiReloadUserKey),
        QString::fromLatin1(kMultiReloadProjectKey),
    });
    store.setValidator({});
    const QString userPath = directory.filePath(QStringLiteral("user.cbor"));
    const QString projectPath = directory.filePath(QStringLiteral("project.cbor"));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::User, userPath));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::Project, projectPath));
    ASSERT_TRUE(store.setValue(ConfigLayer::User, kMultiReloadUserKey,
                               QVariant(QStringLiteral("disk-user"))));
    ASSERT_TRUE(store.setValue(ConfigLayer::Project, kMultiReloadProjectKey,
                               QVariant(QStringLiteral("disk-project"))));
    ASSERT_TRUE(store.saveLayer(ConfigLayer::User));
    ASSERT_TRUE(store.saveLayer(ConfigLayer::Project));

    FastSettingsStore externalUser(userPath);
    externalUser.setValue(QString::fromLatin1(kMultiReloadUserKey),
                          QStringLiteral("external-user"));
    ASSERT_TRUE(externalUser.sync());
    FastSettingsStore externalProject(projectPath);
    externalProject.setValue(QString::fromLatin1(kMultiReloadProjectKey),
                             QStringLiteral("external-project"));
    ASSERT_TRUE(externalProject.sync());

    const QDateTime externalTimestamp = QDateTime::currentDateTime().addSecs(5);
    for (const QString& path : {userPath, projectPath}) {
        QFile changedFile(path);
        ASSERT_TRUE(changedFile.open(QIODevice::ReadOnly));
        ASSERT_TRUE(changedFile.setFileTime(
            externalTimestamp, QFileDevice::FileModificationTime));
    }
    ASSERT_TRUE(store.setValue(ConfigLayer::User, kMultiReloadUserKey,
                               QVariant(QStringLiteral("local-unsaved-user"))));

    EXPECT_FALSE(store.reloadChangedLayers());
    EXPECT_EQ(store.value(kMultiReloadUserKey).toString(),
              QStringLiteral("local-unsaved-user"));
    EXPECT_EQ(store.sourceLayer(kMultiReloadUserKey), ConfigLayer::User);
    EXPECT_EQ(store.value(kMultiReloadProjectKey).toString(),
              QStringLiteral("external-project"));
    EXPECT_EQ(store.sourceLayer(kMultiReloadProjectKey), ConfigLayer::Project);

    ASSERT_TRUE(store.reloadChangedLayers(true));
    EXPECT_EQ(store.value(kMultiReloadUserKey).toString(),
              QStringLiteral("external-user"));
    EXPECT_EQ(store.value(kMultiReloadProjectKey).toString(),
              QStringLiteral("external-project"));
}

TEST(LayeredConfigStoreContractTest,
     PersistentLayerReloadAndSessionRemovalRestoreCorrectPrecedence)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    auto& store = LayeredConfigStore::instance();
    LayeredConfigRestore restore(store, {
        QString::fromLatin1(kPersistentPrecedenceKey),
    });
    store.setValidator({});
    ASSERT_TRUE(store.loadLayer(ConfigLayer::User,
        directory.filePath(QStringLiteral("user.cbor"))));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::Project,
        directory.filePath(QStringLiteral("project.cbor"))));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::Session));

    ASSERT_TRUE(store.setValue(ConfigLayer::User, kPersistentPrecedenceKey,
                               QVariant(11)));
    ASSERT_TRUE(store.setValue(ConfigLayer::Project, kPersistentPrecedenceKey,
                               QVariant(22)));
    ASSERT_TRUE(store.setValue(ConfigLayer::Session, kPersistentPrecedenceKey,
                               QVariant(33)));
    ASSERT_TRUE(store.saveLayer(ConfigLayer::User));
    ASSERT_TRUE(store.saveLayer(ConfigLayer::Project));
    EXPECT_EQ(store.value(kPersistentPrecedenceKey).toInt(), 33);
    EXPECT_EQ(store.sourceLayer(kPersistentPrecedenceKey), ConfigLayer::Session);

    ASSERT_TRUE(store.removeValue(ConfigLayer::Session, kPersistentPrecedenceKey));
    EXPECT_EQ(store.value(kPersistentPrecedenceKey).toInt(), 22);
    EXPECT_EQ(store.sourceLayer(kPersistentPrecedenceKey), ConfigLayer::Project);

    ASSERT_TRUE(store.unloadLayer(ConfigLayer::Project));
    EXPECT_FALSE(store.isLoaded(ConfigLayer::Project));
    EXPECT_EQ(store.value(kPersistentPrecedenceKey).toInt(), 11);
    EXPECT_EQ(store.sourceLayer(kPersistentPrecedenceKey), ConfigLayer::User);

    ASSERT_TRUE(store.loadLayer(ConfigLayer::Project,
        directory.filePath(QStringLiteral("project.cbor"))));
    EXPECT_EQ(store.value(kPersistentPrecedenceKey).toInt(), 22);
    EXPECT_EQ(store.sourceLayer(kPersistentPrecedenceKey), ConfigLayer::Project);

    ASSERT_TRUE(store.removeValue(ConfigLayer::Project, kPersistentPrecedenceKey));
    EXPECT_EQ(store.value(kPersistentPrecedenceKey).toInt(), 11);
    EXPECT_EQ(store.sourceLayer(kPersistentPrecedenceKey), ConfigLayer::User);
}

TEST(LayeredConfigStoreContractTest,
     ImportLayerRejectsMalformedInputAtomicallyThenMergesValidKeys)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    auto& store = LayeredConfigStore::instance();
    LayeredConfigRestore restore(store, {
        QString::fromLatin1(kImportOverrideKey),
        QString::fromLatin1(kImportAdditionKey),
        QString::fromLatin1(kImportLocalOnlyKey),
    });
    store.setValidator({});
    const QString projectPath = directory.filePath(QStringLiteral("project.cbor"));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::Project, projectPath));
    ASSERT_TRUE(store.setValue(ConfigLayer::Project, kImportOverrideKey, QVariant(10)));
    ASSERT_TRUE(store.setValue(ConfigLayer::Project, kImportLocalOnlyKey, QVariant(99)));
    ASSERT_TRUE(store.saveLayer(ConfigLayer::Project));

    const QString malformedPath = directory.filePath(QStringLiteral("malformed.cbor"));
    QFile malformed(malformedPath);
    ASSERT_TRUE(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray scalarCbor = QCborValue(QStringLiteral("not-a-settings-map")).toCbor();
    ASSERT_EQ(malformed.write(scalarCbor), scalarCbor.size());
    malformed.close();

    EXPECT_FALSE(store.importLayer(malformedPath, ConfigLayer::Project));
    EXPECT_EQ(store.value(kImportOverrideKey).toInt(), 10);
    EXPECT_EQ(store.value(kImportLocalOnlyKey).toInt(), 99);
    EXPECT_FALSE(store.value(kImportAdditionKey).isValid());

    const QString importPath = directory.filePath(QStringLiteral("incoming.cbor"));
    FastSettingsStore incoming(importPath);
    incoming.setValue(QString::fromLatin1(kImportOverrideKey), 20);
    incoming.setValue(QString::fromLatin1(kImportAdditionKey), 30);
    ASSERT_TRUE(incoming.sync());

    ASSERT_TRUE(store.importLayer(importPath, ConfigLayer::Project));
    EXPECT_EQ(store.value(kImportOverrideKey).toInt(), 20);
    EXPECT_EQ(store.value(kImportAdditionKey).toInt(), 30);
    EXPECT_EQ(store.value(kImportLocalOnlyKey).toInt(), 99);
    EXPECT_EQ(store.sourceLayer(kImportOverrideKey), ConfigLayer::Project);
    ASSERT_TRUE(store.saveLayer(ConfigLayer::Project));
    FastSettingsStore persisted(projectPath);
    EXPECT_EQ(persisted.valueInt64(QString::fromLatin1(kImportOverrideKey)), 20);
    EXPECT_EQ(persisted.valueInt64(QString::fromLatin1(kImportAdditionKey)), 30);
    EXPECT_EQ(persisted.valueInt64(QString::fromLatin1(kImportLocalOnlyKey)), 99);
}

TEST(LayeredConfigStoreContractTest,
     ChangedLayerRejectsDirtyReloadThenDiscardsOnlyOnExplicitRetry)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    auto& store = LayeredConfigStore::instance();
    LayeredConfigRestore restore(store, {
        QString::fromLatin1(kReloadFailureKey),
    });
    store.setValidator({});
    const QString path = directory.filePath(QStringLiteral("user.cbor"));
    ASSERT_TRUE(store.loadLayer(ConfigLayer::User, path));
    ASSERT_TRUE(store.setValue(ConfigLayer::User, kReloadFailureKey,
                               QVariant(QStringLiteral("initial-disk"))));
    ASSERT_TRUE(store.saveLayer(ConfigLayer::User));

    ASSERT_TRUE(store.setValue(ConfigLayer::User, kReloadFailureKey,
                               QVariant(QStringLiteral("local-unsaved"))));
    FastSettingsStore externalWriter(path);
    externalWriter.setValue(QString::fromLatin1(kReloadFailureKey),
                            QStringLiteral("external-update"));
    ASSERT_TRUE(externalWriter.sync());
    QFile updatedFile(path);
    ASSERT_TRUE(updatedFile.open(QIODevice::ReadOnly));
    ASSERT_TRUE(updatedFile.setFileTime(
        QDateTime::currentDateTime().addSecs(2),
        QFileDevice::FileModificationTime));
    updatedFile.close();

    EXPECT_FALSE(store.reloadChangedLayers());
    EXPECT_EQ(store.value(QString::fromLatin1(kReloadFailureKey)).toString(),
              QStringLiteral("local-unsaved"));
    EXPECT_EQ(store.sourceLayer(kReloadFailureKey), ConfigLayer::User);

    ASSERT_TRUE(store.reloadChangedLayers(true));
    EXPECT_EQ(store.value(QString::fromLatin1(kReloadFailureKey)).toString(),
              QStringLiteral("external-update"));
    EXPECT_EQ(store.sourceLayer(kReloadFailureKey), ConfigLayer::User);
}
