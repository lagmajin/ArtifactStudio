#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>

import Utils.Optional;

using namespace ArtifactCore;

TEST(OptionalContractTest, AliasRepresentsPresentAndEmptyOptionalValues)
{
    const Optional<int> present = 7;
    const Optional<int> empty;
    EXPECT_TRUE(HasValue(present));
    EXPECT_FALSE(HasValue(empty));
}

TEST(OptionalContractTest, ValueOrUsesStoredValueOrFallback)
{
    EXPECT_EQ(ValueOr(Optional<int>{9}, 2), 9);
    EXPECT_EQ(ValueOr(Optional<int>{}, 2), 2);
}

TEST(OptionalContractTest, ValueOrElseInvokesFactoryOnlyWhenEmpty)
{
    int calls = 0;
    EXPECT_EQ(ValueOrElse(Optional<int>{4}, [&] { ++calls; return 8; }), 4);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(ValueOrElse(Optional<int>{}, [&] { ++calls; return 8; }), 8);
    EXPECT_EQ(calls, 1);
}

TEST(OptionalContractTest, TransformMapsPresentValueAndPreservesEmpty)
{
    const auto mapped = Transform(Optional<int>{5}, [](int value) { return std::to_string(value); });
    const auto empty = Transform(Optional<int>{}, [](int value) { return std::to_string(value); });
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "5");
    EXPECT_FALSE(empty.has_value());
}

TEST(OptionalContractTest, TransformDecaysReferenceResultsIntoOwnedOptionalValues)
{
    const Optional<std::string> source{"owned"};
    const auto transformed = Transform(source, [](const std::string& value) -> const std::string& {
        return value;
    });
    static_assert(std::is_same_v<decltype(transformed), const Optional<std::string>>);
    ASSERT_TRUE(transformed.has_value());
    EXPECT_EQ(*transformed, "owned");
}

TEST(OptionalContractTest, AndThenChainsPresentValueAndShortCircuitsEmpty)
{
    int calls = 0;
    const auto chained = AndThen(Optional<int>{3}, [&](int value) {
        ++calls;
        return Optional<std::string>{std::to_string(value * 2)};
    });
    const auto empty = AndThen(Optional<int>{}, [&](int value) {
        ++calls;
        return Optional<std::string>{std::to_string(value)};
    });
    ASSERT_TRUE(chained.has_value());
    EXPECT_EQ(*chained, "6");
    EXPECT_FALSE(empty.has_value());
    EXPECT_EQ(calls, 1);
}

TEST(OptionalContractTest, TransformOrElseAppliesFallbackOnlyForEmptyInput)
{
    EXPECT_EQ(TransformOrElse(Optional<int>{5}, [](int value) { return value + 1; }, 99), 6);
    EXPECT_EQ(TransformOrElse(Optional<int>{}, [](int value) { return value + 1; }, 99), 99);
}

TEST(OptionalContractTest, ToResultPreservesValueAndUsesRequestedErrorForEmpty)
{
    const auto success = ToResult(Optional<int>{12}, ErrorCode::NotFound);
    const auto failure = ToResult(Optional<int>{}, ErrorCode::NotFound);
    EXPECT_EQ(success.value(), 12);
    EXPECT_EQ(failure.error(), ErrorCode::NotFound);
}

TEST(OptionalContractTest, OrElseResultUsesTheSameConversionContract)
{
    const auto success = OrElseResult(Optional<int>{13});
    const auto failure = OrElseResult(Optional<int>{}, ErrorCode::Busy);
    EXPECT_EQ(success.value(), 13);
    EXPECT_EQ(failure.error(), ErrorCode::Busy);
}

TEST(OptionalContractTest, FilterKeepsMatchingValueAndDropsRejectedValue)
{
    const Optional<int> source = 8;
    const auto kept = Filter(source, [](int value) { return value % 2 == 0; });
    const auto rejected = Filter(Optional<int>{7}, [](int value) { return value % 2 == 0; });
    ASSERT_TRUE(kept.has_value());
    EXPECT_EQ(*kept, 8);
    EXPECT_EQ(source, 8);
    EXPECT_FALSE(rejected.has_value());
}

TEST(OptionalContractTest, FilterDoesNotInvokePredicateForEmptyValue)
{
    int calls = 0;
    const auto empty = Filter(Optional<int>{}, [&](int) { ++calls; return true; });
    EXPECT_FALSE(empty.has_value());
    EXPECT_EQ(calls, 0);
}

TEST(OptionalContractTest, DefaultHelpersUseValueInitializationWhenEmpty)
{
    EXPECT_EQ(UnwrapOrDefault(Optional<int>{17}), 17);
    EXPECT_EQ(UnwrapOrDefault(Optional<int>{}), 0);
    EXPECT_EQ(ValueOrDefault(Optional<std::string>{}), "");
    EXPECT_EQ(ValueOrDefault(Optional<std::string>{"text"}), "text");
}

