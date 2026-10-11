#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>

import Data.TypeInference;
import Data.ColumnType;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
std::vector<String> values(std::initializer_list<const char*> input)
{
    std::vector<String> result;
    result.reserve(input.size());
    for (const char* item : input) {
        result.emplace_back(item);
    }
    return result;
}
}

TEST(TypeInferenceStandaloneTest, EmptyInputIsUnknown)
{
    EXPECT_EQ(TypeInference::inferFromValues({}), ColumnType::Unknown);
}

TEST(TypeInferenceStandaloneTest, AllEmptyValuesAreUnknown)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"", "", ""})), ColumnType::Unknown);
}

TEST(TypeInferenceStandaloneTest, EmptyValuesAreIgnoredAlongsideValidValues)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"", "42", "", "-7"})),
              ColumnType::Int);
}

TEST(TypeInferenceStandaloneTest, BooleanWordsAndDigitsInferBoolean)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"true", "false", "1", "0"})),
              ColumnType::Bool);
}

TEST(TypeInferenceStandaloneTest, BooleanAliasesAcceptDocumentedCapitalization)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"yes", "no", "TRUE", "FALSE", "Yes", "No"})),
              ColumnType::Bool);
}

TEST(TypeInferenceStandaloneTest, SignedIntegerValuesInferInteger)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"0", "+17", "-2048"})),
              ColumnType::Int);
}

TEST(TypeInferenceStandaloneTest, DecimalValuesInferFloat)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"1.25", "-0.5", ".75", "3."})),
              ColumnType::Float);
}

TEST(TypeInferenceStandaloneTest, ScientificNotationInfersFloat)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"1e3", "-2.5E-4", "+6E+2"})),
              ColumnType::Float);
}

TEST(TypeInferenceStandaloneTest, IntegerAndDecimalValuesPromoteToFloat)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"12", "12.5", "-8"})),
              ColumnType::Float);
}

TEST(TypeInferenceStandaloneTest, BooleanAndNonBooleanNumericValuePromoteToFloat)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"true", "2"})), ColumnType::Float);
}

TEST(TypeInferenceStandaloneTest, BooleanAndFractionalNumberPromoteToFloat)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"false", "0.5"})), ColumnType::Float);
}

TEST(TypeInferenceStandaloneTest, IsoDateInfersDate)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"2024-06-01", "2025-12-31"})),
              ColumnType::Date);
}

TEST(TypeInferenceStandaloneTest, SlashSeparatedDatesInferDate)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"2024/6/1", "06/01/2025"})),
              ColumnType::Date);
}

TEST(TypeInferenceStandaloneTest, ClockValuesInferTime)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"9:05", "23:59:58", "12:30:00.125"})),
              ColumnType::Time);
}

TEST(TypeInferenceStandaloneTest, IsoDateTimesWithTOrSpaceInferDateTime)
{
    EXPECT_EQ(TypeInference::inferFromValues(
                  values({"2024-06-01T09:05", "2025/12/31 23:59:58.125"})),
              ColumnType::DateTime);
}

TEST(TypeInferenceStandaloneTest, DateTimePatternTakesPrecedenceOverOtherCategories)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"2024-06-01T09:05:30"})),
              ColumnType::DateTime);
}

TEST(TypeInferenceStandaloneTest, MixedDateAndDateTimeValuesBecomeString)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"2024-06-01", "2024-06-01T09:05"})),
              ColumnType::String);
}

TEST(TypeInferenceStandaloneTest, MixedUnrelatedTypesBecomeString)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"17", "hello"})), ColumnType::String);
}

TEST(TypeInferenceStandaloneTest, MalformedNumericStringsBecomeString)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"1e", "2e+", "3.4.5"})),
              ColumnType::String);
}

TEST(TypeInferenceStandaloneTest, WhitespaceIsNotSilentlyTrimmed)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({" 42"})), ColumnType::String);
}

TEST(TypeInferenceStandaloneTest, MatchingIsCaseSensitiveForBooleanTokens)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"TRUE", "False"})), ColumnType::String);
}

TEST(TypeInferenceStandaloneTest, SingleValidValueInfersItsSpecificType)
{
    EXPECT_EQ(TypeInference::inferFromValues(values({"2024-06-01T12:30:45.5"})),
              ColumnType::DateTime);
}
