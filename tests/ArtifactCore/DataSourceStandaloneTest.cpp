#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

import Data.DataSource;
import Data.DataTable;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
class RegistryTestSource final : public IDataSource {
public:
    explicit RegistryTestSource(const String& uri) : uri_(uri) {}

    DataSourceInfo info() const override
    {
        DataSourceInfo result;
        result.uri = uri_;
        result.kind = DataSourceKind::Unknown;
        return result;
    }

    const DataTable& table() const override { return table_; }
    bool reload() override { return true; }
    bool isValid() const override { return true; }
    String lastError() const override { return {}; }

private:
    String uri_;
    DataTable table_;
};

DataSourceFactory sourceFactory()
{
    return [](const String& uri) -> DataSourcePtr {
        return DataSourcePtr(std::make_shared<RegistryTestSource>(uri));
    };
}
}

TEST(DataSourceStandaloneTest, KindNamesCoverAllDeclaredValues)
{
    EXPECT_STREQ(dataSourceKindToString(DataSourceKind::Unknown), "Unknown");
    EXPECT_STREQ(dataSourceKindToString(DataSourceKind::Csv), "Csv");
    EXPECT_STREQ(dataSourceKindToString(DataSourceKind::Json), "Json");
    EXPECT_STREQ(dataSourceKindToString(DataSourceKind::Tsv), "Tsv");
    EXPECT_STREQ(dataSourceKindToString(static_cast<DataSourceKind>(99)), "Unknown");
}

TEST(DataSourceStandaloneTest, UnregisteredExtensionReturnsNull)
{
    EXPECT_EQ(DataSourceRegistry::instance().open(String("asset.unregistered-standalone")), nullptr);
}

TEST(DataSourceStandaloneTest, RegisteredExtensionDispatchesToFactory)
{
    auto& registry = DataSourceRegistry::instance();
    registry.registerFormat(String(".standalone-source"), sourceFactory());

    const auto source = registry.open(String("project.standalone-source"));

    ASSERT_NE(source, nullptr);
    EXPECT_EQ(toStdString(source->info().uri), "project.standalone-source");
}

TEST(DataSourceStandaloneTest, ExtensionMatchingIsCaseInsensitive)
{
    auto& registry = DataSourceRegistry::instance();
    registry.registerFormat(String(".standalone-case"), sourceFactory());

    const auto source = registry.open(String("project.STANDALONE-CASE"));

    ASSERT_NE(source, nullptr);
    EXPECT_EQ(toStdString(source->info().uri), "project.STANDALONE-CASE");
}

TEST(DataSourceStandaloneTest, PathOverloadDispatchesUsingPathExtension)
{
    auto& registry = DataSourceRegistry::instance();
    registry.registerFormat(String(".standalone-path"), sourceFactory());
    const std::filesystem::path path("folder/data.standalone-path");

    const auto source = registry.open(path);

    ASSERT_NE(source, nullptr);
    EXPECT_EQ(toStdString(source->info().uri), path.string());
}

TEST(DataSourceStandaloneTest, ReRegisteringExtensionReplacesPreviousFactory)
{
    auto& registry = DataSourceRegistry::instance();
    registry.registerFormat(String(".standalone-replace"), sourceFactory());
    registry.registerFormat(String(".standalone-replace"), [](const String&) {
        return DataSourcePtr{};
    });

    EXPECT_EQ(registry.open(String("item.standalone-replace")), nullptr);
}

TEST(DataSourceStandaloneTest, RegisteredExtensionsExposeRegisteredKey)
{
    auto& registry = DataSourceRegistry::instance();
    registry.registerFormat(String(".standalone-list"), sourceFactory());

    const auto extensions = registry.registeredExtensions();
    bool found = false;
    for (const auto& extension : extensions) {
        if (toStdString(extension) == ".standalone-list") found = true;
    }
    EXPECT_TRUE(found);
}

TEST(DataSourceStandaloneTest, FactoryReceivesOriginalUri)
{
    auto& registry = DataSourceRegistry::instance();
    registry.registerFormat(String(".standalone-uri"), sourceFactory());

    const auto source = registry.open(String("C:/assets/My File.standalone-uri"));

    ASSERT_NE(source, nullptr);
    EXPECT_EQ(toStdString(source->info().uri), "C:/assets/My File.standalone-uri");
}