TEST(OptionalContractTest, PresenceFallbackOnlyAppliesToEmptyOptional)
{
    EXPECT_TRUE(HasValueOr(Optional<int>{1}, false));
    EXPECT_TRUE(HasValueOr(Optional<int>{}, true));
    EXPECT_FALSE(HasValueOr(Optional<int>{}, false));
}

TEST(OptionalContractTest, MutableAndConstPointersReflectPresence)
{
    Optional<int> present = 21;
    const Optional<int>& readOnly = present;
    Optional<int> empty;
    ASSERT_NE(ValuePtr(present), nullptr);
    EXPECT_EQ(*GetValuePtr(readOnly), 21);
    *ValuePtr(present) = 22;
    EXPECT_EQ(present.value(), 22);
    EXPECT_EQ(ValuePtr(empty), nullptr);
    EXPECT_FALSE(HasValuePtr(empty));
}

TEST(OptionalContractTest, PresenceAliasesAgreeForPresentAndEmptyValues)
{
    const Optional<int> present = 1;
    const Optional<int> empty;
    EXPECT_TRUE(HasAnyValue(present));
    EXPECT_TRUE(HasAny(present));
    EXPECT_TRUE(IsPresent(present));
    EXPECT_TRUE(IsPresentValue(present));
    EXPECT_TRUE(HasContent(present));
    EXPECT_TRUE(ContainsValue(present));
    EXPECT_TRUE(IsPopulated(present));
    EXPECT_TRUE(HasData(present));
    EXPECT_TRUE(NotEmpty(present));
    EXPECT_TRUE(HasElements(present));
    EXPECT_TRUE(HasEntries(present));
    EXPECT_TRUE(Some(present));
    EXPECT_TRUE(HasSome(present));
    EXPECT_TRUE(Any(present));
    EXPECT_TRUE(Exists(present));
    EXPECT_TRUE(Present(present));
    EXPECT_TRUE(Available(present));
    EXPECT_TRUE(ExistsValue(present));
    EXPECT_TRUE(HasPresence(present));
    EXPECT_TRUE(HasState(present));
    EXPECT_TRUE(IsPresentState(present));
    EXPECT_TRUE(HasBeing(present));
    EXPECT_TRUE(IsThere(present));
    EXPECT_TRUE(HasExistence(present));
    EXPECT_TRUE(IsExisting(present));
    EXPECT_TRUE(HasBeingValue(present));
    EXPECT_TRUE(IsAlive(present));
    EXPECT_TRUE(IsPresentBeing(present));
    EXPECT_TRUE(HasValueState(present));
    EXPECT_TRUE(ExistsState(present));
    EXPECT_TRUE(StateExists(present));
    EXPECT_TRUE(HasStoredValue(present));
    EXPECT_TRUE(IsStored(present));
    EXPECT_TRUE(HasKeptValue(present));
    EXPECT_TRUE(HasContainedValue(present));
    EXPECT_TRUE(IsHeld(present));
    EXPECT_TRUE(HasKept(present));
    EXPECT_TRUE(IsKept(present));
    EXPECT_TRUE(HasKeptValueState(present));
    EXPECT_TRUE(IsKeptValue(present));
    EXPECT_TRUE(HasKeptState(present));
    EXPECT_TRUE(IsKeptState(present));
    EXPECT_TRUE(HasKeptPresence(present));
    EXPECT_TRUE(IsKeptPresence(present));
    EXPECT_TRUE(HasKeptStateValue(present));
    EXPECT_TRUE(IsStoredValue(present));

    EXPECT_FALSE(HasAnyValue(empty));
    EXPECT_FALSE(HasAny(empty));
    EXPECT_FALSE(IsPresent(empty));
    EXPECT_FALSE(IsPresentValue(empty));
    EXPECT_FALSE(HasContent(empty));
    EXPECT_FALSE(ContainsValue(empty));
    EXPECT_FALSE(IsPopulated(empty));
    EXPECT_FALSE(HasData(empty));
    EXPECT_FALSE(NotEmpty(empty));
    EXPECT_FALSE(HasElements(empty));
    EXPECT_FALSE(HasEntries(empty));
    EXPECT_FALSE(Some(empty));
    EXPECT_FALSE(HasSome(empty));
    EXPECT_FALSE(Any(empty));
    EXPECT_FALSE(Exists(empty));
    EXPECT_FALSE(Present(empty));
    EXPECT_FALSE(Available(empty));
    EXPECT_FALSE(ExistsValue(empty));
    EXPECT_FALSE(HasPresence(empty));
    EXPECT_FALSE(HasState(empty));
    EXPECT_FALSE(IsPresentState(empty));
    EXPECT_FALSE(HasBeing(empty));
    EXPECT_FALSE(IsThere(empty));
    EXPECT_FALSE(HasExistence(empty));
    EXPECT_FALSE(IsExisting(empty));
    EXPECT_FALSE(HasBeingValue(empty));
    EXPECT_FALSE(IsAlive(empty));
    EXPECT_FALSE(IsPresentBeing(empty));
    EXPECT_FALSE(HasValueState(empty));
    EXPECT_FALSE(ExistsState(empty));
    EXPECT_FALSE(StateExists(empty));
    EXPECT_FALSE(HasStoredValue(empty));
    EXPECT_FALSE(IsStored(empty));
    EXPECT_FALSE(HasKeptValue(empty));
    EXPECT_FALSE(HasContainedValue(empty));
    EXPECT_FALSE(IsHeld(empty));
    EXPECT_FALSE(HasKept(empty));
    EXPECT_FALSE(IsKept(empty));
    EXPECT_FALSE(HasKeptValueState(empty));
    EXPECT_FALSE(IsKeptValue(empty));
    EXPECT_FALSE(HasKeptState(empty));
    EXPECT_FALSE(IsKeptState(empty));
    EXPECT_FALSE(HasKeptPresence(empty));
    EXPECT_FALSE(IsKeptPresence(empty));
    EXPECT_FALSE(HasKeptStateValue(empty));
    EXPECT_FALSE(IsStoredValue(empty));
}

