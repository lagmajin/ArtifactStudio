#include <gtest/gtest.h>

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QTemporaryDir>

import UI.Layout.State;
import Core.FastSettingsStore;
import Serialization.SchemaMigration;

namespace {

using ArtifactCore::UiLayoutState;
using ArtifactCore::Serialization::SchemaMigrationRegistry;

UiLayoutState sampleState()
{
    UiLayoutState state(QStringLiteral("composition-main"));
    state.geometry = QByteArray("\0geometry\xff", 10);
    state.state = QByteArray("dock-layout-v2");
    state.dockState = QByteArray("ads-state\0with-tail", 19);
    state.version = 2;
    return state;
}

} // namespace

TEST(UiLayoutStateContractTest, JsonRoundTripPreservesOpaqueBinaryPayloads)
{
    const UiLayoutState original = sampleState();
    ASSERT_FALSE(original.isEmpty());

    const QJsonObject serialized = original.toJson();
    EXPECT_EQ(serialized.value(QStringLiteral("layoutKey")).toString(),
              QStringLiteral("composition-main"));
    EXPECT_TRUE(serialized.contains(QStringLiteral("geometry_b64")));
    EXPECT_TRUE(serialized.contains(QStringLiteral("state_b64")));
    EXPECT_TRUE(serialized.contains(QStringLiteral("dockState_b64")));

    const UiLayoutState restored = UiLayoutState::fromJson(serialized);
    EXPECT_EQ(restored.layoutKey, original.layoutKey);
    EXPECT_EQ(restored.geometry, original.geometry);
    EXPECT_EQ(restored.state, original.state);
    EXPECT_EQ(restored.dockState, original.dockState);
    EXPECT_EQ(restored.version, original.version);
}

TEST(UiLayoutStateContractTest, MissingJsonFieldsUseEmptyPayloadsAndCurrentVersion)
{
    const UiLayoutState restored = UiLayoutState::fromJson(
        QJsonObject{{QStringLiteral("layoutKey"), QStringLiteral("empty")}});

    EXPECT_EQ(restored.layoutKey, QStringLiteral("empty"));
    EXPECT_TRUE(restored.geometry.isEmpty());
    EXPECT_TRUE(restored.state.isEmpty());
    EXPECT_TRUE(restored.dockState.isEmpty());
    EXPECT_EQ(restored.version, 2);
    EXPECT_TRUE(restored.isEmpty());
}

TEST(UiLayoutStateContractTest, WrongTypedJsonFieldsUseSafeDefaults)
{
    const UiLayoutState restored = UiLayoutState::fromJson(QJsonObject{
        {QStringLiteral("layoutKey"), 17},
        {QStringLiteral("version"), QStringLiteral("legacy")},
        {QStringLiteral("geometry_b64"), 23},
        {QStringLiteral("state_b64"), true},
        {QStringLiteral("dockState_b64"), QJsonValue(QJsonValue::Null)},
    });

    EXPECT_TRUE(restored.layoutKey.isEmpty());
    EXPECT_EQ(restored.version, 2);
    EXPECT_TRUE(restored.geometry.isEmpty());
    EXPECT_TRUE(restored.state.isEmpty());
    EXPECT_TRUE(restored.dockState.isEmpty());
    EXPECT_TRUE(restored.isEmpty());
}

TEST(UiLayoutStateContractTest, QSettingsRoundTripUsesPrefixAndKeepsOtherLayoutsSeparate)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("layout.ini")),
                       QSettings::IniFormat);
    const UiLayoutState original = sampleState();
    original.saveToSettings(settings, QStringLiteral("ui/layouts/main"));

    UiLayoutState unrelated(QStringLiteral("unrelated"));
    unrelated.geometry = QByteArray("other");
    unrelated.saveToSettings(settings, QStringLiteral("ui/layouts/other"));
    settings.sync();

    const UiLayoutState restored = UiLayoutState::loadFromSettings(
        settings, QStringLiteral("ui/layouts/main"));
    EXPECT_EQ(restored.layoutKey, original.layoutKey);
    EXPECT_EQ(restored.geometry, original.geometry);
    EXPECT_EQ(restored.state, original.state);
    EXPECT_EQ(restored.dockState, original.dockState);
    EXPECT_EQ(restored.version, original.version);

    const UiLayoutState otherRestored = UiLayoutState::loadFromSettings(
        settings, QStringLiteral("ui/layouts/other"));
    EXPECT_EQ(otherRestored.layoutKey, QStringLiteral("unrelated"));
    EXPECT_EQ(otherRestored.geometry, QByteArray("other"));
}

