#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

import Utils.Text.Encoding;

using namespace ArtifactCore;

TEST(TextEncodingContractTest, EmptyAndAsciiStringsAreValidUtf8)
{
    EXPECT_TRUE(isValidUtf8(""));
    EXPECT_TRUE(isValidUtf8("Artifact Studio 123"));
    EXPECT_FALSE(firstInvalidUtf8Offset("").has_value());
    EXPECT_EQ(utf8CodepointCount("").value(), 0u);
}

TEST(TextEncodingContractTest, AcceptsUnicodeScalarBoundaryEncodings)
{
    constexpr std::string_view points[] = {
        std::string_view("\x7f", 1),
        std::string_view("\xc2\x80", 2),
        std::string_view("\xdf\xbf", 2),
        std::string_view("\xe0\xa0\x80", 3),
        std::string_view("\xed\x9f\xbf", 3),
        std::string_view("\xee\x80\x80", 3),
        std::string_view("\xf0\x90\x80\x80", 4),
        std::string_view("\xf4\x8f\xbf\xbf", 4)};
    for (const auto point : points) {
        EXPECT_TRUE(isValidUtf8(point));
        EXPECT_FALSE(firstInvalidUtf8Offset(point).has_value());
    }
}

TEST(TextEncodingContractTest, RejectsInvalidLeadBytes)
{
    constexpr std::string_view invalid[] = {
        std::string_view("\x80", 1),
        std::string_view("\xbf", 1),
        std::string_view("\xc0\x80", 2),
        std::string_view("\xc1\xbf", 2),
        std::string_view("\xf5\x80\x80\x80", 4)};
    for (const auto value : invalid) {
        EXPECT_FALSE(isValidUtf8(value));
        ASSERT_TRUE(firstInvalidUtf8Offset(value).has_value());
        EXPECT_EQ(*firstInvalidUtf8Offset(value), 0u);
    }
}

TEST(TextEncodingContractTest, SingleByteUtf8ClassificationMatchesAsciiRange)
{
    for (unsigned int byte = 0; byte <= 0xff; ++byte) {
        const char value = static_cast<char>(byte);
        const std::string_view input(&value, 1);
        EXPECT_EQ(isValidUtf8(input), byte <= 0x7f) << "byte: " << byte;
    }
}

TEST(TextEncodingContractTest, ValidityAndErrorOffsetAgreeForEveryTwoByteSequence)
{
    for (unsigned int first = 0; first <= 0xff; ++first) {
        for (unsigned int second = 0; second <= 0xff; ++second) {
            const char bytes[] = {
                static_cast<char>(first), static_cast<char>(second)};
            const std::string_view input(bytes, sizeof(bytes));
            const bool valid = isValidUtf8(input);
            const auto invalidOffset = firstInvalidUtf8Offset(input);
            EXPECT_EQ(valid, !invalidOffset.has_value())
                << "bytes: " << first << ", " << second;
        }
    }
}

TEST(TextEncodingContractTest, RejectsTruncatedMultibyteSequencesAtTheirLead)
{
    constexpr std::string_view invalid[] = {
        std::string_view("\xc2", 1),
        std::string_view("\xe2\x82", 2),
        std::string_view("\xf0\x9f\x92", 3)};
    for (const auto value : invalid) {
        EXPECT_FALSE(isValidUtf8(value));
        EXPECT_EQ(firstInvalidUtf8Offset(value), 0u);
    }
}

TEST(TextEncodingContractTest, ReportsBadContinuationByteOffset)
{
    constexpr std::string_view invalid("a\xe2(\xa1", 4);
    EXPECT_FALSE(isValidUtf8(invalid));
    EXPECT_EQ(firstInvalidUtf8Offset(invalid), 2u);
}

TEST(TextEncodingContractTest, ReportsExactBadContinuationOffsetForTwoToFourByteSequences)
{
    EXPECT_EQ(firstInvalidUtf8Offset(std::string_view("A\xc2x", 3)), 2u);
    EXPECT_EQ(firstInvalidUtf8Offset(std::string_view("A\xe2\x82x", 4)), 3u);
    EXPECT_EQ(firstInvalidUtf8Offset(std::string_view("A\xf0\x9f\x92x", 5)), 4u);
}

TEST(TextEncodingContractTest, RejectsOverlongSurrogateAndOutOfRangeSequences)
{
    EXPECT_FALSE(isValidUtf8(std::string_view("\xe0\x9f\x80", 3)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xed\xa0\x80", 3)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xf0\x8f\xbf\xbf", 4)));
    EXPECT_FALSE(isValidUtf8(std::string_view("\xf4\x90\x80\x80", 4)));
}

TEST(TextEncodingContractTest, ScalarRangeViolationsReportTheSequenceLead)
{
    EXPECT_EQ(firstInvalidUtf8Offset(std::string_view("ok\xe0\x9f\x80", 5)), 2u);
    EXPECT_EQ(firstInvalidUtf8Offset(std::string_view("ok\xed\xa0\x80", 5)), 2u);
    EXPECT_EQ(firstInvalidUtf8Offset(std::string_view("ok\xf4\x90\x80\x80", 6)), 2u);
}

TEST(TextEncodingContractTest, OffsetIsMeasuredFromTheStartOfTheOriginalInput)
{
    constexpr std::string_view invalid("prefix\xfftail", 11);
    EXPECT_EQ(firstInvalidUtf8Offset(invalid), 6u);
}