TEST(OptionalContractTest, EmptyAndPointerAliasesTrackTheOptionalState)
{
    const Optional<int> present = 3;
    const Optional<int> empty;
    EXPECT_FALSE(IsEmpty(present));
    EXPECT_TRUE(IsEmpty(empty));
    EXPECT_NE(Ptr(present), nullptr);
    EXPECT_NE(ToPtr(present), nullptr);
    EXPECT_EQ(Ptr(empty), nullptr);
    EXPECT_EQ(ToPtr(empty), nullptr);
}

TEST(OptionalContractTest, ToResultUsesFailedAsTheDefaultErrorCode)
{
    const auto success = ToResult(Optional<std::string>{"ok"});
    const auto failure = ToResult(Optional<std::string>{});
    EXPECT_EQ(success.value(), "ok");
    EXPECT_EQ(failure.error(), ErrorCode::Failed);
}

TEST(OptionalContractTest, TransformOrElseConvertsMappedValueToRequestedType)
{
    int calls = 0;
    const int mapped = TransformOrElse(Optional<std::string>{"abcd"},
        [&calls](const std::string& value) {
            ++calls;
            return value.size();
        }, 99);
    EXPECT_EQ(mapped, 4);
    EXPECT_EQ(calls, 1);

    const int fallback = TransformOrElse(Optional<std::string>{},
        [&calls](const std::string& value) {
            ++calls;
            return value.size();
        }, 99);
    EXPECT_EQ(fallback, 99);
    EXPECT_EQ(calls, 1);
}

TEST(OptionalContractTest, PresentPointerHelpersReturnTheStoredObjectAddress)
{
    Optional<std::string> value{"stable"};
    const std::string* constAddress = GetValuePtr(value);
    std::string* mutableAddress = ValuePtr(value);
    ASSERT_NE(constAddress, nullptr);
    EXPECT_EQ(constAddress, mutableAddress);
    EXPECT_TRUE(HasValuePtr(value));
    mutableAddress->append("-updated");
    EXPECT_EQ(value.value(), "stable-updated");
}

TEST(OptionalContractTest, ValueOrCopiesPresentOwningValueAndUsesEmptyFallback)
{
    const Optional<std::string> present{"stored"};
    const Optional<std::string> empty;
    EXPECT_EQ(ValueOr(present, std::string("fallback")), "stored");
    EXPECT_EQ(ValueOr(empty, std::string("fallback")), "fallback");
    EXPECT_EQ(*present, "stored");
}

TEST(OptionalContractTest, TransformAndThenSupportMoveOnlyMappedResults)
{
    const Optional<int> source{31};
    const auto transformed = Transform(source, [](int value) {
        return std::make_unique<int>(value + 1);
    });
    const auto chained = AndThen(source, [](int value) {
        return Optional<std::unique_ptr<int>>{std::make_unique<int>(value + 2)};
    });

    ASSERT_TRUE(transformed.has_value());
    ASSERT_TRUE(*transformed);
    EXPECT_EQ(**transformed, 32);
    ASSERT_TRUE(chained.has_value());
    ASSERT_TRUE(*chained);
    EXPECT_EQ(**chained, 33);
}
