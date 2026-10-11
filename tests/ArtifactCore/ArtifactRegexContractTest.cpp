#include <gtest/gtest.h>

#include <string>
#include <string_view>

import Core.ArtifactRegex;

using namespace ArtifactCore;

TEST(ArtifactRegexContractTest, DefaultConstructionIsInvalid)
{
    const ArtifactRegex regex;
    EXPECT_FALSE(regex.isValid());
    EXPECT_EQ(regex.errorCode(), RegexErrorCode::EmptyPattern);
    EXPECT_FALSE(regex.matches("anything"));
}

TEST(ArtifactRegexContractTest, EmptyPatternReportsCompileError)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex regex;
    EXPECT_FALSE(regex.compile("", false, &error));
    EXPECT_EQ(error, RegexErrorCode::EmptyPattern);
    EXPECT_EQ(regex.errorCode(), RegexErrorCode::EmptyPattern);
}

TEST(ArtifactRegexContractTest, LiteralSearchReturnsByteOffsetAndLength)
{
    ArtifactRegex regex("ab+c");
    ASSERT_TRUE(regex.isValid());
    const auto match = regex.search("--abbbc--");
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 2u);
    EXPECT_EQ(match.groups[0].length, 5u);
}

TEST(ArtifactRegexContractTest, SearchOffsetsCountUtf8InputAsBytes)
{
    constexpr std::string_view input("あx");
    const auto match = ArtifactRegex("x").search(StringView(input.data(), input.size()));
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 3u);
    EXPECT_EQ(match.groups[0].length, 1u);
}

TEST(ArtifactRegexContractTest, MatchesSearchesForSubstringRatherThanWholeInput)
{
    const ArtifactRegex regex("cat");
    EXPECT_TRUE(regex.matches("concatenate"));
    EXPECT_FALSE(regex.matches("dog"));
}

TEST(ArtifactRegexContractTest, IgnoreCaseAppliesToLiteralMatching)
{
    const ArtifactRegex regex("AbC", true);
    EXPECT_TRUE(regex.matches("xxaBcxx"));
    EXPECT_FALSE(ArtifactRegex("AbC", false).matches("abc"));
}

TEST(ArtifactRegexContractTest, ShorthandAndExplicitCharacterClassesMatch)
{
    const ArtifactRegex regex("^[A-Z]\\d\\w$");
    EXPECT_TRUE(regex.matches("Q4_"));
    EXPECT_FALSE(regex.matches("q4_"));
    EXPECT_FALSE(regex.matches("Qx_"));
}

TEST(ArtifactRegexContractTest, NegatedAndRangedCharacterClassesWork)
{
    const ArtifactRegex regex("^[^a-c][0-2]+$");
    EXPECT_TRUE(regex.matches("z012"));
    EXPECT_FALSE(regex.matches("b12"));
    EXPECT_FALSE(regex.matches("z13"));
}

TEST(ArtifactRegexContractTest, AnchorsAndWordBoundariesUseInputEdges)
{
    const ArtifactRegex start("^word");
    const ArtifactRegex end("word$");
    const ArtifactRegex boundary("\\bcat\\b");
    EXPECT_TRUE(start.matches("word at start"));
    EXPECT_FALSE(start.matches("a word"));
    EXPECT_TRUE(end.matches("at end word"));
    EXPECT_FALSE(end.matches("word tail"));
    EXPECT_TRUE(boundary.matches("a cat!"));
    EXPECT_FALSE(boundary.matches("concatenate"));
}

TEST(ArtifactRegexContractTest, CapturesAndAlternationExposeGroupRanges)
{
    const ArtifactRegex regex("(\\w+)@(example|test)\\.com$");
    EXPECT_EQ(regex.groupCount(), 2);
    const auto match = regex.search("mail: user@example.com");
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 6u);
    EXPECT_EQ(match.groups[1].length, 4u);
    EXPECT_EQ(match.groups[2].length, 7u);
}

TEST(ArtifactRegexContractTest, UnmatchedOptionalCaptureIsDistinctFromEmptyCapture)
{
    const auto unmatched = ArtifactRegex("(a)?b").search("b");
    ASSERT_TRUE(unmatched.matched);
    EXPECT_FALSE(unmatched.groups[1].matched());
    EXPECT_EQ(unmatched.groups[1].position, regexNpos);

    const auto empty = ArtifactRegex("(a*)b").search("b");
    ASSERT_TRUE(empty.matched);
    EXPECT_TRUE(empty.groups[1].matched());
    EXPECT_EQ(empty.groups[1].position, 0u);
    EXPECT_EQ(empty.groups[1].length, 0u);
}

TEST(ArtifactRegexContractTest, FindAllReturnsNonOverlappingMatchesInOrder)
{
    const ArtifactRegex regex("aba");
    const auto matches = regex.findAll("ababa aba");
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].groups[0].position, 0u);
    EXPECT_EQ(matches[1].groups[0].position, 6u);
}

TEST(ArtifactRegexContractTest, SearchFromOffsetSkipsEarlierMatches)
{
    const ArtifactRegex regex("x");
    const auto match = regex.search("x--x", 1);
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 3u);
    EXPECT_FALSE(regex.search("x--x", 5).matched);
}

