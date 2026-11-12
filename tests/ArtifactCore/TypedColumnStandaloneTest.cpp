#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>

import Data.TypedColumn;
import Data.ColumnType;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
std::vector<String> columnValues(std::initializer_list<const char*> input)
{
    std::vector<String> result;
    result.reserve(input.size());
    for (const char* value : input) result.emplace_back(value);
    return result;
}
}

TEST(TypedColumnStandaloneTest, DefaultColumnIsUnnamedUnknownAndEmpty)
{
    const TypedColumn column;
    EXPECT_TRUE(column.name().isEmpty());
    EXPECT_EQ(column.type(), ColumnType::Unknown);
    EXPECT_EQ(column.size(), 0);
}

TEST(TypedColumnStandaloneTest, DefaultColumnGettersReturnTheirFallbacks)
{
    const TypedColumn column;
    EXPECT_EQ(column.getString(0), "");
    EXPECT_EQ(column.getInt(0, 17), 17);
    EXPECT_DOUBLE_EQ(column.getFloat(0, 2.5), 2.5);
    EXPECT_TRUE(column.getBool(0, true));
}

TEST(TypedColumnStandaloneTest, IntegerColumnPreservesNameTypeAndRowCount)
{
    const TypedColumn column(String("frames"), columnValues({"12", "-3", "+7"}));
    EXPECT_EQ(column.name(), "frames");
    EXPECT_EQ(column.type(), ColumnType::Int);
    EXPECT_EQ(column.size(), 3);
}

TEST(TypedColumnStandaloneTest, IntegerColumnReturnsValuesThroughNumericGetters)
{
    const TypedColumn column(String("n"), columnValues({"12", "-3"}));
    EXPECT_EQ(column.getInt(0), 12);
    EXPECT_EQ(column.getInt(1), -3);
    EXPECT_DOUBLE_EQ(column.getFloat(0), 12.0);
    EXPECT_FALSE(column.getBool(0));
}

TEST(TypedColumnStandaloneTest, StringGetterPreservesOriginalIntegerText)
{
    const TypedColumn column(String("n"), columnValues({"+0012"}));
    EXPECT_EQ(column.getString(0), "+0012");
}

TEST(TypedColumnStandaloneTest, FloatColumnConvertsToDoubleAndTruncatesIntegerGetter)
{
    const TypedColumn column(String("gain"), columnValues({"1.75", "-2.25"}));
    EXPECT_EQ(column.type(), ColumnType::Float);
    EXPECT_DOUBLE_EQ(column.getFloat(0), 1.75);
    EXPECT_DOUBLE_EQ(column.getFloat(1), -2.25);
    EXPECT_EQ(column.getInt(0), 1);
    EXPECT_EQ(column.getInt(1), -2);
}

TEST(TypedColumnStandaloneTest, FloatColumnRetainsOriginalStringValues)
{
    const TypedColumn column(String("gain"), columnValues({"1e2", ".50"}));
    EXPECT_EQ(column.getString(0), "1e2");
    EXPECT_EQ(column.getString(1), ".50");
}

TEST(TypedColumnStandaloneTest, BooleanColumnParsesTrueAliases)
{
    const TypedColumn column(String("enabled"), columnValues({"true", "1", "yes", "TRUE", "Yes"}));
    EXPECT_EQ(column.type(), ColumnType::Bool);
    for (int row = 0; row < column.size(); ++row) {
        EXPECT_TRUE(column.getBool(row));
        EXPECT_EQ(column.getInt(row), 1);
        EXPECT_DOUBLE_EQ(column.getFloat(row), 1.0);
    }
}

TEST(TypedColumnStandaloneTest, BooleanColumnParsesFalseAliases)
{
    const TypedColumn column(String("enabled"), columnValues({"false", "0", "no", "FALSE", "No"}));
    EXPECT_EQ(column.type(), ColumnType::Bool);
    for (int row = 0; row < column.size(); ++row) {
        EXPECT_FALSE(column.getBool(row));
        EXPECT_EQ(column.getInt(row), 0);
        EXPECT_DOUBLE_EQ(column.getFloat(row), 0.0);
    }
}

TEST(TypedColumnStandaloneTest, StringColumnKeepsTextAndNumericGettersUseDefaults)
{
    const TypedColumn column(String("label"), columnValues({"alpha", "beta"}));
    EXPECT_EQ(column.type(), ColumnType::String);
    EXPECT_EQ(column.getString(0), "alpha");
    EXPECT_EQ(column.getString(1), "beta");
    EXPECT_EQ(column.getInt(0), 0);
    EXPECT_DOUBLE_EQ(column.getFloat(0), 0.0);
    EXPECT_FALSE(column.getBool(0));
}

