#include <gtest/gtest.h>

#include <QByteArray>
#include <QString>
#include <QStringView>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

import Utils.Buffer;

using namespace ArtifactCore;

namespace {
std::string bytes(BufferView view)
{
    return toStdString(view);
}
}

TEST(BufferContractTest, ConvertsByteViewToOwnedQtAndStandardStrings)
{
    constexpr std::string_view source("left\0right", 10);
    const BufferView view = toBufferView(source);
    EXPECT_EQ(toQByteArray(view), QByteArray(source.data(), static_cast<qsizetype>(source.size())));
    EXPECT_EQ(toStdString(view), std::string(source));
}

TEST(BufferContractTest, CombinesTwoViewsAndInitializerListWithoutLosingBinaryBytes)
{
    const QByteArray first("a\0", 2);
    const QByteArray second("b\xff", 2);
    const auto combined = toQByteArray(toBufferView(first), toBufferView(second));
    EXPECT_EQ(combined, QByteArray("a\0b\xff", 4));

    const auto listed = toQByteArray({toBufferView(first), toBufferView(second)});
    EXPECT_EQ(listed, combined);
}

TEST(BufferContractTest, QByteArrayInputViewTracksStableSourceStorage)
{
    const QByteArray source("buffer");
    const BufferView view = toBufferView(source);
    EXPECT_EQ(bytes(view), "buffer");
    EXPECT_EQ(view.data(), source.constData());
}

TEST(BufferContractTest, ConvertsUtf8BufferViewToQString)
{
    constexpr std::string_view utf8("Artifact \xe3\x81\x82", 12);
    EXPECT_EQ(toQString(toBufferView(utf8)), QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size())));
}

TEST(BufferContractTest, ByteArrayConversionReturnsAnIndependentValue)
{
    const QByteArray source("copy");
    QByteArray converted = toQByteArray(toBufferView(source));
    converted[0] = 'C';
    EXPECT_EQ(source, QByteArray("copy"));
    EXPECT_EQ(converted, QByteArray("Copy"));
}

TEST(BufferContractTest, ConvertsBytePointerAndSpanViews)
{
    constexpr std::array<std::uint8_t, 4> source{0, 127, 128, 255};
    const auto pointerView = toBufferView(source.data(), source.size());
    const auto spanView = toBufferView(std::span<const std::uint8_t>(source));
    EXPECT_EQ(bytes(pointerView), std::string("\0\x7f\x80\xff", 4));
    EXPECT_EQ(bytes(spanView), bytes(pointerView));
}

TEST(BufferContractTest, ConvertsQStringViewAsRawUtf16Bytes)
{
    const QString source = QStringLiteral("Ab");
    const QStringView textView(source);
    const BufferView buffer = toBufferView(textView);
    EXPECT_EQ(buffer.size(), textView.size() * static_cast<qsizetype>(sizeof(QChar)));
    EXPECT_EQ(std::string(buffer.data(), static_cast<std::size_t>(buffer.size())),
              std::string(reinterpret_cast<const char*>(textView.data()),
                          static_cast<std::size_t>(textView.size() * sizeof(QChar))));
}

TEST(BufferContractTest, StartsWithAndEndsWithRespectFullByteLengths)
{
    const BufferView value = toBufferView(std::string_view("artifact.core"));
    EXPECT_TRUE(startsWith(value, toBufferView(std::string_view("artifact"))));
    EXPECT_TRUE(endsWith(value, toBufferView(std::string_view("core"))));
    EXPECT_FALSE(startsWith(value, toBufferView(std::string_view("Artifact"))));
    EXPECT_FALSE(endsWith(value, toBufferView(std::string_view("CORE"))));
}

TEST(BufferContractTest, EmptyPrefixAndSuffixMatchIncludingEmptyValue)
{
    const BufferView empty;
    const BufferView value = toBufferView(std::string_view("x"));
    EXPECT_TRUE(startsWith(value, empty));
    EXPECT_TRUE(endsWith(value, empty));
    EXPECT_TRUE(startsWith(empty, empty));
    EXPECT_TRUE(endsWith(empty, empty));
}

TEST(BufferContractTest, ContainsFindsBinaryNeedleAndTreatsEmptyNeedleAsPresent)
{
    const BufferView value = toBufferView(std::string_view("a\0bc", 4));
    EXPECT_TRUE(contains(value, toBufferView(std::string_view("\0b", 2))));
    EXPECT_TRUE(contains(value, BufferView{}));
    EXPECT_FALSE(contains(value, toBufferView(std::string_view("bd"))));
}

