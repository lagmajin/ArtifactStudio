#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import Core.ArtifactString;

using namespace ArtifactCore;

namespace {

std::vector<std::string> toStrings(const Array<String>& values)
{
    std::vector<std::string> result;
    for (const String& value : values) result.push_back(toStdString(value));
    return result;
}

} // namespace

TEST(ArtifactStringContractTest, DefaultNullAndCharacterConstruction)
{
    const String empty;
    const String fromNull(static_cast<const char*>(nullptr));
    const String one('x');
    EXPECT_TRUE(empty.isEmpty());
    EXPECT_TRUE(fromNull.isEmpty());
    EXPECT_EQ(empty.capacity(), String::kSmallCapacity);
    EXPECT_EQ(one.length(), 1u);
    EXPECT_EQ(one.at(0), 'x');
    EXPECT_EQ(one.at(1), '\0');
}

TEST(ArtifactStringContractTest, SmallBufferBoundaryKeepsInlineCapacityUntilExceeded)
{
    const String inlineString(std::string(String::kSmallCapacity, 'a'));
    const String heapString(std::string(String::kSmallCapacity + 1, 'b'));
    EXPECT_EQ(inlineString.length(), String::kSmallCapacity);
    EXPECT_EQ(inlineString.capacity(), String::kSmallCapacity);
    EXPECT_GT(heapString.capacity(), String::kSmallCapacity);
    EXPECT_EQ(heapString.length(), String::kSmallCapacity + 1);
}

TEST(ArtifactStringContractTest, CopyAndMovePreserveContentsAcrossStorageModes)
{
    String small("short");
    String smallCopy(small);
    String smallMoved(std::move(smallCopy));
    EXPECT_EQ(smallMoved, "short");
    EXPECT_TRUE(smallCopy.isEmpty());

    String large(std::string(64, 'L'));
    String largeCopy(large);
    String destination("old");
    destination = std::move(largeCopy);
    EXPECT_EQ(destination.length(), 64u);
    EXPECT_EQ(largeCopy.length(), 0u);
    EXPECT_EQ(large.length(), 64u);
}

TEST(ArtifactStringContractTest, AppendSupportsSelfAndInteriorAliasesAcrossGrowth)
{
    String small("short");
    small.append(small.data(), small.length());
    EXPECT_EQ(small, "shortshort");

    String large(std::string(20, 'x'));
    large.append(large.data(), large.length());
    EXPECT_EQ(large.length(), 40u);
    EXPECT_EQ(large.substr(0, 20), String("xxxxxxxxxxxxxxxxxxxx"));
    EXPECT_EQ(large.substr(20, 20), String("xxxxxxxxxxxxxxxxxxxx"));

    String interior("abcdef");
    interior.append(interior.data() + 1, 3);
    EXPECT_EQ(interior, "abcdefbcd");
}

TEST(ArtifactStringContractTest, AppendOverloadsAndClearMaintainValidTerminator)
{
    String value("a");
    value += "b";
    value += std::string("c");
    value += std::string_view("d");
    value += StringView("ef");
    value += 'g';
    EXPECT_EQ(value, "abcdefg");
    EXPECT_EQ(value.data()[value.length()], '\0');
    value.clear();
    EXPECT_TRUE(value.isEmpty());
    EXPECT_EQ(value.data()[0], '\0');
}

TEST(ArtifactStringContractTest, AppendNullPointerIsNoOpAndPreservesTerminator)
{
    String value("payload");

    value.append(nullptr, 4);

    EXPECT_EQ(value, "payload");
    EXPECT_EQ(value.data()[value.length()], '\0');
}

TEST(ArtifactStringContractTest, ExplicitLengthConstructionAndAppendPreserveEmbeddedNull)
{
    constexpr char bytes[] = {'a', '\0', 'b'};
    String value(bytes, sizeof(bytes));
    value.append("!", 1);
    EXPECT_EQ(value.length(), 4u);
    EXPECT_EQ(value.at(0), 'a');
    EXPECT_EQ(value.at(1), '\0');
    EXPECT_EQ(value.at(2), 'b');
    EXPECT_EQ(value.at(3), '!');
    EXPECT_EQ(toStdString(value), std::string("a\0b!", 4));
    EXPECT_TRUE(value.contains('\0'));
    EXPECT_EQ(String(value), value);
}

