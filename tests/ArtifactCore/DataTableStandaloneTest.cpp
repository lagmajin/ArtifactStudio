#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

import Data.DataTable;
import Data.CsvParser;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
CsvParseResult parse(std::string_view text, bool hasHeader = true)
{
    CsvParseOptions options;
    options.hasHeader = hasHeader;
    return CsvParser::parseString(text, options);
}

std::vector<DataValue> rowValues(std::initializer_list<DataValue> values)
{
    return std::vector<DataValue>(values);
}
}

TEST(DataTableStandaloneTest, DefaultTableHasNoRowsOrColumns)
{
    const DataTable table;
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.rowCount(), 0);
    EXPECT_EQ(table.columnCount(), 0);
    EXPECT_TRUE(table.columns().empty());
}

TEST(DataTableStandaloneTest, DefaultTableAccessReturnsEmptyValues)
{
    const DataTable table;
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(0, 0)));
    EXPECT_EQ(table.getString(0, 0), "");
    EXPECT_EQ(table.getInt(0, 0, 12), 12);
    EXPECT_DOUBLE_EQ(table.getFloat(0, 0, 2.5), 2.5);
    EXPECT_TRUE(table.getBool(0, 0, true));
}

TEST(DataTableStandaloneTest, SuccessfulCsvBuildsNamedRowsAndColumns)
{
    const DataTable table(parse("name,value\nalpha,12\nbeta,2.5"));
    EXPECT_EQ(table.rowCount(), 2);
    EXPECT_EQ(table.columnCount(), 2);
    EXPECT_EQ(table.columnName(0), "name");
    EXPECT_EQ(table.columnName(1), "value");
    EXPECT_EQ(table.columnIndexByName(String("value")), 1);
}

TEST(DataTableStandaloneTest, HeaderlessCsvGetsGeneratedColumnNames)
{
    const DataTable table(parse("alpha,1\nbeta,2", false));
    EXPECT_EQ(table.columnCount(), 2);
    EXPECT_EQ(table.columnName(0), "col_0");
    EXPECT_EQ(table.columnName(1), "col_1");
}

TEST(DataTableStandaloneTest, CsvCellsRemainStringsInVariantStorage)
{
    const DataTable table(parse("value\n42"));
    const DataValue value = table.at(0, 0);
    ASSERT_TRUE(std::holds_alternative<String>(value));
    EXPECT_EQ(std::get<String>(value), "42");
}

TEST(DataTableStandaloneTest, LookupWorksByColumnIndexAndName)
{
    const DataTable table(parse("name,count\nwidget,14"));
    EXPECT_EQ(table.getString(0, 0), "widget");
    EXPECT_EQ(table.getString(0, String("name")), "widget");
    EXPECT_EQ(table.getInt(0, 1), 14);
    EXPECT_EQ(table.getInt(0, String("count")), 14);
}

TEST(DataTableStandaloneTest, IntegerGetterParsesIntegerText)
{
    const DataTable table(parse("value\n-27"));
    EXPECT_EQ(table.getInt(0, 0), -27);
}

TEST(DataTableStandaloneTest, IntegerGetterTruncatesStoredFloatingValue)
{
    DataTable table(parse("value\n0"));
    table.set(0, 0, 3.75);
    EXPECT_EQ(table.getInt(0, 0), 3);
}

TEST(DataTableStandaloneTest, FloatGetterParsesNumericText)
{
    const DataTable table(parse("value\n-1.25e2"));
    EXPECT_DOUBLE_EQ(table.getFloat(0, 0), -125.0);
}

TEST(DataTableStandaloneTest, FloatGetterConvertsStoredInteger)
{
    DataTable table(parse("value\n0"));
    table.set(0, 0, int64_t{42});
    EXPECT_DOUBLE_EQ(table.getFloat(0, 0), 42.0);
}

TEST(DataTableStandaloneTest, BoolGetterRecognizesSupportedStringTokens)
{
    const DataTable table(parse("flag\ntrue\nfalse\n1\n0\nyes\nno"));
    EXPECT_TRUE(table.getBool(0, 0));
    EXPECT_FALSE(table.getBool(1, 0, true));
    EXPECT_TRUE(table.getBool(2, 0));
    EXPECT_FALSE(table.getBool(3, 0, true));
    EXPECT_TRUE(table.getBool(4, 0));
    EXPECT_FALSE(table.getBool(5, 0, true));
}

