#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

import Data.CsvParser;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
std::string cell(const CsvParseResult& result, int row, int column)
{
    return toStdString(result.rows[static_cast<size_t>(row)][static_cast<size_t>(column)]);
}
}

TEST(CsvParserStandaloneTest, EmptyInputReturnsEmptyFileError)
{
    const auto result = CsvParser::parseString("");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, CsvParseError::EmptyFile);
    EXPECT_EQ(result.rowCount, 0);
    EXPECT_EQ(result.columnCount, 0);
}

TEST(CsvParserStandaloneTest, DefaultOptionsParseHeaderAndRows)
{
    const auto result = CsvParser::parseString("name,score\nAda,10\nLin,12");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.headers.size(), 2);
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(toStdString(result.headers[0]), "name");
    EXPECT_EQ(toStdString(result.headers[1]), "score");
    EXPECT_EQ(cell(result, 0, 0), "Ada");
    EXPECT_EQ(cell(result, 1, 1), "12");
    EXPECT_EQ(result.rowCount, 2);
    EXPECT_EQ(result.columnCount, 2);
}

TEST(CsvParserStandaloneTest, HeaderlessModeKeepsTheFirstRecordAsData)
{
    CsvParseOptions options;
    options.hasHeader = false;
    const auto result = CsvParser::parseString("a,b\nc,d", options);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.headers.empty());
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(cell(result, 0, 0), "a");
    EXPECT_EQ(cell(result, 1, 1), "d");
    EXPECT_EQ(result.columnCount, 2);
}

TEST(CsvParserStandaloneTest, ExplicitCommaDelimiterParsesFields)
{
    CsvParseOptions options;
    options.delimiter = CsvDelimiter::Comma;
    const auto result = CsvParser::parseString("a,b\n1,2", options);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 0), "1");
    EXPECT_EQ(cell(result, 0, 1), "2");
}

TEST(CsvParserStandaloneTest, ExplicitSemicolonTabAndPipeDelimitersParse)
{
    const struct Case { CsvDelimiter delimiter; std::string_view input; } cases[] = {
        {CsvDelimiter::Semicolon, "a;b\n1;2"},
        {CsvDelimiter::Tab, "a\tb\n1\t2"},
        {CsvDelimiter::Pipe, "a|b\n1|2"},
    };
    for (const auto& test : cases) {
        CsvParseOptions options;
        options.delimiter = test.delimiter;
        const auto result = CsvParser::parseString(test.input, options);
        ASSERT_TRUE(result.ok());
        ASSERT_EQ(result.rows.size(), 1);
        EXPECT_EQ(cell(result, 0, 0), "1");
        EXPECT_EQ(cell(result, 0, 1), "2");
    }
}

TEST(CsvParserStandaloneTest, AutomaticDelimiterDetectionRecognizesSupportedSeparators)
{
    const std::string_view inputs[] = {
        "a,b\n1,2", "a;b\n1;2", "a\tb\n1\t2", "a|b\n1|2"};
    for (const auto input : inputs) {
        const auto result = CsvParser::parseString(input);
        ASSERT_TRUE(result.ok());
        ASSERT_EQ(result.rows.size(), 1);
        EXPECT_EQ(result.columnCount, 2);
        EXPECT_EQ(cell(result, 0, 1), "2");
    }
}

TEST(CsvParserStandaloneTest, ExplicitDelimiterOverridesAutomaticDetection)
{
    CsvParseOptions options;
    options.delimiter = CsvDelimiter::Pipe;
    const auto result = CsvParser::parseString("a,b|c\n1,2|3", options);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.headers.size(), 2);
    EXPECT_EQ(toStdString(result.headers[0]), "a,b");
    EXPECT_EQ(toStdString(result.headers[1]), "c");
}

TEST(CsvParserStandaloneTest, DelimitersInsideQuotedFieldsArePreserved)
{
    const auto result = CsvParser::parseString("name,note\nAda,\"writer, mathematician\"");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 1), "writer, mathematician");
}

TEST(CsvParserStandaloneTest, DoubledQuoteEscapesQuoteInsideQuotedField)
{
    const auto result = CsvParser::parseString(R"(name,note
Ada,"said ""hello""")");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 1), "said \"hello\"");
}

TEST(CsvParserStandaloneTest, QuotedFieldsMayContainNewlines)
{
    const auto result = CsvParser::parseString("name,note\nAda,\"first line\nsecond line\"");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(cell(result, 0, 1), "first line\nsecond line");
}

TEST(CsvParserStandaloneTest, CustomQuoteCharacterIsSupported)
{
    CsvParseOptions options;
    options.quoteChar = '\'';
    const auto result = CsvParser::parseString("name,note\nAda,'has, comma'", options);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 1), "has, comma");
}

TEST(CsvParserStandaloneTest, CustomEscapeCharacterEscapesNextQuotedCharacter)
{
    CsvParseOptions options;
    options.escapeChar = '\\';
    const auto result = CsvParser::parseString(R"(name,note
Ada,"say \"hi\"")", options);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 1), "say \"hi\"");
}

TEST(CsvParserStandaloneTest, TrimmingRemovesSurroundingWhitespaceByDefault)
{
    const auto result = CsvParser::parseString(" name , value \n Ada , 42 ");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(toStdString(result.headers[0]), "name");
    EXPECT_EQ(cell(result, 0, 0), "Ada");
    EXPECT_EQ(cell(result, 0, 1), "42");
}

