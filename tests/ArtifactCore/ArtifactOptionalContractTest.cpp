#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

import Core.ArtifactOptional;

using ArtifactCore::ArtifactNullopt;
using ArtifactCore::ArtifactOptional;
using ArtifactCore::make_optional;

namespace {
struct LifetimeProbe {
    static inline int liveCount = 0;
    int value = 0;

    explicit LifetimeProbe(int input = 0) : value(input) { ++liveCount; }
    LifetimeProbe(const LifetimeProbe& other) : value(other.value) { ++liveCount; }
    LifetimeProbe(LifetimeProbe&& other) noexcept : value(other.value) { ++liveCount; }
    LifetimeProbe& operator=(const LifetimeProbe&) = default;
    LifetimeProbe& operator=(LifetimeProbe&&) noexcept = default;
    ~LifetimeProbe() { --liveCount; }
};
}

static_assert(std::is_same_v<decltype(std::declval<ArtifactOptional<int>&>().value()), int&>);
static_assert(std::is_same_v<decltype(std::declval<const ArtifactOptional<int>&>().value()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<ArtifactOptional<int>&&>().value()), int&&>);
static_assert(std::is_same_v<decltype(*std::declval<ArtifactOptional<int&>&>()), int&>);
static_assert(std::is_same_v<decltype(*std::declval<ArtifactOptional<const int&>&>()), const int&>);

TEST(ArtifactOptionalContractTest, DefaultAndNulloptConstructionAreEmpty)
{
    const ArtifactOptional<int> defaultValue;
    const ArtifactOptional<int> nullValue(ArtifactNullopt);
    EXPECT_FALSE(defaultValue.has_value());
    EXPECT_FALSE(static_cast<bool>(defaultValue));
    EXPECT_FALSE(nullValue.has_value());
}

TEST(ArtifactOptionalContractTest, ValueConstructionAndAssignmentSetPresence)
{
    ArtifactOptional<int> value(12);
    EXPECT_TRUE(value.has_value());
    value = 27;
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 27);
}

TEST(ArtifactOptionalContractTest, ValueThrowsWhenOptionalIsEmpty)
{
    const ArtifactOptional<int> empty;
    EXPECT_THROW((void)empty.value(), std::bad_optional_access);
}

TEST(ArtifactOptionalContractTest, ValueOrSelectsPresentValueOrFallback)
{
    const ArtifactOptional<int> present(5);
    const ArtifactOptional<int> empty;
    EXPECT_EQ(present.value_or(9), 5);
    EXPECT_EQ(empty.value_or(9), 9);
}

TEST(ArtifactOptionalContractTest, RvalueValueOrMovesPresentOwningValue)
{
    ArtifactOptional<std::string> present(std::string("stored"));
    std::string extracted = std::move(present).value_or(std::string("fallback"));
    EXPECT_EQ(extracted, "stored");
}

TEST(ArtifactOptionalContractTest, RvalueValueOrMovesMoveOnlyFallbackForEmptyOptional)
{
    ArtifactOptional<std::unique_ptr<int>> empty;
    auto fallback = std::make_unique<int>(91);

    auto extracted = std::move(empty).value_or(std::move(fallback));

    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(*extracted, 91);
    EXPECT_EQ(fallback, nullptr);
    EXPECT_FALSE(empty.has_value());
}

TEST(ArtifactOptionalContractTest, CopyConstructionCreatesIndependentValue)
{
    const ArtifactOptional<std::string> source(std::string("source"));
    ArtifactOptional<std::string> copy(source);
    copy.value().append("-copy");
    EXPECT_EQ(source.value(), "source");
    EXPECT_EQ(copy.value(), "source-copy");
}

TEST(ArtifactOptionalContractTest, CopyConstructionPreservesEmptyState)
{
    const ArtifactOptional<int> source;
    const ArtifactOptional<int> copy(source);
    EXPECT_FALSE(copy.has_value());
}

TEST(ArtifactOptionalContractTest, SelfCopyAssignmentPreservesContainedValue)
{
    ArtifactOptional<std::string> value(std::string("kept"));

    value = value;

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "kept");
}

TEST(ArtifactOptionalContractTest, SelfMoveAssignmentPreservesContainedValue)
{
    ArtifactOptional<std::string> value(std::string("kept"));

    value = std::move(value);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "kept");
}

TEST(ArtifactOptionalContractTest, MoveConstructionTransfersOwningValue)
{
    ArtifactOptional<std::string> source(std::string("payload"));
    ArtifactOptional<std::string> moved(std::move(source));
    ASSERT_TRUE(moved.has_value());
    EXPECT_EQ(moved.value(), "payload");
}

TEST(ArtifactOptionalContractTest, MoveConstructionLeavesSourceEngagedAndReusable)
{
    ArtifactOptional<std::string> source(std::string("payload"));
    ArtifactOptional<std::string> moved(std::move(source));

    ASSERT_TRUE(moved.has_value());
    EXPECT_EQ(moved.value(), "payload");
    EXPECT_TRUE(source.has_value());
    source = std::string("reused");
    EXPECT_EQ(source.value(), "reused");
}

