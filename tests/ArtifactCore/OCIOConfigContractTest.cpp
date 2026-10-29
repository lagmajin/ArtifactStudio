#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

import Color.OCIOConfig;

using namespace ArtifactCore;

namespace {
QJsonObject fixtureConfig()
{
    QJsonObject roles;
    roles.insert(QStringLiteral("scene_linear"), QStringLiteral("Working Linear"));
    roles.insert(QStringLiteral("default_byte"), QStringLiteral("Display sRGB"));

    QJsonObject workingSpace;
    workingSpace.insert(QStringLiteral("name"), QStringLiteral("Working Linear"));
    workingSpace.insert(QStringLiteral("family"), QStringLiteral("Working"));
    workingSpace.insert(QStringLiteral("description"), QStringLiteral("Scene linear test space"));
    workingSpace.insert(QStringLiteral("is_data"), false);
    workingSpace.insert(QStringLiteral("aliases"),
                        QJsonArray{QStringLiteral("working"), QStringLiteral("scene")});

    QJsonObject displaySpace;
    displaySpace.insert(QStringLiteral("name"), QStringLiteral("Display sRGB"));
    displaySpace.insert(QStringLiteral("family"), QStringLiteral("Display"));
    displaySpace.insert(QStringLiteral("is_data"), true);

    QJsonObject firstView;
    firstView.insert(QStringLiteral("display"), QStringLiteral("Monitor"));
    firstView.insert(QStringLiteral("view"), QStringLiteral("Standard"));
    firstView.insert(QStringLiteral("colorspace"), QStringLiteral("Display sRGB"));
    firstView.insert(QStringLiteral("looks"), QStringLiteral("neutral"));

    QJsonObject secondView;
    secondView.insert(QStringLiteral("display"), QStringLiteral("Monitor"));
    secondView.insert(QStringLiteral("view"), QStringLiteral("Soft Proof"));
    secondView.insert(QStringLiteral("colorspace"), QStringLiteral("Display sRGB"));

    QJsonObject json;
    json.insert(QStringLiteral("config_file_path"), QStringLiteral("memory://ocio-test"));
    json.insert(QStringLiteral("roles"), roles);
    json.insert(QStringLiteral("color_spaces"), QJsonArray{workingSpace, displaySpace});
    json.insert(QStringLiteral("display_views"), QJsonArray{firstView, secondView});
    json.insert(QStringLiteral("active_display"), QStringLiteral("Monitor"));
    json.insert(QStringLiteral("active_view"), QStringLiteral("Standard"));
    json.insert(QStringLiteral("active_looks"), QStringLiteral("neutral"));
    json.insert(QStringLiteral("working_space"), QStringLiteral("Working Linear"));
    return json;
}
}

TEST(OCIOConfigContractTest, LoadsRolesColorSpacesAliasesAndWorkingSpace)
{
    OCIOConfig config;
    ASSERT_TRUE(config.loadFromJson(fixtureConfig()));
    EXPECT_TRUE(config.isValid());
    EXPECT_EQ(config.colorSpaceForRole(QStringLiteral("scene_linear")),
              QStringLiteral("Working Linear"));
    EXPECT_EQ(config.colorSpaceForRole(QStringLiteral("missing")), QString());
    EXPECT_EQ(config.workingSpace(), QStringLiteral("Working Linear"));

    const OCIOColorSpace* byName = config.colorSpace(QStringLiteral("Working Linear"));
    ASSERT_NE(byName, nullptr);
    EXPECT_EQ(byName->family, QStringLiteral("Working"));
    EXPECT_FALSE(byName->isData);
    const OCIOColorSpace* byAlias = config.colorSpace(QStringLiteral("scene"));
    ASSERT_NE(byAlias, nullptr);
    EXPECT_EQ(byAlias->name, QStringLiteral("Working Linear"));
    EXPECT_EQ(config.colorSpaces().size(), 2);
    EXPECT_EQ(config.roles().size(), 2);
}

