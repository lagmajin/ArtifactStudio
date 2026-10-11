#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string_view>

import Utils.Text.Number;

using namespace ArtifactCore;

TEST(TextNumberContractTest, ParsesSignedDecimalValues)
{
    EXPECT_EQ(parseInt64("0").value(), 0);
    EXPECT_EQ(parseInt64("42").value(), 42);
    EXPECT_EQ(parseInt64("-42").value(), -42);
}

TEST(TextNumberContractTest, ParsesSignedMinimumAndMaximumExactly)
{
    const auto minimum = parseInt64("-9223372036854775808");
    const auto maximum = parseInt64("9223372036854775807");
    ASSERT_TRUE(minimum);
    ASSERT_TRUE(maximum);
    EXPECT_EQ(minimum.value(), std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(maximum.value(), std::numeric_limits<std::int64_t>::max());
}

TEST(TextNumberContractTest, SignedParserRejectsEmptyPartialAndWhitespaceInputs)
{
    constexpr std::string_view invalid[] = {"", "12x", " 12", "12 ", "\t12", "+12"};
    for (const auto value : invalid) {
        EXPECT_FALSE(parseInt64(value)) << "input: " << value;
    }
}

TEST(TextNumberContractTest, SignedParserRejectsOverflowAndUnderflow)
{
    EXPECT_FALSE(parseInt64("9223372036854775808"));
    EXPECT_FALSE(parseInt64("-9223372036854775809"));
}

TEST(TextNumberContractTest, ParsesUnsignedZeroAndMaximum)
{
    const auto zero = parseUInt64("0");
    const auto maximum = parseUInt64("18446744073709551615");
    ASSERT_TRUE(zero);
    ASSERT_TRUE(maximum);
    EXPECT_EQ(zero.value(), 0u);
    EXPECT_EQ(maximum.value(), std::numeric_limits<std::uint64_t>::max());
}

TEST(TextNumberContractTest, UnsignedParserRejectsNegativeAndOverflowValues)
{
    EXPECT_FALSE(parseUInt64("-1"));
    EXPECT_FALSE(parseUInt64("18446744073709551616"));
}

TEST(TextNumberContractTest, UnsignedParserRejectsNegativeZero)
{
    EXPECT_FALSE(parseUInt64("-0"));
    EXPECT_FALSE(parseUInt64("-000"));
}

TEST(TextNumberContractTest, UnsignedParserRejectsEmptyAndTrailingCharacters)
{
    EXPECT_FALSE(parseUInt64(""));
    EXPECT_FALSE(parseUInt64("17items"));
}

TEST(TextNumberContractTest, UnsignedParserRejectsPlusSignAndWhitespace)
{
    EXPECT_FALSE(parseUInt64("+17"));
    EXPECT_FALSE(parseUInt64(" 17"));
    EXPECT_FALSE(parseUInt64("17\n"));
}

TEST(TextNumberContractTest, ParsesOnlyDocumentedBooleanSpellings)
{
    EXPECT_TRUE(parseBool("true").value());
    EXPECT_TRUE(parseBool("1").value());
    EXPECT_FALSE(parseBool("false").value());
    EXPECT_FALSE(parseBool("0").value());
}

TEST(TextNumberContractTest, BooleanParserRejectsCaseAndWhitespaceVariants)
{
    constexpr std::string_view invalid[] = {"", "TRUE", "False", " true", "false ", "yes", "2"};
    for (const auto value : invalid) {
        EXPECT_FALSE(parseBool(value)) << "input: " << value;
    }
}

TEST(TextNumberContractTest, InvalidInputReportsParserOperationAndObjectId)
{
    const auto signedError = parseInt64("broken", "frame.index");
    const auto unsignedError = parseUInt64("-3", "frame.count");
    const auto booleanError = parseBool("yes", "feature.enabled");
    EXPECT_EQ(signedError.error(), ErrorCode::InvalidArgument);
    EXPECT_EQ(signedError.errorContext().operation, "number.parseInt64");
    EXPECT_EQ(signedError.errorContext().objectId, "frame.index");
    EXPECT_EQ(unsignedError.errorContext().operation, "number.parseUInt64");
    EXPECT_EQ(unsignedError.errorContext().objectId, "frame.count");
    EXPECT_EQ(booleanError.errorContext().operation, "number.parseBool");
    EXPECT_EQ(booleanError.errorContext().objectId, "feature.enabled");
}

TEST(TextNumberContractTest, EmptyInputUsesSpecificSignedAndUnsignedMessages)
{
    EXPECT_EQ(parseInt64("").errorContext().message, "integer input is empty");
    EXPECT_EQ(parseUInt64("").errorContext().message, "unsigned integer input is empty");
}

TEST(TextNumberContractTest, MalformedInputsReportSpecificMessagesWithoutObjectIds)
{
    const auto signedError = parseInt64("12x");
    const auto unsignedError = parseUInt64("-1");
    const auto booleanError = parseBool("TRUE");

    EXPECT_EQ(signedError.errorContext().message, "invalid integer input");
    EXPECT_EQ(unsignedError.errorContext().message, "invalid unsigned integer input");
    EXPECT_EQ(booleanError.errorContext().message, "invalid boolean input");
    EXPECT_TRUE(signedError.errorContext().objectId.empty());
    EXPECT_TRUE(unsignedError.errorContext().objectId.empty());
    EXPECT_TRUE(booleanError.errorContext().objectId.empty());
}

TEST(TextNumberContractTest, ParseErrorsCarryAValidSourceLocation)
{
    EXPECT_TRUE(parseInt64("x").errorContext().location.hasValue());
    EXPECT_TRUE(parseUInt64("x").errorContext().location.hasValue());
    EXPECT_TRUE(parseBool("x").errorContext().location.hasValue());
}

TEST(TextNumberContractTest, ParsesNonNullTerminatedStringViewSlices)
{
    const std::string_view storage = "prefix42suffix";
    const auto signedValue = parseInt64(storage.substr(6, 2));
    const auto unsignedValue = parseUInt64(storage.substr(6, 2));
    ASSERT_TRUE(signedValue);
    ASSERT_TRUE(unsignedValue);
    EXPECT_EQ(signedValue.value(), 42);
    EXPECT_EQ(unsignedValue.value(), 42u);
}

TEST(TextNumberContractTest, BooleanParserReadsNonNullTerminatedStringViewSlices)
{
    constexpr std::string_view storage = "xtruefalsey";
    const auto trueValue = parseBool(storage.substr(1, 4));
    const auto falseValue = parseBool(storage.substr(5, 5));

    ASSERT_TRUE(trueValue);
    ASSERT_TRUE(falseValue);
    EXPECT_TRUE(trueValue.value());
    EXPECT_FALSE(falseValue.value());
}

TEST(TextNumberContractTest, LeadingZerosAndNegativeZeroParseExactly)
{
    EXPECT_EQ(parseInt64("00042").value(), 42);
    EXPECT_EQ(parseInt64("-0").value(), 0);
    EXPECT_EQ(parseUInt64("00042").value(), 42u);
}

TEST(TextNumberContractTest, LongLeadingZeroPaddingPreservesExtremalValues)
{
    constexpr std::string_view signedMaximum =
        "0000000000000000000000000000009223372036854775807";
    constexpr std::string_view signedMinimum =
        "-0000000000000000000000000000009223372036854775808";
    constexpr std::string_view unsignedMaximum =
        "00000000000000000000000000000018446744073709551615";

    const auto maximum = parseInt64(signedMaximum);
    const auto minimum = parseInt64(signedMinimum);
    const auto unsignedMax = parseUInt64(unsignedMaximum);
    ASSERT_TRUE(maximum);
    ASSERT_TRUE(minimum);
    ASSERT_TRUE(unsignedMax);
    EXPECT_EQ(maximum.value(), std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(minimum.value(), std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(unsignedMax.value(), std::numeric_limits<std::uint64_t>::max());
}

TEST(TextNumberContractTest, EmbeddedNullBytesAreRejectedInsteadOfTruncating)
{
    constexpr char embeddedNull[] = {'4', '2', '\0', '7'};
    const std::string_view input(embeddedNull, sizeof(embeddedNull));
    EXPECT_FALSE(parseInt64(input));
    EXPECT_FALSE(parseUInt64(input));

    constexpr char boolWithNull[] = {'t', 'r', 'u', 'e', '\0'};
    EXPECT_FALSE(parseBool(std::string_view(boolWithNull, sizeof(boolWithNull))));
}