TEST(BufferContractTest, TrimRemovesAsciiSpaceTabLineFeedAndCarriageReturn)
{
    const BufferView input = toBufferView(std::string_view(" \t\rvalue\n "));
    EXPECT_EQ(bytes(trim(input)), "value");
    EXPECT_EQ(bytes(trim(BufferView{})), "");
}

TEST(BufferContractTest, LeftAndRightTrimAreIndependent)
{
    const BufferView input = toBufferView(std::string_view(" \tvalue \r"));
    EXPECT_EQ(bytes(ltrim(input)), "value \r");
    EXPECT_EQ(bytes(rtrim(input)), " \tvalue");
    EXPECT_EQ(bytes(trimLeft(input)), "value \r");
    EXPECT_EQ(bytes(trimRight(input)), " \tvalue");
    EXPECT_EQ(bytes(trimStart(input)), "value \r");
    EXPECT_EQ(bytes(trimEnd(input)), " \tvalue");
}

TEST(BufferContractTest, TrimDoesNotTreatFormFeedAsWhitespace)
{
    const BufferView input = toBufferView(std::string_view("\fvalue\f"));
    EXPECT_EQ(bytes(trim(input)), "\fvalue\f");
}

TEST(BufferContractTest, SplitClampsLeftSizeToAvailableBytes)
{
    const BufferView input = toBufferView(std::string_view("abcd"));
    const auto middle = split(input, 2);
    EXPECT_EQ(bytes(middle.first), "ab");
    EXPECT_EQ(bytes(middle.second), "cd");
    const auto beyond = split(input, 99);
    EXPECT_EQ(bytes(beyond.first), "abcd");
    EXPECT_TRUE(beyond.second.isEmpty());
}

TEST(BufferContractTest, SplitAtClampsNegativeAndPastEndIndices)
{
    const BufferView input = toBufferView(std::string_view("abcd"));
    const auto negative = splitAt(input, -1);
    EXPECT_TRUE(negative.first.isEmpty());
    EXPECT_EQ(bytes(negative.second), "abcd");
    const auto beyond = splitAt(input, 8);
    EXPECT_EQ(bytes(beyond.first), "abcd");
    EXPECT_TRUE(beyond.second.isEmpty());
}

TEST(BufferContractTest, SplitFirstRetainsDelimiterInRightPart)
{
    const auto parts = splitFirst(toBufferView(std::string_view("a/b/c")), '/');
    EXPECT_EQ(bytes(parts.first), "a");
    EXPECT_EQ(bytes(parts.second), "/b/c");
}

TEST(BufferContractTest, SplitFirstWithoutDelimiterReturnsWholeLeftPart)
{
    const auto parts = splitFirst(toBufferView(std::string_view("plain")), '/');
    EXPECT_EQ(bytes(parts.first), "plain");
    EXPECT_TRUE(parts.second.isEmpty());
}

TEST(BufferContractTest, DelimitersAtEdgesProduceEmptyFieldSlices)
{
    const BufferView input = toBufferView(std::string_view("/middle/"));

    EXPECT_TRUE(beforeFirst(input, '/').isEmpty());
    EXPECT_EQ(bytes(afterFirst(input, '/')), "middle/");
    EXPECT_EQ(bytes(beforeLast(input, '/')), "/middle");
    EXPECT_TRUE(afterLast(input, '/').isEmpty());
}

TEST(BufferContractTest, BeforeAndAfterFirstReturnExpectedSlices)
{
    const BufferView input = toBufferView(std::string_view("one:two:three"));
    EXPECT_EQ(bytes(beforeFirst(input, ':')), "one");
    EXPECT_EQ(bytes(afterFirst(input, ':')), "two:three");
    EXPECT_EQ(bytes(beforeSeparator(input, ':')), "one");
    EXPECT_EQ(bytes(afterSeparator(input, ':')), "two:three");
    EXPECT_EQ(bytes(beforeDelimiter(input, ':')), "one");
    EXPECT_EQ(bytes(afterDelimiter(input, ':')), "two:three");
}

TEST(BufferContractTest, SplitLastRetainsDelimiterInRightPart)
{
    const auto parts = splitLast(toBufferView(std::string_view("one:two:three")), ':');
    EXPECT_EQ(bytes(parts.first), "one:two");
    EXPECT_EQ(bytes(parts.second), ":three");
}