TEST(DataTableStandaloneTest, BoolGetterConvertsStoredInteger)
{
    DataTable table(parse("flag\n0"));
    table.set(0, 0, int64_t{2});
    EXPECT_TRUE(table.getBool(0, 0));
    table.set(0, 0, int64_t{0});
    EXPECT_FALSE(table.getBool(0, 0, true));
}

TEST(DataTableStandaloneTest, CallerFallbackSurvivesUnconvertibleText)
{
    const DataTable table(parse("text\nhello"));
    EXPECT_EQ(table.getInt(0, 0, 77), 77);
    EXPECT_DOUBLE_EQ(table.getFloat(0, 0, 1.25), 1.25);
    EXPECT_TRUE(table.getBool(0, 0, true));
}

TEST(DataTableStandaloneTest, UnknownColumnNameReturnsFallbackOrEmptyString)
{
    const DataTable table(parse("name\nalpha"));
    EXPECT_EQ(table.columnIndexByName(String("missing")), -1);
    EXPECT_EQ(table.getString(0, String("missing")), "");
    EXPECT_EQ(table.getInt(0, String("missing"), 23), 23);
    EXPECT_DOUBLE_EQ(table.getFloat(0, String("missing"), 3.5), 3.5);
    EXPECT_TRUE(table.getBool(0, String("missing"), true));
}

TEST(DataTableStandaloneTest, OutOfRangeRowsAndColumnsReturnFallbacks)
{
    const DataTable table(parse("name,value\na,7"));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(-1, 0)));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(1, 0)));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(0, -1)));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(0, 2)));
    EXPECT_EQ(table.getString(-1, 0), "");
    EXPECT_EQ(table.getInt(0, 2, 9), 9);
    EXPECT_DOUBLE_EQ(table.getFloat(4, 0, 6.5), 6.5);
    EXPECT_TRUE(table.getBool(4, 0, true));
}

TEST(DataTableStandaloneTest, InvalidColumnNamesReturnEmptyName)
{
    const DataTable table(parse("name\na"));
    EXPECT_EQ(table.columnName(-1), "");
    EXPECT_EQ(table.columnName(1), "");
}

TEST(DataTableStandaloneTest, AddRowWithMatchingWidthAppendsData)
{
    DataTable table(parse("name,count\na,1"));
    table.addRow(rowValues({String("b"), int64_t{2}}));
    ASSERT_EQ(table.rowCount(), 2);
    EXPECT_EQ(table.getString(1, String("name")), "b");
    EXPECT_EQ(table.getInt(1, String("count")), 2);
}

TEST(DataTableStandaloneTest, AddRowWithWrongWidthIsIgnored)
{
    DataTable table(parse("name,count\na,1"));
    table.addRow(rowValues({String("b")}));
    table.addRow(rowValues({String("b"), int64_t{2}, false}));
    EXPECT_EQ(table.rowCount(), 1);
}

TEST(DataTableStandaloneTest, AddColumnCreatesEmptyCellsForExistingRows)
{
    DataTable table(parse("name\na\nb"));
    table.addColumn(String("active"));
    ASSERT_EQ(table.columnCount(), 2);
    EXPECT_EQ(table.columnIndexByName(String("active")), 1);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(0, 1)));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(table.at(1, 1)));
}

TEST(DataTableStandaloneTest, DuplicateColumnNameIsIgnored)
{
    DataTable table(parse("name,count\na,1"));
    table.addColumn(String("name"));
    EXPECT_EQ(table.columnCount(), 2);
    EXPECT_EQ(table.columnIndexByName(String("name")), 0);
}

TEST(DataTableStandaloneTest, AddColumnToEmptyTableCreatesAddressableSchema)
{
    DataTable table;
    table.addColumn(String("new"));
    EXPECT_EQ(table.columnCount(), 1);
    EXPECT_EQ(table.columnName(0), "new");
    EXPECT_EQ(table.rowCount(), 0);
}