TEST(ArtifactOptionalContractTest, CopyAssignmentHandlesAllPresenceTransitions)
{
    ArtifactOptional<int> empty;
    const ArtifactOptional<int> emptySource;
    const ArtifactOptional<int> present(31);
    empty = present;
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty.value(), 31);

    empty = emptySource;
    EXPECT_FALSE(empty.has_value());

    ArtifactOptional<int> existing(8);
    existing = present;
    EXPECT_EQ(existing.value(), 31);
}

TEST(ArtifactOptionalContractTest, MoveAssignmentHandlesAllPresenceTransitions)
{
    ArtifactOptional<std::string> empty;
    ArtifactOptional<std::string> source(std::string("moved"));
    empty = std::move(source);
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty.value(), "moved");

    ArtifactOptional<std::string> destination(std::string("old"));
    destination = std::move(empty);
    ASSERT_TRUE(destination.has_value());
    EXPECT_EQ(destination.value(), "moved");

    const ArtifactOptional<std::string> emptySource;
    destination = emptySource;
    EXPECT_FALSE(destination.has_value());
}

TEST(ArtifactOptionalContractTest, NulloptAssignmentAndResetAreIdempotent)
{
    ArtifactOptional<int> value(4);
    value = ArtifactNullopt;
    value.reset();
    EXPECT_FALSE(value.has_value());
}

TEST(ArtifactOptionalContractTest, RepeatedResetDestroysContainedObjectOnce)
{
    const int baseline = LifetimeProbe::liveCount;
    {
        ArtifactOptional<LifetimeProbe> value(LifetimeProbe{73});
        ASSERT_TRUE(value.has_value());
        value.reset();
        EXPECT_FALSE(value.has_value());
        EXPECT_EQ(LifetimeProbe::liveCount, baseline);
        value.reset();
        EXPECT_EQ(LifetimeProbe::liveCount, baseline);
    }
    EXPECT_EQ(LifetimeProbe::liveCount, baseline);
}

TEST(ArtifactOptionalContractTest, CopyAssignmentDestroysOldObjectAndCopiesNewValue)
{
    const int baseline = LifetimeProbe::liveCount;
    {
        ArtifactOptional<LifetimeProbe> destination(LifetimeProbe{1});
        const ArtifactOptional<LifetimeProbe> source(LifetimeProbe{2});
        destination = source;
        ASSERT_TRUE(destination.has_value());
        EXPECT_EQ(destination.value().value, 2);
    }
    EXPECT_EQ(LifetimeProbe::liveCount, baseline);
}

TEST(ArtifactOptionalContractTest, SwapExchangesTwoPresentValues)
{
    ArtifactOptional<int> left(3);
    ArtifactOptional<int> right(9);
    left.swap(right);
    EXPECT_EQ(left.value(), 9);
    EXPECT_EQ(right.value(), 3);
}

TEST(ArtifactOptionalContractTest, SwapExchangesPresentAndEmptyStates)
{
    ArtifactOptional<int> present(42);
    ArtifactOptional<int> empty;

    present.swap(empty);
    EXPECT_FALSE(present.has_value());
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty.value(), 42);

    empty.swap(present);
    EXPECT_TRUE(present.has_value());
    EXPECT_EQ(present.value(), 42);
    EXPECT_FALSE(empty.has_value());
}

TEST(ArtifactOptionalContractTest, SwapTransfersMoveOnlyOwningValueAcrossPresenceStates)
{
    ArtifactOptional<std::unique_ptr<int>> present(std::make_unique<int>(73));
    ArtifactOptional<std::unique_ptr<int>> empty;

    present.swap(empty);

    EXPECT_FALSE(present.has_value());
    ASSERT_TRUE(empty.has_value());
    ASSERT_NE(empty.value(), nullptr);
    EXPECT_EQ(*empty.value(), 73);

    empty.swap(present);

    ASSERT_TRUE(present.has_value());
    ASSERT_NE(present.value(), nullptr);
    EXPECT_EQ(*present.value(), 73);
    EXPECT_FALSE(empty.has_value());
}

TEST(ArtifactOptionalContractTest, ReferenceOptionalAliasesAndRebindsTarget)
{
    int first = 2;
    int second = 7;
    ArtifactOptional<int&> reference(first);
    *reference = 4;
    EXPECT_EQ(first, 4);
    reference = second;
    EXPECT_EQ(&reference.value(), &second);
    reference.reset();
    EXPECT_FALSE(reference.has_value());
}

TEST(ArtifactOptionalContractTest, EmptyReferenceOptionalValueThrows)
{
    const ArtifactOptional<int&> empty;
    EXPECT_THROW((void)empty.value(), std::bad_optional_access);
}

TEST(ArtifactOptionalContractTest, EmptyReferenceOptionalValueOrReturnsFallbackAlias)
{
    int fallback = 13;
    const ArtifactOptional<int&> empty;

    int& selected = empty.value_or(fallback);
    selected = 29;

    EXPECT_EQ(fallback, 29);
    EXPECT_EQ(&selected, &fallback);
}

TEST(ArtifactOptionalContractTest, ConstReferenceOptionalRemainsReadOnly)
{
    const int stored = 18;
    const ArtifactOptional<const int&> reference(stored);
    ASSERT_TRUE(reference.has_value());
    EXPECT_EQ(reference.value(), 18);
    EXPECT_EQ(reference.value_or(5), 18);
}

TEST(ArtifactOptionalContractTest, MakeOptionalForwardsConstructionArguments)
{
    const auto value = make_optional<std::string>(4, 'a');
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "aaaa");
}