TEST(TypedColumnStandaloneTest, DateColumnRetainsTextAndUsesNumericDefaults)
{
    const TypedColumn column(String("date"), columnValues({"2024-02-29", "2025-01-01"}));
    EXPECT_EQ(column.type(), ColumnType::Date);
    EXPECT_EQ(column.getString(0), "2024-02-29");
    EXPECT_EQ(column.getInt(0), 0);
    EXPECT_DOUBLE_EQ(column.getFloat(0), 0.0);
}

TEST(TypedColumnStandaloneTest, InvalidRowsReturnEmptyStringOrCallerFallback)
{
    const TypedColumn column(String("n"), columnValues({"4", "5"}));
    EXPECT_EQ(column.getString(-1), "");
    EXPECT_EQ(column.getString(2), "");
    EXPECT_EQ(column.getInt(-1, 90), 90);
    EXPECT_EQ(column.getInt(2, -8), -8);
    EXPECT_DOUBLE_EQ(column.getFloat(-1, 3.25), 3.25);
    EXPECT_DOUBLE_EQ(column.getFloat(2, -1.5), -1.5);
    EXPECT_TRUE(column.getBool(-1, true));
    EXPECT_TRUE(column.getBool(2, true));
}

TEST(TypedColumnStandaloneTest, ValidFalseBooleanDoesNotUseCallerFallback)
{
    const TypedColumn column(String("flag"), columnValues({"false", "true"}));
    EXPECT_FALSE(column.getBool(0, true));
    EXPECT_TRUE(column.getBool(1, false));
}

TEST(TypedColumnStandaloneTest, EmptyRawValuesAreRetainedAsRows)
{
    const TypedColumn column(String("n"), columnValues({"", "8", ""}));
    EXPECT_EQ(column.type(), ColumnType::Int);
    EXPECT_EQ(column.size(), 3);
    EXPECT_EQ(column.getString(0), "");
    EXPECT_EQ(column.getString(1), "8");
    EXPECT_EQ(column.getString(2), "");
}

TEST(TypedColumnStandaloneTest, EmptyNumericCellsConvertToZero)
{
    const TypedColumn column(String("n"), columnValues({"", "8"}));
    EXPECT_EQ(column.getInt(0), 0);
    EXPECT_DOUBLE_EQ(column.getFloat(0), 0.0);
}

TEST(TypedColumnStandaloneTest, SingleValueColumnInfersItsType)
{
    const TypedColumn column(String("only"), columnValues({"-42"}));
    EXPECT_EQ(column.type(), ColumnType::Int);
    EXPECT_EQ(column.size(), 1);
    EXPECT_EQ(column.getInt(0), -42);
}

TEST(TypedColumnStandaloneTest, WhitespaceTextRemainsStringData)
{
    const TypedColumn column(String("raw"), columnValues({" 42"}));
    EXPECT_EQ(column.type(), ColumnType::String);
    EXPECT_EQ(column.getString(0), " 42");
}

TEST(TypedColumnStandaloneTest, DateTimeColumnPreservesOriginalTimestamp)
{
    const TypedColumn column(String("time"), columnValues({"2024-02-29T12:30:45.25"}));
    EXPECT_EQ(column.type(), ColumnType::DateTime);
    EXPECT_EQ(column.getString(0), "2024-02-29T12:30:45.25");
}

TEST(TypedColumnStandaloneTest, ColumnNameCanBeEmptyIndependentlyOfValues)
{
    const TypedColumn column(String(), columnValues({"1", "2"}));
    EXPECT_TRUE(column.name().isEmpty());
    EXPECT_EQ(column.type(), ColumnType::Int);
    EXPECT_EQ(column.size(), 2);
}

TEST(TypedColumnStandaloneTest, FloatScientificNotationConvertsForAllGetters)
{
    const TypedColumn column(String("v"), columnValues({"1.25e2", "-3e1"}));
    EXPECT_EQ(column.type(), ColumnType::Float);
    EXPECT_DOUBLE_EQ(column.getFloat(0), 125.0);
    EXPECT_DOUBLE_EQ(column.getFloat(1), -30.0);
    EXPECT_EQ(column.getInt(0), 125);
    EXPECT_EQ(column.getInt(1), -30);
}
