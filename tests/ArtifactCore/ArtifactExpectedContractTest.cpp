#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

import Core.ArtifactExpected;

using namespace ArtifactCore;

static_assert(std::is_same_v<decltype(std::declval<ArtifactExpected<int>&>().value()), int&>);
static_assert(std::is_same_v<decltype(std::declval<const ArtifactExpected<int>&>().value()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<ArtifactExpected<int>&&>().value()), int&&>);

namespace {

struct NumericError {
    int code = 0;
};

struct CountingIntFallback {
    int* conversions;

    operator int() const
    {
        ++*conversions;
        return 41;
    }
};

struct CountingErrorFallback {
    int* conversions;

    operator NumericError() const
    {
        ++*conversions;
        return NumericError{99};
    }
};

} // namespace

TEST(ArtifactExpectedContractTest, BoolAndAccessorsTrackValueAndErrorStates)
{
    ArtifactExpected<int> success(12);
    ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    EXPECT_TRUE(success);
    EXPECT_TRUE(success.hasValue());
    EXPECT_FALSE(success.hasError());
    EXPECT_FALSE(failure);
    EXPECT_FALSE(failure.hasValue());
    EXPECT_TRUE(failure.hasError());
}

TEST(ArtifactExpectedContractTest, DefaultConstructionStartsAsUnknownError)
{
    const ArtifactExpected<int> value;

    EXPECT_FALSE(value);
    EXPECT_FALSE(value.hasValue());
    EXPECT_TRUE(value.hasError());
    EXPECT_EQ(value.error().code, ArtifactExpectedErrorCode::Unknown);
    EXPECT_EQ(value.valueOr(17), 17);
}

TEST(ArtifactExpectedContractTest, ValueOrAndErrorOrSelectTheActiveAlternative)
{
    const ArtifactExpectedError fallback{ArtifactExpectedErrorCode::Failed, "fallback"};
    ArtifactExpected<int> success(5);
    ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    EXPECT_EQ(success.valueOr(9), 5);
    EXPECT_EQ(failure.valueOr(9), 9);
    EXPECT_EQ(success.errorOr(fallback).code, ArtifactExpectedErrorCode::Failed);
    EXPECT_EQ(failure.errorOr(fallback).code, ArtifactExpectedErrorCode::NotFound);
}

TEST(ArtifactExpectedContractTest, ValueOrConvertsFallbackOnlyWhenValueIsAbsent)
{
    const ArtifactExpected<int> success(7);
    const ArtifactExpected<int> failure(
        ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    int conversions = 0;
    const CountingIntFallback fallback{&conversions};

    EXPECT_EQ(success.valueOr(fallback), 7);
    EXPECT_EQ(conversions, 0);
    EXPECT_EQ(failure.valueOr(fallback), 41);
    EXPECT_EQ(conversions, 1);
}

TEST(ArtifactExpectedContractTest, ErrorOrConvertsFallbackOnlyWhenErrorIsAbsent)
{
    const ArtifactExpected<int> success(7);
    const ArtifactExpected<int> failure(
        ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    int conversions = 0;
    const CountingErrorFallback fallback{&conversions};

    EXPECT_EQ(success.errorOr(fallback).code, 99);
    EXPECT_EQ(conversions, 1);
    EXPECT_EQ(failure.errorOr(fallback).code, ArtifactExpectedErrorCode::NotFound);
    EXPECT_EQ(conversions, 1);
}

TEST(ArtifactExpectedContractTest, ToOptionalPreservesValueAndDropsError)
{
    const ArtifactExpected<int> success(21);
    const ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::Failed, "failed"});
    EXPECT_TRUE(success.toOptional().has_value());
    EXPECT_EQ(success.toOptional().value(), 21);
    EXPECT_FALSE(failure.toOptional().has_value());
}

TEST(ArtifactExpectedContractTest, AndThenInvokesOnlyForValue)
{
    const ArtifactExpected<int> success(6);
    const ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    int calls = 0;
    const auto multiply = [&calls](const int value) {
        ++calls;
        return ArtifactExpected<long>(value * 3L);
    };
    const auto mappedSuccess = success.andThen(multiply);
    const auto mappedFailure = failure.andThen(multiply);
    EXPECT_EQ(mappedSuccess.value(), 18L);
    EXPECT_EQ(mappedFailure.error().code, ArtifactExpectedErrorCode::NotFound);
    EXPECT_EQ(calls, 1);
}