TEST(TextEncodingContractTest, DetectsAllBomKindsAndPrefersUtf32LittleEndian)
{
    EXPECT_EQ(detectBom(std::string_view("\xef\xbb\xbf", 3)), ByteOrderMark::Utf8);
    EXPECT_EQ(detectBom(std::string_view("\xff\xfe", 2)), ByteOrderMark::Utf16LittleEndian);
    EXPECT_EQ(detectBom(std::string_view("\xfe\xff", 2)), ByteOrderMark::Utf16BigEndian);
    EXPECT_EQ(detectBom(std::string_view("\xff\xfe\x00\x00", 4)), ByteOrderMark::Utf32LittleEndian);
    EXPECT_EQ(detectBom(std::string_view("\x00\x00\xfe\xff", 4)), ByteOrderMark::Utf32BigEndian);
    EXPECT_EQ(detectBom("plain"), ByteOrderMark::None);
}

TEST(TextEncodingContractTest, PartialBomPrefixesAreNotStripped)
{
    constexpr std::string_view partialUtf8("\xef\xbb", 2);
    constexpr std::string_view partialUtf16("\xff", 1);
    EXPECT_EQ(detectBom(partialUtf8), ByteOrderMark::None);
    EXPECT_EQ(detectBom(partialUtf16), ByteOrderMark::None);
    EXPECT_EQ(stripBom(partialUtf8), partialUtf8);
    EXPECT_EQ(stripBom(partialUtf16), partialUtf16);
}

TEST(TextEncodingContractTest, BomSizeAndStripHandleEachRecognizedPrefix)
{
    EXPECT_EQ(bomSize(ByteOrderMark::None), 0u);
    EXPECT_EQ(bomSize(ByteOrderMark::Utf8), 3u);
    EXPECT_EQ(bomSize(ByteOrderMark::Utf16BigEndian), 2u);
    EXPECT_EQ(bomSize(ByteOrderMark::Utf32LittleEndian), 4u);
    EXPECT_EQ(stripBom(std::string_view("\xef\xbb\xbftext", 7)), "text");
    EXPECT_EQ(stripBom(std::string_view("\x00\x00\xfe\xfftext", 8)), "text");
    EXPECT_EQ(bomSize(static_cast<ByteOrderMark>(255)), 0u);
}

TEST(TextEncodingContractTest, CheckedUtf8ConversionPreservesValidBytes)
{
    constexpr std::string_view utf8("A\xe3\x81\x82", 4);
    const auto converted = fromUtf8Checked(utf8);
    ASSERT_TRUE(converted);
    EXPECT_EQ(toStdString(converted.value()), std::string(utf8));
}

TEST(TextEncodingContractTest, CheckedConversionsAcceptEmptyPayloads)
{
    const auto stringValue = fromUtf8Checked(std::string_view{});
    const auto bomValue = fromUtf8BomAware(std::string_view("\xef\xbb\xbf", 3));
    const auto codepoints = toUtf32Checked(std::string_view{});

    ASSERT_TRUE(stringValue);
    EXPECT_TRUE(stringValue.value().isEmpty());
    ASSERT_TRUE(bomValue);
    EXPECT_TRUE(bomValue.value().isEmpty());
    ASSERT_TRUE(codepoints);
    EXPECT_TRUE(codepoints.value().empty());
}

TEST(TextEncodingContractTest, CheckedUtf8ConversionReportsFirstInvalidByte)
{
    constexpr std::string_view invalid("ok\x80", 3);
    const auto converted = fromUtf8Checked(invalid);
    ASSERT_FALSE(converted);
    EXPECT_EQ(converted.error(), ErrorCode::InvalidArgument);
    EXPECT_EQ(converted.errorContext().objectId, "2");
    EXPECT_EQ(converted.errorContext().operation, "encoding.fromUtf8Checked");
}

TEST(TextEncodingContractTest, BomAwareConversionStripsRecognizedBomAndRewritesErrorOperation)
{
    constexpr std::string_view valid("\xef\xbb\xbfhello", 8);
    constexpr std::string_view invalid("\xef\xbb\xbf\xc0\xaf", 5);
    const auto converted = fromUtf8BomAware(valid);
    const auto rejected = fromUtf8BomAware(invalid);
    ASSERT_TRUE(converted);
    EXPECT_EQ(toStdString(converted.value()), "hello");
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.errorContext().operation, "encoding.fromUtf8BomAware");
    EXPECT_EQ(rejected.errorContext().objectId, "0");
}

TEST(TextEncodingContractTest, Utf8ToUtf32DecodesOneThroughFourByteCodePoints)
{
    constexpr std::string_view utf8("A\xc2\xa2\xe2\x82\xac\xf0\x9f\x98\x80", 10);
    const auto converted = toUtf32Checked(utf8);
    ASSERT_TRUE(converted);
    ASSERT_EQ(converted.value().size(), 4u);
    EXPECT_EQ(converted.value()[0], U'A');
    EXPECT_EQ(converted.value()[1], U'\u00a2');
    EXPECT_EQ(converted.value()[2], U'\u20ac');
    EXPECT_EQ(converted.value()[3], U'\U0001f600');
}

TEST(TextEncodingContractTest, CodepointCountMatchesDecodedSequenceAndPropagatesFailure)
{
    EXPECT_EQ(utf8CodepointCount("A\xe3\x81\x82").value(), 2u);
    const auto invalid = utf8CodepointCount(std::string_view("\x80", 1));
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.errorContext().operation, "encoding.toUtf32Checked");
}

TEST(TextEncodingContractTest, Utf32ConversionFailureReportsInvalidByteOffset)
{
    constexpr std::string_view invalid("A\xe2\x28\xa1", 4);
    const auto converted = toUtf32Checked(invalid);
    ASSERT_FALSE(converted);
    EXPECT_EQ(converted.errorContext().operation, "encoding.toUtf32Checked");
    EXPECT_EQ(converted.errorContext().objectId, "2");
}
