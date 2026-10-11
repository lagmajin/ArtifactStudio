#include <gtest/gtest.h>

#include <string>
#include <utility>

import Data.ColumnType;
import Core.ArtifactString;

using namespace ArtifactCore;

TEST(ColumnTypeStandaloneTest, EveryDeclaredTypeHasStableDisplayName)
{
    EXPECT_STREQ(columnTypeToString(ColumnType::Unknown), "Unknown");
    EXPECT_STREQ(columnTypeToString(ColumnType::String), "String");
    EXPECT_STREQ(columnTypeToString(ColumnType::Int), "Int");
    EXPECT_STREQ(columnTypeToString(ColumnType::Float), "Float");
    EXPECT_STREQ(columnTypeToString(ColumnType::Bool), "Bool");
    EXPECT_STREQ(columnTypeToString(ColumnType::Date), "Date");
    EXPECT_STREQ(columnTypeToString(ColumnType::Time), "Time");
    EXPECT_STREQ(columnTypeToString(ColumnType::DateTime), "DateTime");
}

TEST(ColumnTypeStandaloneTest, UnknownIntegerValueFallsBackToUnknownName)
{
    EXPECT_STREQ(columnTypeToString(static_cast<ColumnType>(999)), "Unknown");
}

TEST(ColumnTypeStandaloneTest, CanonicalNamesRoundTrip)
{
    const std::pair<const char*, ColumnType> cases[] = {
        {"String", ColumnType::String}, {"Int", ColumnType::Int},
        {"Float", ColumnType::Float}, {"Bool", ColumnType::Bool},
        {"Date", ColumnType::Date}, {"Time", ColumnType::Time},
        {"DateTime", ColumnType::DateTime}, {"Unknown", ColumnType::Unknown},
    };
    for (const auto& [name, expected] : cases) {
        EXPECT_EQ(columnTypeFromString(String(name)), expected);
    }
}

TEST(ColumnTypeStandaloneTest, StringTypeAcceptsLowercaseAlias)
{
    EXPECT_EQ(columnTypeFromString(String("string")), ColumnType::String);
}

TEST(ColumnTypeStandaloneTest, IntegerTypeAcceptsIntegerAliases)
{
    EXPECT_EQ(columnTypeFromString(String("int")), ColumnType::Int);
    EXPECT_EQ(columnTypeFromString(String("Integer")), ColumnType::Int);
    EXPECT_EQ(columnTypeFromString(String("integer")), ColumnType::Int);
}

TEST(ColumnTypeStandaloneTest, FloatTypeAcceptsNumericAliases)
{
    EXPECT_EQ(columnTypeFromString(String("float")), ColumnType::Float);
    EXPECT_EQ(columnTypeFromString(String("Double")), ColumnType::Float);
    EXPECT_EQ(columnTypeFromString(String("double")), ColumnType::Float);
    EXPECT_EQ(columnTypeFromString(String("Number")), ColumnType::Float);
    EXPECT_EQ(columnTypeFromString(String("number")), ColumnType::Float);
}

TEST(ColumnTypeStandaloneTest, BooleanTypeAcceptsBooleanAliases)
{
    EXPECT_EQ(columnTypeFromString(String("bool")), ColumnType::Bool);
    EXPECT_EQ(columnTypeFromString(String("Boolean")), ColumnType::Bool);
    EXPECT_EQ(columnTypeFromString(String("boolean")), ColumnType::Bool);
}

TEST(ColumnTypeStandaloneTest, DateAndTimeTypesAcceptLowercaseAliases)
{
    EXPECT_EQ(columnTypeFromString(String("date")), ColumnType::Date);
    EXPECT_EQ(columnTypeFromString(String("time")), ColumnType::Time);
}

TEST(ColumnTypeStandaloneTest, DateTimeTypeAcceptsTimestampAliases)
{
    EXPECT_EQ(columnTypeFromString(String("datetime")), ColumnType::DateTime);
    EXPECT_EQ(columnTypeFromString(String("Timestamp")), ColumnType::DateTime);
    EXPECT_EQ(columnTypeFromString(String("timestamp")), ColumnType::DateTime);
}

TEST(ColumnTypeStandaloneTest, UnknownAndEmptyNamesMapToUnknown)
{
    EXPECT_EQ(columnTypeFromString(String("decimal")), ColumnType::Unknown);
    EXPECT_EQ(columnTypeFromString(String("")), ColumnType::Unknown);
}

TEST(ColumnTypeStandaloneTest, AliasMatchingIsCaseSensitiveOutsideListedAliases)
{
    EXPECT_EQ(columnTypeFromString(String("STRING")), ColumnType::Unknown);
    EXPECT_EQ(columnTypeFromString(String("INTEGER")), ColumnType::Unknown);
    EXPECT_EQ(columnTypeFromString(String("DATETIME")), ColumnType::Unknown);
}
