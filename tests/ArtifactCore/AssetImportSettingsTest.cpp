#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <QSize>

import Asset.Import.Setting;

using namespace ArtifactCore;

TEST(AssetImportSettingsTest, DefaultsAreStable)
{
    const AssetImportSettings settings;

    EXPECT_EQ(settings.proxyResolution, QSize(1920, 1080));
    EXPECT_EQ(settings.jpegQuality, 85);
    EXPECT_FALSE(settings.generateProxyOnImport);
    EXPECT_TRUE(settings.normalizeColorSpace);
    EXPECT_EQ(settings.workingColorSpace, QStringLiteral("Linear"));
}

TEST(AssetImportSettingsTest, JsonRoundTripPreservesEverySetting)
{
    AssetImportSettings settings;
    settings.proxyResolution = QSize(1280, 720);
    settings.jpegQuality = 93;
    settings.generateProxyOnImport = true;
    settings.normalizeColorSpace = false;
    settings.workingColorSpace = QStringLiteral("Display P3");

    const AssetImportSettings restored = AssetImportSettings::fromJson(settings.toJson());

    EXPECT_EQ(restored.proxyResolution, settings.proxyResolution);
    EXPECT_EQ(restored.jpegQuality, settings.jpegQuality);
    EXPECT_EQ(restored.generateProxyOnImport, settings.generateProxyOnImport);
    EXPECT_EQ(restored.normalizeColorSpace, settings.normalizeColorSpace);
    EXPECT_EQ(restored.workingColorSpace, settings.workingColorSpace);
}

TEST(AssetImportSettingsTest, JsonClampsResolutionAndJpegQualityBounds)
{
    const QJsonObject belowMinimum{
        {QStringLiteral("proxyWidth"), 0},
        {QStringLiteral("proxyHeight"), -12},
        {QStringLiteral("jpegQuality"), -1},
    };
    const AssetImportSettings low = AssetImportSettings::fromJson(belowMinimum);
    EXPECT_EQ(low.proxyResolution, QSize(1, 1));
    EXPECT_EQ(low.jpegQuality, 1);

    const QJsonObject aboveMaximum{
        {QStringLiteral("proxyWidth"), 3840},
        {QStringLiteral("proxyHeight"), 2160},
        {QStringLiteral("jpegQuality"), 120},
    };
    const AssetImportSettings high = AssetImportSettings::fromJson(aboveMaximum);
    EXPECT_EQ(high.proxyResolution, QSize(3840, 2160));
    EXPECT_EQ(high.jpegQuality, 100);
}

TEST(AssetImportSettingsTest, PartialJsonKeepsDefaultsForUnspecifiedFields)
{
    const QJsonObject partial{
        {QStringLiteral("proxyWidth"), 1280},
        {QStringLiteral("workingColorSpace"), QStringLiteral("ACEScg")},
    };

    const AssetImportSettings restored = AssetImportSettings::fromJson(partial);

    EXPECT_EQ(restored.proxyResolution, QSize(1280, 1080));
    EXPECT_EQ(restored.jpegQuality, 85);
    EXPECT_FALSE(restored.generateProxyOnImport);
    EXPECT_TRUE(restored.normalizeColorSpace);
    EXPECT_EQ(restored.workingColorSpace, QStringLiteral("ACEScg"));
}

TEST(AssetImportSettingsTest, MismatchedJsonTypesFallBackToDefaults)
{
    const QJsonObject malformed{
        {QStringLiteral("proxyWidth"), QStringLiteral("wide")},
        {QStringLiteral("proxyHeight"), QStringLiteral("tall")},
        {QStringLiteral("jpegQuality"), QStringLiteral("high")},
        {QStringLiteral("generateProxyOnImport"), QStringLiteral("true")},
        {QStringLiteral("normalizeColorSpace"), QStringLiteral("false")},
        {QStringLiteral("workingColorSpace"), 17},
    };

    const AssetImportSettings restored = AssetImportSettings::fromJson(malformed);

    EXPECT_EQ(restored.proxyResolution, QSize(1920, 1080));
    EXPECT_EQ(restored.jpegQuality, 85);
    EXPECT_FALSE(restored.generateProxyOnImport);
    EXPECT_TRUE(restored.normalizeColorSpace);
    EXPECT_EQ(restored.workingColorSpace, QStringLiteral("Linear"));
}
