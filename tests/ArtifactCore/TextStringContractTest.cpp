#include <gtest/gtest.h>

#include <cstddef>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

import Utils.Text.String;

using namespace ArtifactCore;

namespace {
String joinParts(const std::vector<std::string_view>& parts, char separator)
{
    return join(std::span<const std::string_view>(parts.data(), parts.size()), separator);
}
}

TEST(TextStringContractTest, DetectsAllAsciiWhitespaceCharacters)
{
    constexpr char whitespace[] = {' ', '\t', '\n', '\r', '\f', '\v'};
    for (const char character : whitespace) {
        EXPECT_TRUE(isAsciiWhitespace(character));
    }
    EXPECT_FALSE(isAsciiWhitespace('x'));
    EXPECT_FALSE(isAsciiWhitespace('\0'));
}

TEST(TextStringContractTest, TrimsWhitespaceFromBothEndsAndPreservesInterior)
{
    EXPECT_EQ(trimView(" \tvalue with space\r\n"), "value with space");
}

TEST(TextStringContractTest, TrimsAllWhitespaceAndLeavesEmptyInputEmpty)
{
    EXPECT_TRUE(trimView(" \t\r\n\f\v").empty());
    EXPECT_TRUE(trimView("").empty());
}

TEST(TextStringContractTest, TrimmingUsesAsciiWhitespaceOnly)
{
    constexpr std::string_view nbsp("\xc2\xa0", 2);
    EXPECT_EQ(trimView(nbsp), nbsp);
}

TEST(TextStringContractTest, TrimViewPreservesEmbeddedNulBytes)
{
    constexpr std::string_view input(" \0x\0 ", 5);
    const auto trimmed = trimView(input);
    ASSERT_EQ(trimmed.size(), 3u);
    EXPECT_EQ(trimmed[0], '\0');
    EXPECT_EQ(trimmed[1], 'x');
    EXPECT_EQ(trimmed[2], '\0');
}

TEST(TextStringContractTest, TrimViewAliasesTheTrimmedSubrange)
{
    constexpr std::string_view source = " \tasset name\r\n";

    const auto trimmed = trimView(source);

    EXPECT_EQ(trimmed.data(), source.data() + 2);
    EXPECT_EQ(trimmed.size(), 10u);
    EXPECT_EQ(trimmed, "asset name");
}

TEST(TextStringContractTest, PrefixAndSuffixMatchExactByteSequences)
{
    EXPECT_TRUE(startsWith("artifact.core", "artifact"));
    EXPECT_TRUE(endsWith("artifact.core", "core"));
    EXPECT_FALSE(startsWith("artifact.core", "Artifact"));
    EXPECT_FALSE(endsWith("artifact.core", "CORE"));
}

TEST(TextStringContractTest, EmptyPrefixAndSuffixMatchEveryValue)
{
    EXPECT_TRUE(startsWith("value", ""));
    EXPECT_TRUE(endsWith("value", ""));
    EXPECT_TRUE(startsWith("", ""));
    EXPECT_TRUE(endsWith("", ""));
}

TEST(TextStringContractTest, LongerPrefixOrSuffixDoesNotMatch)
{
    EXPECT_FALSE(startsWith("a", "asset"));
    EXPECT_FALSE(endsWith("a", "asset"));
}

TEST(TextStringContractTest, SplitPreservesLeadingTrailingAndRepeatedEmptyFields)
{
    const auto parts = splitView(",a,,b,", ',');
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_EQ(parts[0], "");
    EXPECT_EQ(parts[1], "a");
    EXPECT_EQ(parts[2], "");
    EXPECT_EQ(parts[3], "b");
    EXPECT_EQ(parts[4], "");
}

TEST(TextStringContractTest, SplittingEmptyAndUnseparatedInputReturnsOnePart)
{
    const auto empty = splitView("", ',');
    const auto single = splitView("asset", ',');
    ASSERT_EQ(empty.size(), 1u);
    EXPECT_TRUE(empty[0].empty());
    ASSERT_EQ(single.size(), 1u);
    EXPECT_EQ(single[0], "asset");
}

TEST(TextStringContractTest, SplitCanUseNulAsASeparator)
{
    constexpr std::string_view input("left\0right", 10);
    const auto parts = splitView(input, '\0');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "left");
    EXPECT_EQ(parts[1], "right");
}

TEST(TextStringContractTest, JoinEmptySingleAndMultipleParts)
{
    const std::vector<std::string_view> empty;
    const std::vector<std::string_view> one{"solo"};
    const std::vector<std::string_view> many{"red", "", "blue"};
    EXPECT_EQ(joinParts(empty, '|'), "");
    EXPECT_EQ(joinParts(one, '|'), "solo");
    EXPECT_EQ(joinParts(many, '|'), "red||blue");
}

TEST(TextStringContractTest, JoinSupportsNulSeparatorAndEmbeddedNulData)
{
    const std::vector<std::string_view> parts{std::string_view("a\0b", 3), "c"};
    const String joined = joinParts(parts, '\0');
    EXPECT_EQ(joined.length(), 5u);
    EXPECT_EQ(toStdString(joined), std::string("a\0b\0c", 5));
}

TEST(TextStringContractTest, SplitAndJoinRoundTripPreservesAllFields)
{
    constexpr std::string_view source("first,,third,", 13);
    const auto parts = splitView(source, ',');
    EXPECT_EQ(joinParts(parts, ','), source);
}

TEST(TextStringContractTest, SplitJoinRoundTripsEveryShortDelimiterAndNulPattern)
{
    constexpr char alphabet[] = {'a', ',', '\0'};
    constexpr char separators[] = {',', '\0'};
    std::size_t combinations = 1;

    for (std::size_t length = 0; length <= 6; ++length) {
        for (std::size_t encoded = 0; encoded < combinations; ++encoded) {
            std::string source(length, '\0');
            std::size_t remaining = encoded;
            for (char& character : source) {
                character = alphabet[remaining % std::size(alphabet)];
                remaining /= std::size(alphabet);
            }

            for (const char separator : separators) {
                const auto fields = splitView(std::string_view(source), separator);
                SCOPED_TRACE(::testing::Message()
                    << "length=" << length << " encoded=" << encoded
                    << " separator=" << static_cast<int>(separator));
                EXPECT_EQ(toStdString(joinParts(fields, separator)), source);
            }
        }
        combinations *= std::size(alphabet);
    }
}

TEST(TextStringContractTest, SplitViewsReferToOriginalInputStorage)
{
    const std::string_view source = "alpha,beta";
    const auto parts = splitView(source, ',');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0].data(), source.data());
    EXPECT_EQ(parts[1].data(), source.data() + 6);
}
