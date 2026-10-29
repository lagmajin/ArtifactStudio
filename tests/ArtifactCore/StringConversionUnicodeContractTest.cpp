#include <gtest/gtest.h>

#include <QString>

#include <string>

import Utils.Convertor.String;

using ArtifactCore::toStdString;
using ArtifactCore::toStdU16String;
using ArtifactCore::toStdU32String;

TEST(StringConversionUnicodeContractTest, EmptyStringProducesEmptyStandardStrings)
{
    const QString input;
    EXPECT_TRUE(toStdString(input).empty());
    EXPECT_TRUE(toStdU16String(input).empty());
    EXPECT_TRUE(toStdU32String(input).empty());
}

TEST(StringConversionUnicodeContractTest, Utf8ConversionPreservesMultibyteAndSupplementaryCharacters)
{
    const QString input = QString::fromUtf8("Aé漢😀");
    const std::string converted = toStdString(input);

    EXPECT_EQ(converted, std::string("Aé漢😀"));
    EXPECT_EQ(converted.size(), 10u);
}

TEST(StringConversionUnicodeContractTest, Utf16ConversionPreservesQtCodeUnits)
{
    const QString input = QString::fromUtf8("Aé漢😀");
    const std::u16string converted = toStdU16String(input);

    ASSERT_EQ(converted.size(), 5u);
    EXPECT_EQ(converted[0], u'A');
    EXPECT_EQ(converted[1], u'é');
    EXPECT_EQ(converted[2], u'漢');
    EXPECT_EQ(converted[3], static_cast<char16_t>(0xD83D));
    EXPECT_EQ(converted[4], static_cast<char16_t>(0xDE00));
}

TEST(StringConversionUnicodeContractTest, Utf32ConversionCombinesSupplementarySurrogatePair)
{
    const QString input = QString::fromUtf8("Aé漢😀");
    const std::u32string converted = toStdU32String(input);

    ASSERT_EQ(converted.size(), 4u);
    EXPECT_EQ(converted[0], U'A');
    EXPECT_EQ(converted[1], U'é');
    EXPECT_EQ(converted[2], U'漢');
    EXPECT_EQ(converted[3], static_cast<char32_t>(0x1F600));
}

TEST(StringConversionUnicodeContractTest, Utf32LengthCountsCodePointsRatherThanUtf16Units)
{
    const QString input = QString::fromUtf8("😀😀");
    EXPECT_EQ(input.size(), 4);
    EXPECT_EQ(toStdU16String(input).size(), 4u);
    EXPECT_EQ(toStdU32String(input).size(), 2u);
}