TEST(ArtifactStringContractTest, ExplicitLengthSearchFindsPatternsAcrossEmbeddedNullBytes)
{
    const String value(std::string_view("a\0b\0a", 5));
    constexpr std::string_view nullAndB("\0b", 2);

    EXPECT_EQ(value.indexOf(nullAndB), 1);
    EXPECT_EQ(value.indexOf(nullAndB, 2), -1);
    EXPECT_TRUE(value.contains(nullAndB));
    EXPECT_EQ(value.count('\0'), 2);
    EXPECT_EQ(value.lastIndexOf('\0'), 3);
}

TEST(ArtifactStringContractTest, MidLeftRightAndSubstrClampAtBounds)
{
    const String value("abcdef");
    EXPECT_EQ(value.substr(2, 3), "cde");
    EXPECT_EQ(value.substr(4, 99), "ef");
    EXPECT_TRUE(value.substr(value.length()).isEmpty());
    EXPECT_EQ(toStdString(value.mid(1, 2)), "bc");
    EXPECT_EQ(toStdString(value.left(3)), "abc");
    EXPECT_EQ(toStdString(value.right(2)), "ef");
    EXPECT_EQ(toStdString(value.right(99)), "abcdef");
    EXPECT_TRUE(value.left(0).isEmpty());
}

TEST(ArtifactStringContractTest, MultibyteUtf8OperationsUseByteOffsets)
{
    constexpr std::string_view firstCharacter("\xE6\x9D\xB1", 3);
    constexpr std::string_view secondCharacter("\xE4\xBA\xAC", 3);
    const String value(std::string_view("\xE6\x9D\xB1\xE4\xBA\xAC", 6));

    EXPECT_EQ(value.length(), 6u);
    EXPECT_EQ(value.indexOf(secondCharacter), 3);
    EXPECT_EQ(toStdString(value.substr(0, 3)), std::string(firstCharacter));
    EXPECT_EQ(toStdString(value.substr(3, 3)), std::string(secondCharacter));
    EXPECT_EQ(toStdString(value),
              std::string(firstCharacter) + std::string(secondCharacter));
}

TEST(ArtifactStringContractTest, SearchHonorsStartOffsetAndMissingResults)
{
    const String value("ababa");
    EXPECT_EQ(value.indexOf('a'), 0);
    EXPECT_EQ(value.indexOf('a', 1), 2);
    EXPECT_EQ(value.indexOf("ba"), 1);
    EXPECT_EQ(value.indexOf(std::string_view("ba"), 2), 3);
    EXPECT_EQ(value.lastIndexOf('a'), 4);
    EXPECT_EQ(value.find('z'), static_cast<size_t>(-1));
    EXPECT_EQ(value.find("missing"), static_cast<size_t>(-1));
    EXPECT_TRUE(value.contains("aba"));
    EXPECT_EQ(value.count('a'), 3);
}

TEST(ArtifactStringContractTest, PrefixSuffixAndOrderingUseBytewiseContents)
{
    const String value("artifact");
    EXPECT_TRUE(value.startsWith("art"));
    EXPECT_TRUE(value.endsWith(std::string_view("fact")));
    EXPECT_FALSE(value.startsWith("fact"));
    EXPECT_FALSE(value.endsWith("art"));
    EXPECT_LT(String("alpha"), String("beta"));
    EXPECT_NE(String("same"), String("different"));
}

TEST(ArtifactStringContractTest, ReplaceClampsRemovedLengthAndIgnoresOutOfRangePosition)
{
    String value("abcdef");
    value.replace(2, 2, "XY");
    EXPECT_EQ(value, "abXYef");
    value.replace(4, 99, std::string_view("!"));
    EXPECT_EQ(value, "abXY!");
    value.replace(99, 1, "ignored");
    EXPECT_EQ(value, "abXY!");
    value.replace(0, 2, std::string(""));
    EXPECT_EQ(value, "XY!");
}