TEST(CsvParserStandaloneTest, TrimmingCanBeDisabled)
{
    CsvParseOptions options;
    options.trimWhitespace = false;
    const auto result = CsvParser::parseString("name,value\n Ada , 42 ", options);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 0), " Ada ");
    EXPECT_EQ(cell(result, 0, 1), " 42 ");
}

TEST(CsvParserStandaloneTest, EmptyPhysicalLinesAreIgnored)
{
    const auto result = CsvParser::parseString("a,b\n\n1,2\n\n");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rowCount, 1);
}

TEST(CsvParserStandaloneTest, SingleEmptyFieldRowIsSkippedByDefault)
{
    const auto result = CsvParser::parseString("\"\"");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, CsvParseError::EmptyFile);
}

TEST(CsvParserStandaloneTest, SingleEmptyFieldCanBeRetainedWhenSkippingIsDisabled)
{
    CsvParseOptions options;
    options.hasHeader = false;
    options.skipEmptyRows = false;
    const auto result = CsvParser::parseString("\"\"\nvalue", options);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(cell(result, 0, 0), "");
    EXPECT_EQ(cell(result, 1, 0), "value");
}

TEST(CsvParserStandaloneTest, InconsistentColumnCountReturnsSpecificError)
{
    const auto result = CsvParser::parseString("a,b\n1,2\n3");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, CsvParseError::InconsistentColumnCount);
    EXPECT_NE(result.errorMessage.find("expected 2"), std::string::npos);
}

TEST(CsvParserStandaloneTest, SingleColumnAndSingleRowInputIsValid)
{
    CsvParseOptions options;
    options.hasHeader = false;
    const auto result = CsvParser::parseString("only", options);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.rowCount, 1);
    EXPECT_EQ(result.columnCount, 1);
    EXPECT_EQ(cell(result, 0, 0), "only");
}

TEST(CsvParserStandaloneTest, TrailingNewlineDoesNotCreateExtraRecord)
{
    CsvParseOptions options;
    options.hasHeader = false;
    const auto result = CsvParser::parseString("a,b\n", options);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rowCount, 1);
}

TEST(CsvParserStandaloneTest, DelimiterAndErrorEnumsHaveStableTextMappings)
{
    EXPECT_EQ(csvDelimiterToChar(CsvDelimiter::Comma), ',');
    EXPECT_EQ(csvDelimiterToChar(CsvDelimiter::Semicolon), ';');
    EXPECT_EQ(csvDelimiterToChar(CsvDelimiter::Tab), '\t');
    EXPECT_EQ(csvDelimiterToChar(CsvDelimiter::Pipe), '|');
    EXPECT_EQ(csvDelimiterToChar(CsvDelimiter::Auto), ',');
    EXPECT_STREQ(csvParseErrorToString(CsvParseError::None), "None");
    EXPECT_STREQ(csvParseErrorToString(CsvParseError::InconsistentColumnCount),
                 "InconsistentColumnCount");
    EXPECT_STREQ(csvParseErrorToString(static_cast<CsvParseError>(999)), "Unknown");
}

TEST(CsvParserStandaloneTest, HeaderOnlyInputIsValidAndReportsItsColumns)
{
    const auto result = CsvParser::parseString("first,second,third");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.headers.size(), 3);
    EXPECT_TRUE(result.rows.empty());
    EXPECT_EQ(result.rowCount, 0);
    EXPECT_EQ(result.columnCount, 3);
}

TEST(CsvParserStandaloneTest, AutomaticDetectionIgnoresSeparatorsInsideQuotes)
{
    const auto result = CsvParser::parseString("name,note\nAda,\"one;two,three\"");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.columnCount, 2);
    EXPECT_EQ(cell(result, 0, 1), "one;two,three");
}

TEST(CsvParserStandaloneTest, EmptyInteriorCellsKeepTheirColumnPositions)
{
    const auto result = CsvParser::parseString("a,b,c\n1,,3");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.columnCount, 3);
    EXPECT_EQ(cell(result, 0, 0), "1");
    EXPECT_EQ(cell(result, 0, 1), "");
    EXPECT_EQ(cell(result, 0, 2), "3");
}

TEST(CsvParserStandaloneTest, TrailingDelimiterProducesAnEmptyLastCell)
{
    const auto result = CsvParser::parseString("a,b,\n1,2,");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.columnCount, 3);
    EXPECT_EQ(cell(result, 0, 2), "");
}

TEST(CsvParserStandaloneTest, CarriageReturnOnlySeparatesRecords)
{
    CsvParseOptions options;
    options.hasHeader = false;
    const auto result = CsvParser::parseString("a,b\r1,2\r3,4", options);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.rows.size(), 3);
    EXPECT_EQ(cell(result, 1, 0), "1");
    EXPECT_EQ(cell(result, 2, 1), "4");
}

TEST(CsvParserStandaloneTest, TrimmingAlsoAppliesToQuotedFieldEdges)
{
    const auto result = CsvParser::parseString("name\n\"  Ada Lovelace  \"");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(cell(result, 0, 0), "Ada Lovelace");
}

TEST(CsvParserStandaloneTest, EqualDelimiterCountsPreferComma)
{
    EXPECT_EQ(CsvParser::detectDelimiter("a,b;c"), ',');
    const auto result = CsvParser::parseString("a,b;c\n1,2;3");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.columnCount, 2);
    EXPECT_EQ(cell(result, 0, 1), "2;3");
}

TEST(CsvParserStandaloneTest, DelimiterDetectionDefaultsToCommaWhenOnlyQuotedSeparatorsExist)
{
    EXPECT_EQ(CsvParser::detectDelimiter("\"a;b|c\""), ',');
}