TEST(ArtifactRegexContractTest, EmbeddedNulIsMatchedAsAByteWithinExplicitLength)
{
    constexpr char bytes[] = {'a', '\0', 'b'};
    const StringView text(bytes, sizeof(bytes));
    const auto match = ArtifactRegex("b").search(text);
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].position, 2u);
}

TEST(ArtifactRegexContractTest, ReplaceAllExpandsCapturesAndEscapesDollar)
{
    const ArtifactRegex regex("(\\w+)=(\\w+)");
    const String replaced = regex.replaceAll("width=10 height=20", "$2 x $1 $$");
    EXPECT_EQ(replaced, "10 x width $ 20 x height $");
}

TEST(ArtifactRegexContractTest, ZeroWidthMatchesAdvanceDuringFindAllAndReplacement)
{
    const ArtifactRegex regex("\\b");
    const auto matches = regex.findAll("a b");
    ASSERT_EQ(matches.size(), 4u);
    EXPECT_EQ(matches[0].groups[0].position, 0u);
    EXPECT_EQ(matches[1].groups[0].position, 1u);
    EXPECT_EQ(matches[2].groups[0].position, 2u);
    EXPECT_EQ(matches[3].groups[0].position, 3u);
    EXPECT_EQ(regex.replaceAll("a b", "|"), "|a| |b|");
}

TEST(ArtifactRegexContractTest, InvalidCompileClearsPreviouslyCompiledProgram)
{
    ArtifactRegex regex("ready");
    ASSERT_TRUE(regex.isValid());
    RegexErrorCode error = RegexErrorCode::None;
    EXPECT_FALSE(regex.compile("(broken", false, &error));
    EXPECT_EQ(error, RegexErrorCode::MissingClosingParen);
    EXPECT_FALSE(regex.isValid());
    EXPECT_FALSE(regex.matches("ready"));
}

TEST(ArtifactRegexContractTest, InvalidCompileClearsPreviousCaptureCount)
{
    ArtifactRegex regex("(first)(second)");
    ASSERT_TRUE(regex.isValid());
    ASSERT_EQ(regex.groupCount(), 2);

    EXPECT_FALSE(regex.compile("(broken"));

    EXPECT_FALSE(regex.isValid());
    EXPECT_EQ(regex.groupCount(), 0);
}

TEST(ArtifactRegexContractTest, PatternBeyondLengthLimitReportsPatternTooLong)
{
    const std::string pattern(16'385, 'a');
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex regex;

    EXPECT_FALSE(regex.compile(StringView(pattern.data(), pattern.size()), false, &error));
    EXPECT_EQ(error, RegexErrorCode::PatternTooLong);
    EXPECT_EQ(regex.errorCode(), RegexErrorCode::PatternTooLong);
    EXPECT_FALSE(regex.isValid());
}

TEST(ArtifactRegexContractTest, RecompileReplacesPatternAndCaseMode)
{
    ArtifactRegex regex("old");
    ASSERT_TRUE(regex.isValid());
    ASSERT_TRUE(regex.compile("new", true));
    EXPECT_FALSE(regex.matches("old"));
    EXPECT_TRUE(regex.matches("NEW"));
    EXPECT_EQ(regex.errorCode(), RegexErrorCode::None);
}

TEST(ArtifactRegexContractTest, ParserReportsTrailingEscapeAndMissingClassTerminator)
{
    RegexErrorCode error = RegexErrorCode::None;
    ArtifactRegex trailing;
    EXPECT_FALSE(trailing.compile("abc\\", false, &error));
    EXPECT_EQ(error, RegexErrorCode::TrailingBackslash);

    ArtifactRegex bracket;
    EXPECT_FALSE(bracket.compile("[abc", false, &error));
    EXPECT_EQ(error, RegexErrorCode::MissingClosingBracket);
}

TEST(ArtifactRegexContractTest, NonCapturingGroupsDoNotConsumeCaptureSlots)
{
    const ArtifactRegex regex("(a)(?:b)(c)");
    EXPECT_EQ(regex.groupCount(), 2);
    const auto match = regex.search("abc");
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[1].length, 1u);
    EXPECT_EQ(match.groups[2].position, 2u);
}

TEST(ArtifactRegexContractTest, LookaheadAssertsWithoutConsumingInput)
{
    const ArtifactRegex positive("foo(?=123)");
    const ArtifactRegex negative("a(?!b)");
    const auto match = positive.search("foo123bar");
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].length, 3u);
    EXPECT_FALSE(positive.matches("foo456bar"));
    const auto hits = negative.findAll("ab ac ad");
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].groups[0].position, 3u);
    EXPECT_EQ(hits[1].groups[0].position, 6u);
}

TEST(ArtifactRegexContractTest, BackreferencesMatchCapturedTextExactly)
{
    const ArtifactRegex regex("(\\w+) \\1");
    EXPECT_TRUE(regex.matches("hello hello world"));
    EXPECT_FALSE(regex.matches("hello world"));
    const auto match = regex.search("say bye bye now");
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[1].length, 3u);
}

TEST(ArtifactRegexContractTest, LazyQuantifierReturnsShortestSuccessfulMatch)
{
    const ArtifactRegex regex("a+?");
    const auto match = regex.search("aaaa");
    ASSERT_TRUE(match.matched);
    EXPECT_EQ(match.groups[0].length, 1u);
}
