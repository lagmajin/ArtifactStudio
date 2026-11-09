#include <gtest/gtest.h>

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QString>

import Clipboard.ClipboardManager;

using namespace ArtifactCore;

TEST(ClipboardManagerContractTest,
     SystemClipboardRoundTripPreservesBundleAndKeyframePayloads)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    int argc = 1;
    char applicationName[] = "ClipboardManagerContractTest";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);
    ASSERT_NE(application.clipboard(), nullptr);
    application.clipboard()->clear();

    QJsonArray layers;
    layers.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("layer-01")},
        {QStringLiteral("name"), QStringLiteral("Foreground")},
        {QStringLiteral("opacity"), 0.625},
    });
    layers.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("layer-02")},
        {QStringLiteral("name"), QStringLiteral("Background")},
        {QStringLiteral("opacity"), 1.0},
    });
    const QJsonObject metadata{
        {QStringLiteral("sourceProjectName"), QStringLiteral("Source project")},
        {QStringLiteral("extension"), QJsonObject{
             {QStringLiteral("revision"), 17}}},
    };

    auto& manager = ClipboardManager::instance();
    manager.copyLayerBundle(layers, QStringLiteral("Two layers"), metadata);
    ASSERT_TRUE(manager.hasProjectBundleData());
    ASSERT_TRUE(manager.hasLayerData());
    EXPECT_EQ(manager.pasteLayers(), layers);
    EXPECT_EQ(manager.description(), QStringLiteral("Two layers"));

    manager.syncFromSystemClipboard();

    EXPECT_EQ(manager.currentType(), ClipboardType::ProjectBundle);
    EXPECT_TRUE(manager.hasProjectBundleData());
    EXPECT_TRUE(manager.hasLayerData());
    EXPECT_EQ(manager.pasteLayers(), layers);
    EXPECT_EQ(manager.description(), QStringLiteral("Two layers"));
    const QJsonObject restoredBundle = manager.pasteProjectBundle();
    EXPECT_EQ(restoredBundle.value(QStringLiteral("sourceProjectName")).toString(),
              QStringLiteral("Source project"));
    EXPECT_EQ(restoredBundle.value(QStringLiteral("extension")).toObject()
                  .value(QStringLiteral("revision")).toInt(), 17);

    application.clipboard()->setText(QStringLiteral(
        "{\"artifact_clipboard\":\"layer\",\"layers\":{\"invalid\":true}}"));
    manager.syncFromSystemClipboard();

    EXPECT_TRUE(manager.isEmpty());
    EXPECT_EQ(manager.currentType(), ClipboardType::None);
    EXPECT_FALSE(manager.hasLayerData());
    EXPECT_FALSE(manager.hasProjectBundleData());
    EXPECT_TRUE(manager.pasteLayers().isEmpty());
    EXPECT_TRUE(manager.description().isEmpty());

    const QJsonObject preferredPayload{
        {QStringLiteral("artifact_clipboard"), QStringLiteral("layer")},
        {QStringLiteral("layers"), QJsonArray{
             QJsonObject{{QStringLiteral("id"), QStringLiteral("preferred-mime")}}}},
    };
    const QJsonObject fallbackPayload{
        {QStringLiteral("artifact_clipboard"), QStringLiteral("layer")},
        {QStringLiteral("layers"), QJsonArray{
             QJsonObject{{QStringLiteral("id"), QStringLiteral("text-fallback")}}}},
    };
    auto* conflictingMime = new QMimeData();
    conflictingMime->setData(
        QByteArrayLiteral("application/x-artifact-clipboard+json"),
        QJsonDocument(preferredPayload).toJson(QJsonDocument::Compact));
    conflictingMime->setText(QString::fromUtf8(
        QJsonDocument(fallbackPayload).toJson(QJsonDocument::Compact)));
    application.clipboard()->setMimeData(conflictingMime);
    manager.syncFromSystemClipboard();
    ASSERT_EQ(manager.currentType(), ClipboardType::Layer);
    ASSERT_EQ(manager.pasteLayers().size(), 1);
    EXPECT_EQ(manager.pasteLayers().first().toObject()
                  .value(QStringLiteral("id")).toString(),
              QStringLiteral("preferred-mime"));

    auto* malformedPreferredMime = new QMimeData();
    malformedPreferredMime->setData(
        QByteArrayLiteral("application/x-artifact-clipboard+json"),
        QByteArrayLiteral("{"));
    malformedPreferredMime->setText(QString::fromUtf8(
        QJsonDocument(fallbackPayload).toJson(QJsonDocument::Compact)));
    application.clipboard()->setMimeData(malformedPreferredMime);
    manager.syncFromSystemClipboard();
    EXPECT_TRUE(manager.isEmpty());
    EXPECT_EQ(manager.currentType(), ClipboardType::None);

    const QJsonObject malformedKeyframePayload{
        {QStringLiteral("artifact_clipboard"), QStringLiteral("keyframes")},
        {QStringLiteral("propertyPath"), QStringLiteral("transform.position.x")},
        {QStringLiteral("keyframes"), QJsonArray{
             QJsonObject{{QStringLiteral("frame"), 7},
                         {QStringLiteral("value"), 0.5}},
             QStringLiteral("not-a-keyframe"),
         }},
    };
    application.clipboard()->setText(QString::fromUtf8(
        QJsonDocument(malformedKeyframePayload).toJson(QJsonDocument::Compact)));
    manager.syncFromSystemClipboard();
    EXPECT_TRUE(manager.isEmpty());
    EXPECT_EQ(manager.currentType(), ClipboardType::None);
    EXPECT_FALSE(manager.hasKeyframeData());

    const QJsonArray keyframes{
        QJsonObject{{QStringLiteral("frame"), 17},
                    {QStringLiteral("value"), 0.75}},
        QJsonObject{{QStringLiteral("frame"), -4},
                    {QStringLiteral("value"), 0.125}},
        QJsonObject{{QStringLiteral("frame"), 9},
                    {QStringLiteral("value"), 0.5}},
    };
    manager.copyKeyframes(QStringLiteral("transform.position.x"), keyframes,
                          QStringLiteral("source-layer"));
    manager.syncFromSystemClipboard();

    ASSERT_EQ(manager.currentType(), ClipboardType::Keyframes);
    EXPECT_TRUE(manager.hasKeyframeData());
    EXPECT_FALSE(manager.hasLayerData());
    EXPECT_FALSE(manager.hasProjectBundleData());
    EXPECT_EQ(manager.pasteKeyframes(), keyframes);
    EXPECT_EQ(manager.pasteKeyframesPropertyPath(),
              QStringLiteral("transform.position.x"));

    const QMimeData* clipboardMime = application.clipboard()->mimeData();
    ASSERT_NE(clipboardMime, nullptr);
    const QByteArray serialized = clipboardMime->data(
        QByteArrayLiteral("application/x-artifact-clipboard+json"));
    const QJsonObject envelope = QJsonDocument::fromJson(serialized).object();
    ASSERT_FALSE(envelope.isEmpty());
    EXPECT_EQ(envelope.value(QStringLiteral("artifact_clipboard")).toString(),
              QStringLiteral("keyframes"));
    EXPECT_EQ(envelope.value(QStringLiteral("sourceFrameStart")).toInt(), -4);
    EXPECT_EQ(envelope.value(QStringLiteral("sourceFrameEnd")).toInt(), 17);

    const QJsonArray partiallyMalformedKeyframes{
        QJsonObject{{QStringLiteral("frame"), 9},
                    {QStringLiteral("value"), 0.9}},
        QJsonObject{{QStringLiteral("value"), 0.5}},
        QJsonObject{{QStringLiteral("frame"), QStringLiteral("not-a-frame")},
                    {QStringLiteral("value"), 0.25}},
        QJsonObject{{QStringLiteral("frame"), 3},
                    {QStringLiteral("value"), 0.3}},
    };
    manager.copyKeyframes(QStringLiteral("transform.position.x"),
                          partiallyMalformedKeyframes,
                          QStringLiteral("source-layer"));
    const QJsonObject malformedKeyframeEnvelope = QJsonDocument::fromJson(
        application.clipboard()->mimeData()->data(
            QByteArrayLiteral("application/x-artifact-clipboard+json"))).object();
    EXPECT_EQ(malformedKeyframeEnvelope.value(QStringLiteral("sourceFrameStart")).toInt(), 3);
    EXPECT_EQ(malformedKeyframeEnvelope.value(QStringLiteral("sourceFrameEnd")).toInt(), 9);

    const QJsonArray easing{
        QJsonObject{{QStringLiteral("frame"), 4},
                    {QStringLiteral("interpolation"), QStringLiteral("bezier")},
                    {QStringLiteral("influence"), 0.35}},
        QJsonObject{{QStringLiteral("frame"), 12},
                    {QStringLiteral("interpolation"), QStringLiteral("hold")},
                    {QStringLiteral("influence"), 0.0}},
    };
    manager.copyKeyframeEasing(QStringLiteral("transform.position.x"), easing,
                               QStringLiteral("source-layer"));
    const QByteArray easingMimeType = QByteArrayLiteral(
        "application/x-artifact-keyframe-easing");
    const QByteArray easingPayload = application.clipboard()->mimeData()->data(
        easingMimeType);
    ASSERT_FALSE(easingPayload.isEmpty());

    auto* easingOnlyMime = new QMimeData();
    easingOnlyMime->setData(easingMimeType, easingPayload);
    application.clipboard()->setMimeData(easingOnlyMime);
    manager.syncFromSystemClipboard();

    EXPECT_EQ(manager.currentType(), ClipboardType::KeyframeEasing);
    EXPECT_TRUE(manager.hasKeyframeEasingData());
    EXPECT_EQ(manager.pasteKeyframeEasing(), easing);
    EXPECT_EQ(manager.pasteKeyframeEasingPropertyPath(),
              QStringLiteral("transform.position.x"));
    EXPECT_FALSE(manager.hasKeyframeData());
}
\n
