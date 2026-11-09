#include <gtest/gtest.h>

#include <QString>

#include <string>

import Utils.Convertor.String;
import Utils.String;

using ArtifactCore::toStdString;
using ArtifactCore::toStdU16String;
using ArtifactCore::toStdU32String;
using ArtifactCore::toQString;

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

TEST(StringConversionUnicodeContractTest, InvalidSurrogatesBecomeUnicodeReplacementCharacters)
{
    const QChar codeUnits[] = {
        QChar(static_cast<ushort>(0xD800)), // Unpaired high surrogate.
        QChar(u'A'),
        QChar(static_cast<ushort>(0xDC00)), // Unpaired low surrogate.
    };
    const QString input(codeUnits, 3);

    const std::string utf8 = toStdString(input);
    EXPECT_EQ(utf8, std::string("\xEF\xBF\xBD" "A" "\xEF\xBF\xBD"));

    const std::u32string utf32 = toStdU32String(input);
    ASSERT_EQ(utf32.size(), 3u);
    EXPECT_EQ(utf32[0], static_cast<char32_t>(0xFFFD));
    EXPECT_EQ(utf32[1], U'A');
    EXPECT_EQ(utf32[2], static_cast<char32_t>(0xFFFD));
}

TEST(StringConversionUnicodeContractTest, PreservesEmbeddedNullAcrossUtf8Utf16AndUtf32)
{
    const QChar codeUnits[] = {QChar(u'A'), QChar(u'\0'), QChar(u'B')};
    const QString input(codeUnits, 3);

    const std::string utf8 = toStdString(input);
    ASSERT_EQ(utf8.size(), 3u);
    EXPECT_EQ(utf8[0], 'A');
    EXPECT_EQ(utf8[1], '\0');
    EXPECT_EQ(utf8[2], 'B');

    const std::u16string utf16 = toStdU16String(input);
    ASSERT_EQ(utf16.size(), 3u);
    EXPECT_EQ(utf16[0], u'A');
    EXPECT_EQ(utf16[1], u'\0');
    EXPECT_EQ(utf16[2], u'B');

    const std::u32string utf32 = toStdU32String(input);
    ASSERT_EQ(utf32.size(), 3u);
    EXPECT_EQ(utf32[0], U'A');
    EXPECT_EQ(utf32[1], U'\0');
    EXPECT_EQ(utf32[2], U'B');
}

TEST(StringConversionUnicodeContractTest, Utf8InputConversionPreservesEmbeddedNull)
{
    const std::string input("A\0B", 3);
    const QString converted = toQString(input);

    ASSERT_EQ(converted.size(), 3);
    EXPECT_EQ(converted[0], QChar(u'A'));
    EXPECT_EQ(converted[1], QChar(u'\0'));
    EXPECT_EQ(converted[2], QChar(u'B'));
}

TEST(StringConversionUnicodeContractTest, InvalidUtf8BytesBecomeReplacementCharacters)
{
    const std::string input("A\x80" "B", 3);
    const QString converted = toQString(input);

    EXPECT_EQ(converted, QString::fromUtf8("A\xEF\xBF\xBD" "B"));
    EXPECT_EQ(toStdString(converted), std::string("A\xEF\xBF\xBD" "B"));
}