TEST(BufferContractTest, BeforeAndAfterLastReturnExpectedSlicesWhenFound)
{
    const BufferView input = toBufferView(std::string_view("one:two:three"));
    EXPECT_EQ(bytes(beforeLast(input, ':')), "one:two");
    EXPECT_EQ(bytes(afterLast(input, ':')), "three");
    EXPECT_EQ(bytes(suffixAfter(input, ':')), "three");
    EXPECT_EQ(bytes(tailAfter(input, ':')), "three");
    EXPECT_EQ(bytes(headBefore(input, ':')), "one");
    EXPECT_EQ(bytes(prefixOf(input, ':')), "one");
    EXPECT_EQ(bytes(suffixOf(input, ':')), "three");
    EXPECT_EQ(bytes(trailingPart(input, ':')), "three");
    EXPECT_EQ(bytes(leadingPart(input, ':')), "one");
    EXPECT_EQ(bytes(takeBefore(input, ':')), "one");
    EXPECT_EQ(bytes(takeAfter(input, ':')), "three");
}

TEST(BufferContractTest, AfterLastWithoutDelimiterKeepsWholeInput)
{
    const BufferView input = toBufferView(std::string_view("plain"));

    EXPECT_EQ(bytes(afterLast(input, ':')), "plain");
    EXPECT_EQ(bytes(suffixAfter(input, ':')), "plain");
    EXPECT_EQ(bytes(tailAfter(input, ':')), "plain");
    EXPECT_EQ(bytes(suffixOf(input, ':')), "plain");
    EXPECT_EQ(bytes(trailingPart(input, ':')), "plain");
    EXPECT_EQ(bytes(takeAfter(input, ':')), "plain");
}

TEST(BufferContractTest, PrefixAndSuffixSliceHelpersRespectMismatch)
{
    const BufferView input = toBufferView(std::string_view("prefix-middle-suffix"));
    EXPECT_EQ(bytes(afterPrefix(input, toBufferView(std::string_view("prefix-")))), "middle-suffix");
    EXPECT_EQ(bytes(beforeSuffix(input, toBufferView(std::string_view("-suffix")))), "prefix-middle");
    EXPECT_EQ(bytes(afterSuffix(input, toBufferView(std::string_view("-suffix")))), "-suffix");
    EXPECT_TRUE(afterPrefix(input, toBufferView(std::string_view("wrong"))).isEmpty());
    EXPECT_TRUE(beforeSuffix(input, toBufferView(std::string_view("wrong"))).isEmpty());
}

TEST(BufferContractTest, EmptyPrefixAndSuffixMatchWholeValueWithoutSuffixBytes)
{
    const BufferView input = toBufferView(std::string_view("payload"));
    const BufferView empty;

    EXPECT_EQ(bytes(afterPrefix(input, empty)), "payload");
    EXPECT_EQ(bytes(beforeSuffix(input, empty)), "payload");
    EXPECT_TRUE(afterSuffix(input, empty).isEmpty());
}

TEST(BufferContractTest, PrefixAndSuffixAliasesMatchPrimaryPredicates)
{
    const BufferView value = toBufferView(std::string_view("artifact"));
    EXPECT_TRUE(hasPrefix(value, toBufferView(std::string_view("art"))));
    EXPECT_TRUE(hasSuffix(value, toBufferView(std::string_view("fact"))));
    EXPECT_FALSE(hasPrefix(value, toBufferView(std::string_view("fact"))));
    EXPECT_FALSE(hasSuffix(value, toBufferView(std::string_view("art"))));
}

TEST(BufferContractTest, RepeatHandlesPositiveZeroAndNegativeCounts)
{
    const BufferView value = toBufferView(std::string_view("ab"));
    EXPECT_EQ(repeat(value, 3), QByteArray("ababab"));
    EXPECT_TRUE(repeat(value, 0).isEmpty());
    EXPECT_TRUE(repeat(value, -2).isEmpty());
    EXPECT_TRUE(repeat(BufferView{}, 4).isEmpty());
}

TEST(BufferContractTest, AppendReturnsCombinedCopyWithoutChangingInput)
{
    const QByteArray base("left");
    const QByteArray extra("right");
    const QByteArray appended = appendToQByteArray(base, toBufferView(extra));
    EXPECT_EQ(appended, QByteArray("leftright"));
    EXPECT_EQ(base, QByteArray("left"));
}