TEST(ArtifactExpectedContractTest, OrElseInvokesOnlyForError)
{
    const ArtifactExpected<int> success(8);
    const ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    int calls = 0;
    const auto recover = [&calls](const ArtifactExpectedError&) {
        ++calls;
        return ArtifactExpected<int>(13);
    };
    EXPECT_EQ(success.orElse(recover).value(), 8);
    EXPECT_EQ(failure.orElse(recover).value(), 13);
    EXPECT_EQ(calls, 1);
}

TEST(ArtifactExpectedContractTest, TransformMapsValuesAndPreservesErrors)
{
    const ArtifactExpected<int> success(4);
    const ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::Failed, "failed"});
    const auto mappedSuccess = success.transform([](const int value) { return std::to_string(value * 2); });
    const auto mappedFailure = failure.transform([](const int value) { return value * 2; });
    EXPECT_EQ(mappedSuccess.value(), "8");
    EXPECT_EQ(mappedFailure.error().code, ArtifactExpectedErrorCode::Failed);
}

TEST(ArtifactExpectedContractTest, TransformCallbacksRunOnlyForTheirActiveAlternative)
{
    const ArtifactExpected<int> success(7);
    const ArtifactExpected<int> failure(
        ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    int valueTransformCalls = 0;
    int errorTransformCalls = 0;

    const auto stillFailure = failure.transform([&valueTransformCalls](const int value) {
        ++valueTransformCalls;
        return value * 2;
    });
    const auto stillSuccess = success.transformError([&errorTransformCalls](
        const ArtifactExpectedError&) {
        ++errorTransformCalls;
        return NumericError{42};
    });

    EXPECT_EQ(valueTransformCalls, 0);
    ASSERT_TRUE(stillFailure.hasError());
    EXPECT_EQ(stillFailure.error().code, ArtifactExpectedErrorCode::NotFound);
    EXPECT_EQ(errorTransformCalls, 0);
    ASSERT_TRUE(stillSuccess.hasValue());
    EXPECT_EQ(stillSuccess.value(), 7);
}

TEST(ArtifactExpectedContractTest, TransformErrorMapsOnlyErrors)
{
    const ArtifactExpected<int> success(7);
    const ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    const auto mappedSuccess = success.transformError([](const ArtifactExpectedError&) { return NumericError{99}; });
    const auto mappedFailure = failure.transformError([](const ArtifactExpectedError&) { return NumericError{42}; });
    EXPECT_EQ(mappedSuccess.value(), 7);
    EXPECT_EQ(mappedFailure.error().code, 42);
}

TEST(ArtifactExpectedContractTest, SupportsCustomErrorTypes)
{
    ArtifactExpected<int, NumericError> failure(NumericError{17});
    EXPECT_TRUE(failure.hasError());
    EXPECT_EQ(failure.error().code, 17);
    EXPECT_EQ(failure.valueOr(3), 3);
    EXPECT_EQ(failure.errorOr(NumericError{2}).code, 17);
}

TEST(ArtifactExpectedContractTest, RvalueValueAccessorMovesTheStoredValue)
{
    ArtifactExpected<std::string> success(std::string("payload"));
    std::string extracted = std::move(success).value();
    EXPECT_EQ(extracted, "payload");
}

TEST(ArtifactExpectedContractTest, MutableValueAccessorUpdatesStoredValue)
{
    ArtifactExpected<int> success(12);
    success.value() = 31;
    EXPECT_EQ(success.value(), 31);
}

TEST(ArtifactExpectedContractTest, MutableErrorAccessorUpdatesStoredError)
{
    ArtifactExpected<int> failure(ArtifactExpectedError{ArtifactExpectedErrorCode::Failed, "old"});
    failure.error().code = ArtifactExpectedErrorCode::InvalidArgument;
    EXPECT_EQ(failure.error().code, ArtifactExpectedErrorCode::InvalidArgument);
}

TEST(ArtifactExpectedContractTest, AndThenPreservesCustomErrorTypeOnFailure)
{
    ArtifactExpected<int, NumericError> failure(NumericError{27});
    int calls = 0;
    const auto result = failure.andThen([&calls](const int value) {
        ++calls;
        return ArtifactExpected<long, NumericError>(value * 2L);
    });
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(result.error().code, 27);
}

TEST(ArtifactExpectedContractTest, OrElsePreservesExistingCustomErrorWhenReturningError)
{
    ArtifactExpected<int, NumericError> failure(NumericError{8});
    int calls = 0;
    const auto result = failure.orElse([&calls](const NumericError& error) {
        ++calls;
        return ArtifactExpected<int, NumericError>(error);
    });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(result.error().code, 8);
}

TEST(ArtifactExpectedContractTest, TransformErrorChangesErrorTypeAndKeepsValueOnSuccess)
{
    const ArtifactExpected<int> success(14);
    const auto transformed = success.transformError([](const ArtifactExpectedError&) {
        return NumericError{99};
    });
    ASSERT_TRUE(transformed.hasValue());
    EXPECT_EQ(transformed.value(), 14);
}

TEST(ArtifactExpectedContractTest, MoveOnlyValueCanBeStoredAndMovedOut)
{
    ArtifactExpected<std::unique_ptr<int>> success(std::make_unique<int>(73));
    std::unique_ptr<int> extracted = std::move(success).value();
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(*extracted, 73);
}

TEST(ArtifactExpectedContractTest, MoveOnlyErrorCanBeStoredAndMovedOut)
{
    ArtifactExpected<int, std::unique_ptr<int>> failure(
        std::make_unique<int>(81));
    std::unique_ptr<int> extracted = std::move(failure.error());

    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(*extracted, 81);
    EXPECT_TRUE(failure.hasError());
}

TEST(ArtifactExpectedContractTest, TransformCanProduceMoveOnlyValue)
{
    const ArtifactExpected<int> source(29);
    const auto transformed = source.transform([](const int value) {
        return std::make_unique<int>(value + 1);
    });

    ASSERT_TRUE(transformed.hasValue());
    ASSERT_NE(transformed.value(), nullptr);
    EXPECT_EQ(*transformed.value(), 30);
}

TEST(ArtifactExpectedContractTest, TransformErrorCanProduceMoveOnlyError)
{
    const ArtifactExpected<int> source(
        ArtifactExpectedError{ArtifactExpectedErrorCode::NotFound, "missing"});
    const auto transformed = source.transformError([](const ArtifactExpectedError& error) {
        return std::make_unique<std::string>(error.message.data());
    });

    ASSERT_TRUE(transformed.hasError());
    ASSERT_NE(transformed.error(), nullptr);
    EXPECT_EQ(*transformed.error(), "missing");
}

TEST(ArtifactExpectedContractTest, ErrorOrCopiesActiveErrorForCustomErrorType)
{
    const ArtifactExpected<int, NumericError> failure(NumericError{55});
    const NumericError selected = failure.errorOr(NumericError{2});
    EXPECT_EQ(selected.code, 55);
}

TEST(ArtifactExpectedContractTest, CopyAndMovePreserveTheActiveAlternative)
{
    using Expected = ArtifactExpected<std::string, NumericError>;
    const Expected success(std::string("payload"));
    const Expected failure(NumericError{73});

    Expected copiedSuccess(success);
    Expected copiedFailure(failure);
    Expected movedSuccess(std::move(copiedSuccess));
    Expected movedFailure(std::move(copiedFailure));

    ASSERT_TRUE(movedSuccess.hasValue());
    EXPECT_EQ(movedSuccess.value(), "payload");
    ASSERT_TRUE(movedFailure.hasError());
    EXPECT_EQ(movedFailure.error().code, 73);
    EXPECT_EQ(success.value(), "payload");
    EXPECT_EQ(failure.error().code, 73);
}

TEST(ArtifactExpectedContractTest, CopyAssignmentSwitchesBetweenValueAndErrorStates)
{
    using Expected = ArtifactExpected<std::string, NumericError>;
    Expected target(std::string("initial"));
    const Expected failure(NumericError{17});
    target = failure;
    ASSERT_TRUE(target.hasError());
    EXPECT_EQ(target.error().code, 17);

    const Expected replacement(std::string("recovered"));
    target = replacement;
    ASSERT_TRUE(target.hasValue());
    EXPECT_EQ(target.value(), "recovered");

    target = target;
    EXPECT_EQ(target.value(), "recovered");
}

TEST(ArtifactExpectedContractTest, MoveAssignmentSwitchesStatesForMoveOnlyPayloads)
{
    using Expected = ArtifactExpected<std::unique_ptr<int>, std::unique_ptr<long>>;
    Expected destination(std::make_unique<int>(1));
    Expected failure(std::make_unique<long>(23));
    destination = std::move(failure);
    ASSERT_TRUE(destination.hasError());
    ASSERT_NE(destination.error(), nullptr);
    EXPECT_EQ(*destination.error(), 23);
    EXPECT_TRUE(failure.hasError());

    Expected success(std::make_unique<int>(42));
    destination = std::move(success);
    ASSERT_TRUE(destination.hasValue());
    ASSERT_NE(destination.value(), nullptr);
    EXPECT_EQ(*destination.value(), 42);
    EXPECT_TRUE(success.hasValue());
}