TEST(DataTableStandaloneTest, SetUpdatesExistingCellAndIgnoresInvalidCoordinates)
{
    DataTable table(parse("name\na"));
    table.set(0, 0, String("updated"));
    EXPECT_EQ(table.getString(0, 0), "updated");
    table.set(-1, 0, String("ignored"));
    table.set(0, 1, String("ignored"));
    EXPECT_EQ(table.getString(0, 0), "updated");
}

TEST(DataTableStandaloneTest, ClearRemovesSchemaLookupAndRows)
{
    DataTable table(parse("name\na"));
    table.clear();
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.rowCount(), 0);
    EXPECT_EQ(table.columnCount(), 0);
    EXPECT_EQ(table.columnIndexByName(String("name")), -1);
}

TEST(DataTableStandaloneTest, InvalidCsvCreatesEmptyTable)
{
    CsvParseResult invalid;
    invalid.error = CsvParseError::EmptyFile;
    const DataTable table(invalid);
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.columnCount(), 0);
}

TEST(DataTableStandaloneTest, RowReturnsTheStoredVariantSequence)
{
    DataTable table(parse("name,value\na,1"));
    table.set(0, 1, int64_t{7});
    const auto& values = table.row(0);
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(std::get<String>(values[0]), "a");
    EXPECT_EQ(std::get<int64_t>(values[1]), 7);
}

TEST(DataTableStandaloneTest, InvalidRowReturnsAnEmptySequence)
{
    const DataTable table(parse("name\na"));
    EXPECT_TRUE(table.row(-1).empty());
    EXPECT_TRUE(table.row(1).empty());
}

TEST(DataTableStandaloneTest, DataValueToStringFormatsEveryVariantAlternative)
{
    EXPECT_EQ(dataValueToString(DataValue{}), "");
    EXPECT_EQ(dataValueToString(DataValue(String("text"))), "text");
    EXPECT_EQ(dataValueToString(DataValue(int64_t{42})), "42");
    EXPECT_EQ(dataValueToString(DataValue(2.5)), "2.500000");
    EXPECT_EQ(dataValueToString(DataValue(true)), "true");
    EXPECT_EQ(dataValueToString(DataValue(false)), "false");
}

TEST(DataTableStandaloneTest, StringGetterFormatsNonStringVariantValues)
{
    DataTable table(parse("a,b,c\nx,x,x"));
    table.set(0, 0, int64_t{8});
    table.set(0, 1, 1.25);
    table.set(0, 2, true);
    EXPECT_EQ(table.getString(0, 0), "8");
    EXPECT_EQ(table.getString(0, 1), "1.250000");
    EXPECT_EQ(table.getString(0, 2), "true");
}

TEST(DataTableStandaloneTest, MonostateUsesFallbacksForTypedGetters)
{
    DataTable table(parse("value\nx"));
    table.set(0, 0, std::monostate{});
    EXPECT_EQ(table.getString(0, 0), "");
    EXPECT_EQ(table.getInt(0, 0, 9), 9);
    EXPECT_DOUBLE_EQ(table.getFloat(0, 0, 1.75), 1.75);
    EXPECT_TRUE(table.getBool(0, 0, true));
}

TEST(DataTableStandaloneTest, AddingAColumnToEmptyTableKeepsItRowEmpty)
{
    DataTable table;
    table.addColumn(String("schemaOnly"));
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.columnCount(), 1);
    EXPECT_EQ(table.rowCount(), 0);
}

TEST(DataTableStandaloneTest, AddingAColumnPreservesRowsAndExistingValues)
{
    DataTable table(parse("name\na\nb"));
    table.addColumn(String("score"));
    table.set(1, 1, int64_t{5});
    ASSERT_EQ(table.rowCount(), 2);
    EXPECT_EQ(table.getString(0, 0), "a");
    EXPECT_EQ(table.getInt(1, String("score")), 5);
}

TEST(DataTableStandaloneTest, DuplicateHeaderLookupResolvesToLastOccurrence)
{
    const DataTable table(parse("value,value\nfirst,second"));
    ASSERT_EQ(table.columnCount(), 2);
    EXPECT_EQ(table.columnIndexByName(String("value")), 1);
    EXPECT_EQ(table.getString(0, String("value")), "second");
}