TEST(ArtifactStringContractTest, NullReplacementPointerDeletesSelectedRange)
{
    String value("abcdef");

    value.replace(2, 3, static_cast<const char*>(nullptr));

    EXPECT_EQ(value, "abf");
    EXPECT_EQ(value.data()[value.length()], '\0');
}

TEST(ArtifactStringContractTest, NumericConversionReportsSuccessAndInvalidInput)
{
    bool integerOk = false;
    bool doubleOk = false;
    EXPECT_EQ(String("-42tail").toInt(&integerOk), -42);
    EXPECT_TRUE(integerOk);
    EXPECT_DOUBLE_EQ(String("3.25rest").toDouble(&doubleOk), 3.25);
    EXPECT_TRUE(doubleOk);
    EXPECT_EQ(String("not-number").toInt(&integerOk), 0);
    EXPECT_FALSE(integerOk);
    EXPECT_DOUBLE_EQ(String("invalid").toDouble(&doubleOk), 0.0);
    EXPECT_FALSE(doubleOk);
}

TEST(ArtifactStringContractTest, NumberFormattingUsesRequestedPrecision)
{
    EXPECT_EQ(String::number(-23), "-23");
    EXPECT_EQ(String::number(1.26, 1), "1.3");
    EXPECT_EQ(String::number(1.25, 3), "1.250");
    EXPECT_EQ(String::number(1.0, -2), "1");
}

TEST(ArtifactStringContractTest, AsciiCaseAndTrimHelpersDoNotDependOnLocale)
{
    EXPECT_EQ(asciiLower(StringView("MiXeD")), String("mixed"));
    EXPECT_EQ(asciiUpper(StringView("MiXeD")), String("MIXED"));
    EXPECT_EQ(asciiTrimmed(StringView(" \tvalue\r\n")), String("value"));
    EXPECT_EQ(asciiTrimmed(StringView("\v\f")), String(""));
    EXPECT_TRUE(asciiIsSpace('\v'));
    EXPECT_FALSE(asciiIsSpace('\0'));
    EXPECT_EQ(asciiLowerChar('A'), 'a');
    EXPECT_EQ(asciiUpperChar('z'), 'Z');
}

TEST(ArtifactStringContractTest, AsciiPrefixSuffixHelpersHandleEmptyAndOversizedInputs)
{
    const StringView value("artifact");
    EXPECT_TRUE(asciiStartsWith(value, StringView("art")));
    EXPECT_TRUE(asciiEndsWith(value, StringView("fact")));
    EXPECT_TRUE(asciiStartsWith(value, StringView("")));
    EXPECT_TRUE(asciiEndsWith(value, StringView("")));
    EXPECT_FALSE(asciiStartsWith(value, StringView("artifacts")));
    EXPECT_FALSE(asciiEndsWith(value, StringView("artifacts")));
}

TEST(ArtifactStringContractTest, AsciiSplitPreservesEmptySegmentsIncludingEmptyInput)
{
    EXPECT_EQ(toStrings(asciiSplit(StringView("a,,b,"), ',')),
              (std::vector<std::string>{"a", "", "b", ""}));
    EXPECT_EQ(toStrings(asciiSplit(StringView(""), ',')),
              (std::vector<std::string>{""}));
}

TEST(ArtifactStringContractTest, AsciiJoinUsesSeparatorBetweenPartsOnly)
{
    Array<String> parts{"a", "", "b"};
    EXPECT_EQ(asciiJoin(parts, StringView("::")), "a::::b");
    EXPECT_EQ(asciiJoin(Array<String>{}, StringView("::")), "");
    EXPECT_EQ(asciiJoin(Array<String>{"only"}, StringView("::")), "only");
}