TEST(OCIOConfigContractTest, DisplayQueriesDeduplicateDisplaysAndPreserveViewOrder)
{
    OCIOConfig config;
    ASSERT_TRUE(config.loadFromJson(fixtureConfig()));
    EXPECT_EQ(config.displays(), QStringList{QStringLiteral("Monitor")});
    EXPECT_EQ(config.viewsForDisplay(QStringLiteral("Monitor")),
              (QStringList{QStringLiteral("Standard"), QStringLiteral("Soft Proof")}));
    EXPECT_TRUE(config.viewsForDisplay(QStringLiteral("Unknown")).isEmpty());
    EXPECT_EQ(config.displayViews().size(), 2);
}

TEST(OCIOConfigContractTest, ActiveDisplayAndViewRejectUnknownValuesWithoutReplacingSelection)
{
    OCIOConfig config;
    ASSERT_TRUE(config.loadFromJson(fixtureConfig()));
    config.setActiveDisplay(QStringLiteral("Missing Monitor"));
    EXPECT_EQ(config.activeDisplay(), QStringLiteral("Monitor"));
    EXPECT_NE(config.lastError().indexOf(QStringLiteral("Unknown display")), -1);

    config.setActiveView(QStringLiteral("Missing View"));
    EXPECT_EQ(config.activeView(), QStringLiteral("Standard"));
    EXPECT_NE(config.lastError().indexOf(QStringLiteral("Unknown view")), -1);

    config.setActiveView(QStringLiteral("Soft Proof"));
    EXPECT_EQ(config.activeView(), QStringLiteral("Soft Proof"));
    config.setActiveLooks(QStringLiteral("film"));
    EXPECT_EQ(config.activeLooks(), QStringLiteral("film"));
}

TEST(OCIOConfigContractTest, WorkingSpaceAcceptsNamesAndAliasesButRejectsUnknownNames)
{
    OCIOConfig config;
    ASSERT_TRUE(config.loadFromJson(fixtureConfig()));
    config.setWorkingSpace(QStringLiteral("scene"));
    EXPECT_EQ(config.workingSpace(), QStringLiteral("scene"));

    config.setWorkingSpace(QStringLiteral("Missing Space"));
    EXPECT_EQ(config.workingSpace(), QStringLiteral("scene"));
    EXPECT_NE(config.lastError().indexOf(QStringLiteral("Unknown working color space")), -1);
}

TEST(OCIOConfigContractTest, JsonRoundTripPreservesConfigurationMetadata)
{
    OCIOConfig original;
    ASSERT_TRUE(original.loadFromJson(fixtureConfig()));
    original.setConfigFilePath(QStringLiteral("/configs/color.ocio.json"));

    OCIOConfig restored;
    ASSERT_TRUE(restored.loadFromJson(original.toJson()));
    EXPECT_TRUE(restored.isValid());
    EXPECT_EQ(restored.configFilePath(), QStringLiteral("/configs/color.ocio.json"));
    EXPECT_EQ(restored.colorSpaceForRole(QStringLiteral("default_byte")),
              QStringLiteral("Display sRGB"));
    const OCIOColorSpace* restoredSpace =
        restored.colorSpace(QStringLiteral("working"));
    ASSERT_NE(restoredSpace, nullptr);
    EXPECT_EQ(restoredSpace->name, QStringLiteral("Working Linear"));
    EXPECT_EQ(restored.activeDisplay(), QStringLiteral("Monitor"));
    EXPECT_EQ(restored.activeView(), QStringLiteral("Standard"));
    EXPECT_EQ(restored.activeLooks(), QStringLiteral("neutral"));
    EXPECT_EQ(restored.workingSpace(), QStringLiteral("Working Linear"));
    EXPECT_EQ(restored.displayViews().size(), 2);
}

TEST(OCIOConfigContractTest, LoadingMalformedJsonFileReportsErrorWithoutMarkingConfigValid)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString path = tempDir.filePath(QStringLiteral("invalid-config.json"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_EQ(file.write("{invalid json"), 13);
    file.close();

    OCIOConfig config;
    EXPECT_FALSE(config.loadFromFile(path));
    EXPECT_FALSE(config.isValid());
    EXPECT_NE(config.lastError().indexOf(QStringLiteral("JSON parse error")), -1);
}
