#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

import Data.CsvDataSource;
import Data.DataSource;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
class TemporaryCsv {
public:
    TemporaryCsv()
    {
        static std::atomic_uint64_t nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("artifact-csv-source-" + std::to_string(stamp) + "-" +
             std::to_string(nextId.fetch_add(1)) + ".csv");
    }

    ~TemporaryCsv()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

    void changeExtension(const std::string& extension)
    {
        auto renamedPath = path_;
        renamedPath.replace_extension(extension);
        std::filesystem::rename(path_, renamedPath);
        path_ = std::move(renamedPath);
    }

    void write(const std::string& contents) const
    {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file << contents;
        file.close();
        if (!file) throw std::runtime_error("failed to write temporary CSV");
    }

private:
    std::filesystem::path path_;
};
}

TEST(CsvDataSourceStandaloneTest, ExistingCsvLoadsTableAndMetadata)
{
    TemporaryCsv file;
    file.write("name,count\nwidget,3\ngizmo,7\n");
    const CsvDataSource source(file.path());
    ASSERT_TRUE(source.isValid());
    EXPECT_TRUE(source.lastError().isEmpty());
    EXPECT_EQ(source.table().rowCount(), 2);
    EXPECT_EQ(source.table().columnCount(), 2);
    EXPECT_EQ(source.table().getString(0, String("name")), "widget");
    const auto info = source.info();
    EXPECT_EQ(info.kind, DataSourceKind::Csv);
    EXPECT_EQ(toStdString(info.displayName), file.path().filename().string());
    EXPECT_EQ(info.rowCount, 2);
    EXPECT_EQ(info.columnCount, 2);
    EXPECT_EQ(info.fileSize, 28);
}

TEST(CsvDataSourceStandaloneTest, StringUriConstructorLoadsTheSameFile)
{
    TemporaryCsv file;
    file.write("value\n42\n");
    const CsvDataSource source(String(file.path().string()));
    ASSERT_TRUE(source.isValid());
    EXPECT_EQ(source.table().getInt(0, String("value")), 42);
    EXPECT_EQ(toStdString(source.info().uri), file.path().string());
}

TEST(CsvDataSourceStandaloneTest, MissingFileSetsErrorAndLeavesEmptyTable)
{
    TemporaryCsv file;
    const CsvDataSource source(file.path());
    EXPECT_FALSE(source.isValid());
    EXPECT_FALSE(source.lastError().isEmpty());
    EXPECT_EQ(source.table().rowCount(), 0);
    EXPECT_EQ(source.table().columnCount(), 0);
}

TEST(CsvDataSourceStandaloneTest, ReloadReadsUpdatedFileContents)
{
    TemporaryCsv file;
    file.write("value\nfirst\n");
    CsvDataSource source(file.path());
    ASSERT_TRUE(source.isValid());
    file.write("value\nsecond\nthird\n");
    ASSERT_TRUE(source.reload());
    EXPECT_EQ(source.table().rowCount(), 2);
    EXPECT_EQ(source.table().getString(0, String("value")), "second");
    EXPECT_EQ(source.table().getString(1, String("value")), "third");
}

TEST(CsvDataSourceStandaloneTest, ReloadFailureClearsPreviouslyLoadedTable)
{
    TemporaryCsv file;
    file.write("value\nkept\n");
    CsvDataSource source(file.path());
    ASSERT_TRUE(source.isValid());
    std::filesystem::remove(file.path());
    EXPECT_FALSE(source.reload());
    EXPECT_FALSE(source.isValid());
    EXPECT_EQ(source.table().rowCount(), 0);
    EXPECT_EQ(source.table().columnCount(), 0);
}

TEST(CsvDataSourceStandaloneTest, InconsistentRowsFailAndExposeParserError)
{
    TemporaryCsv file;
    file.write("a,b\n1,2\n3\n");
    const CsvDataSource source(file.path());
    EXPECT_FALSE(source.isValid());
    EXPECT_NE(toStdString(source.lastError()).find("expected 2"), std::string::npos);
    EXPECT_EQ(source.table().rowCount(), 0);
}

TEST(CsvDataSourceStandaloneTest, HeaderOnlyCsvIsValidWithZeroRows)
{
    TemporaryCsv file;
    file.write("first,second\n");
    const CsvDataSource source(file.path());
    ASSERT_TRUE(source.isValid());
    EXPECT_EQ(source.table().rowCount(), 0);
    EXPECT_EQ(source.table().columnCount(), 2);
    EXPECT_EQ(source.info().rowCount, 0);
    EXPECT_EQ(source.info().columnCount, 2);
}

TEST(CsvDataSourceStandaloneTest, DisplayNameUsesFileNameRatherThanFullPath)
{
    TemporaryCsv file;
    file.write("x\n1");
    const CsvDataSource source(file.path());
    const auto displayName = source.info().displayName;
    EXPECT_EQ(toStdString(displayName), file.path().filename().string());
    EXPECT_NE(toStdString(displayName), file.path().string());
}

TEST(CsvDataSourceStandaloneTest, FactoryConstructsUsableSource)
{
    TemporaryCsv file;
    file.write("name\nfactory\n");
    const DataSourcePtr source = makeCsvDataSource(String(file.path().string()));
    ASSERT_TRUE(static_cast<bool>(source));
    ASSERT_TRUE(source->isValid());
    EXPECT_EQ(source->table().getString(0, String("name")), "factory");
}

TEST(CsvDataSourceStandaloneTest, CsvRegistrationOpensUppercaseExtension)
{
    TemporaryCsv file;
    file.write("name\nregistered\n");
    file.changeExtension(".CSV");
    csvDataSourceAutoRegister();
    const auto source = DataSourceRegistry::instance().open(file.path());
    ASSERT_TRUE(static_cast<bool>(source));
    ASSERT_TRUE(source->isValid());
    EXPECT_EQ(source->table().getString(0, String("name")), "registered");
}