TEST(UiLayoutStateContractTest, FastSettingsStoreRoundTripPersistsAllThreePayloads)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.cbor"));
    const UiLayoutState original = sampleState();
    {
        ArtifactCore::FastSettingsStore store(path);
        original.saveToStore(store, QStringLiteral("layouts/composition"));
        ASSERT_TRUE(store.sync());
    }

    ArtifactCore::FastSettingsStore restoredStore(path);
    const UiLayoutState restored = UiLayoutState::loadFromStore(
        restoredStore, QStringLiteral("layouts/composition"));
    EXPECT_EQ(restored.layoutKey, original.layoutKey);
    EXPECT_EQ(restored.geometry, original.geometry);
    EXPECT_EQ(restored.state, original.state);
    EXPECT_EQ(restored.dockState, original.dockState);
    EXPECT_EQ(restored.version, original.version);
}

TEST(UiLayoutStateContractTest, MissingFastSettingsPrefixUsesEmptyCurrentState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ArtifactCore::FastSettingsStore store(
        directory.filePath(QStringLiteral("settings.cbor")));

    const UiLayoutState restored = UiLayoutState::loadFromStore(
        store, QStringLiteral("layouts/not-saved"));

    EXPECT_TRUE(restored.layoutKey.isEmpty());
    EXPECT_TRUE(restored.geometry.isEmpty());
    EXPECT_TRUE(restored.state.isEmpty());
    EXPECT_TRUE(restored.dockState.isEmpty());
    EXPECT_EQ(restored.version, 2);
    EXPECT_TRUE(restored.isEmpty());
}

TEST(UiLayoutStateContractTest, SerializableDeserializeReplacesWholeState)
{
    UiLayoutState destination(QStringLiteral("old"));
    destination.geometry = QByteArray("old-geometry");
    destination.dockState = QByteArray("old-dock");
    const UiLayoutState source = sampleState();

    EXPECT_TRUE(destination.deserialize(source.serialize()));
    EXPECT_EQ(destination.layoutKey, source.layoutKey);
    EXPECT_EQ(destination.geometry, source.geometry);
    EXPECT_EQ(destination.state, source.state);
    EXPECT_EQ(destination.dockState, source.dockState);
    EXPECT_EQ(destination.version, source.version);
}

TEST(UiLayoutStateContractTest, VersionOneMigrationAddsDockPayloadAndPreservesExistingData)
{
    auto& migrations = SchemaMigrationRegistry::instance();
    QJsonObject legacy{
        {QStringLiteral("layoutKey"), QStringLiteral("legacy-composition")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("geometry_b64"),
         QString::fromLatin1(QByteArray("geometry").toBase64())},
        {QStringLiteral("state_b64"),
         QString::fromLatin1(QByteArray("main-window-state").toBase64())},
        {QStringLiteral("vendorExtension"),
         QJsonObject{{QStringLiteral("dockMode"), QStringLiteral("compact")},
                     {QStringLiteral("restoreToken"), 17}}},
    };

    ASSERT_TRUE(migrations.hasMigrationPath(QStringLiteral("UiLayoutState"), 1, 2));
    ASSERT_TRUE(migrations.migrate(QStringLiteral("UiLayoutState"), 1, 2, legacy));

    EXPECT_EQ(legacy.value(QStringLiteral("version")).toInt(), 2);
    EXPECT_EQ(legacy.value(QStringLiteral("layoutKey")).toString(),
              QStringLiteral("legacy-composition"));
    EXPECT_EQ(legacy.value(QStringLiteral("geometry_b64")).toString(),
              QString::fromLatin1(QByteArray("geometry").toBase64()));
    EXPECT_EQ(legacy.value(QStringLiteral("state_b64")).toString(),
              QString::fromLatin1(QByteArray("main-window-state").toBase64()));
    EXPECT_EQ(legacy.value(QStringLiteral("dockState_b64")).toString(),
              QString());
    EXPECT_EQ(legacy.value(QStringLiteral("vendorExtension")).toObject()
                  .value(QStringLiteral("dockMode")).toString(),
              QStringLiteral("compact"));
    EXPECT_EQ(legacy.value(QStringLiteral("vendorExtension")).toObject()
                  .value(QStringLiteral("restoreToken")).toInt(),
              17);
}