TEST(ArtifactStringContractTest, AsciiSplitJoinRoundTripsShortDelimiterAndNulPatterns)
{
    constexpr std::array<char, 3> alphabet{'a', ',', '\0'};
    constexpr std::array<char, 2> separators{',', '\0'};
    std::size_t combinations = 1;

    for (std::size_t length = 0; length <= 6; ++length) {
        for (std::size_t encoded = 0; encoded < combinations; ++encoded) {
            std::string source(length, '\0');
            std::size_t remaining = encoded;
            for (char& character : source) {
                character = alphabet[remaining % alphabet.size()];
                remaining /= alphabet.size();
            }

            for (const char separator : separators) {
                const auto fields = asciiSplit(
                    StringView(source.data(), source.size()), separator);
                const StringView separatorView(&separator, 1);
                SCOPED_TRACE(::testing::Message()
                    << "length=" << length << " encoded=" << encoded
                    << " separator=" << static_cast<int>(separator));
                EXPECT_EQ(toStdString(asciiJoin(fields, separatorView)), source);
            }
        }
        combinations *= alphabet.size();
    }
}

TEST(ArtifactStringContractTest, StringViewExplicitLengthPreservesNullBytes)
{
    constexpr char bytes[] = {'a', '\0', 'b'};
    const StringView view(bytes, sizeof(bytes));
    EXPECT_EQ(view.length(), 3u);
    EXPECT_EQ(view.at(0), 'a');
    EXPECT_EQ(view.at(1), '\0');
    EXPECT_EQ(view.at(2), 'b');
    EXPECT_EQ(view.at(3), '\0');
    EXPECT_EQ(view, StringView(bytes, sizeof(bytes)));
    EXPECT_NE(view, StringView("a"));
}

TEST(ArtifactStringContractTest, AssignmentAndSelfAssignmentPreserveStorageInvariants)
{
    String value(std::string(64, 'x'));
    value = String("small");
    EXPECT_EQ(value, "small");
    EXPECT_EQ(value.capacity(), String::kSmallCapacity);

    value = std::string_view("a longer replacement");
    EXPECT_EQ(value, "a longer replacement");
    value = value;
    EXPECT_EQ(value, "a longer replacement");
    value = std::move(value);
    EXPECT_EQ(value, "a longer replacement");
}

TEST(ArtifactStringContractTest, ReplaceCanReadReplacementBytesFromSameString)
{
    String value("abcdef");
    value.replace(1, 2, std::string_view(value.data() + 3, 2));
    EXPECT_EQ(value, "adedef");
}

TEST(ArtifactStringContractTest, ReplaceCanGrowFromAnAliasedInteriorView)
{
    String value("abcd");
    const std::string_view replacement(value.data() + 1, 3);

    value.replace(1, 1, replacement);

    EXPECT_EQ(value, "abcdcd");
    EXPECT_EQ(value.length(), 6u);
    EXPECT_EQ(value.data()[value.length()], '\0');
}

TEST(ArtifactStringContractTest, AppendStringViewCanAliasAcrossInlineGrowth)
{
    String value("0123456789abcdef");
    value.append(StringView(value.data(), value.length()));
    EXPECT_EQ(value, "0123456789abcdef0123456789abcdef");
    EXPECT_EQ(value.data()[value.length()], '\0');
}

TEST(ArtifactStringContractTest, EmptyAndZeroLengthViewsHaveConsistentBounds)
{
    const String empty;
    EXPECT_TRUE(empty.mid(0).isEmpty());
    EXPECT_TRUE(empty.left(5).isEmpty());
    EXPECT_TRUE(empty.right(5).isEmpty());
    const String value("abc");
    EXPECT_TRUE(value.right(0).isEmpty());
    EXPECT_EQ(toStdString(value.right(0)), "");
}

TEST(ArtifactStringContractTest, AsciiCaseHelpersLeaveHighBitBytesUnchanged)
{
    constexpr char bytes[] = {static_cast<char>(0xC3), static_cast<char>(0x89), 'A'};
    constexpr char lowerExpected[] = {static_cast<char>(0xC3), static_cast<char>(0x89), 'a'};
    constexpr char upperExpected[] = {static_cast<char>(0xC3), static_cast<char>(0x89), 'A'};
    EXPECT_EQ(asciiLower(StringView(bytes, sizeof(bytes))),
              String(lowerExpected, sizeof(lowerExpected)));
    EXPECT_EQ(asciiUpper(StringView(bytes, sizeof(bytes))),
              String(upperExpected, sizeof(upperExpected)));
}

