#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

import Core.ArtifactString;
import Utils.Result;

using namespace ArtifactCore;

static_assert(std::is_same_v<decltype(std::declval<Result<int>&>().value()), int&>);
static_assert(std::is_same_v<decltype(std::declval<const Result<int>&>().value()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<Result<int>&&>().value()), int&&>);

TEST(ResultContractTest, ValueConstructionAndOkFactoriesExposeTheValue)
{
    const Result<int> copied(17);
    const Result<int> moved = Result<int>::ok(23);
    EXPECT_TRUE(copied.success());
    EXPECT_TRUE(copied.hasValue());
    EXPECT_EQ(copied.value(), 17);
    EXPECT_EQ(moved.value(), 23);
}

TEST(ResultContractTest, FailureFactoryHasNoValueAndRetainsErrorCode)
{
    const auto failed = Result<int>::fail(ErrorCode::NotFound);
    EXPECT_FALSE(failed.success());
    EXPECT_FALSE(failed.hasValue());
    EXPECT_TRUE(failed.hasError());
    EXPECT_EQ(failed.error(), ErrorCode::NotFound);
    EXPECT_EQ(failed.valuePtr(), nullptr);
}

TEST(ResultContractTest, ValueFallbacksUseTheFallbackOnlyWhenEmpty)
{
    const auto success = Result<int>::ok(8);
    const auto failure = Result<int>::fail(ErrorCode::Busy);
    EXPECT_EQ(success.valueOr(4), 8);
    EXPECT_EQ(failure.valueOr(4), 4);
    EXPECT_EQ(failure.valueOrElse(6), 6);
    EXPECT_EQ(failure.valueOrDefault(), 0);
}

TEST(ResultContractTest, ValueOrElseFactoryIsLazyOnSuccess)
{
    int fallbackCalls = 0;
    const auto success = Result<int>::ok(5);
    const auto failure = Result<int>::fail();
    EXPECT_EQ(success.valueOrElse([&] { ++fallbackCalls; return 9; }), 5);
    EXPECT_EQ(fallbackCalls, 0);
    EXPECT_EQ(failure.valueOrElse([&] { ++fallbackCalls; return 9; }), 9);
    EXPECT_EQ(fallbackCalls, 1);
}

TEST(ResultContractTest, TransformMapsSuccessAndPreservesFailure)
{
    const auto success = Result<int>::ok(7).transform([](int value) { return value * 3; });
    const auto failure = Result<int>::fail(ErrorCode::Cancelled)
                             .transform([](int value) { return value * 3; });
    EXPECT_EQ(success.value(), 21);
    EXPECT_FALSE(failure.hasValue());
    EXPECT_EQ(failure.error(), ErrorCode::Cancelled);
}

TEST(ResultContractTest, AndThenShortCircuitsFailureAndChainsSuccess)
{
    int calls = 0;
    const auto success = Result<int>::ok(4).andThen([&](int value) {
        ++calls;
        return Result<std::string>::ok(std::to_string(value));
    });
    const auto failure = Result<int>::fail(ErrorCode::Busy).andThen([&](int value) {
        ++calls;
        return Result<std::string>::ok(std::to_string(value));
    });
    EXPECT_EQ(success.value(), "4");
    EXPECT_FALSE(failure.hasValue());
    EXPECT_EQ(failure.error(), ErrorCode::Busy);
    EXPECT_EQ(calls, 1);
}

TEST(ResultContractTest, OrElseRecoversFailureButLeavesSuccessUntouched)
{
    int recoveryCalls = 0;
    const auto success = Result<int>::ok(3).orElse([&](const Status&) {
        ++recoveryCalls;
        return Result<int>::ok(10);
    });
    const auto recovered = Result<int>::fail(ErrorCode::NotFound).orElse([&](const Status& status) {
        ++recoveryCalls;
        return status.error == ErrorCode::NotFound ? Result<int>::ok(10) : Result<int>::fail();
    });
    EXPECT_EQ(success.value(), 3);
    EXPECT_EQ(recovered.value(), 10);
    EXPECT_EQ(recoveryCalls, 1);
}

TEST(ResultContractTest, PointerAndOptionalViewsReflectValuePresence)
{
    const auto success = Result<int>::ok(12);
    const auto failure = Result<int>::fail();
    ASSERT_NE(success.valuePtr(), nullptr);
    EXPECT_EQ(*success.getIfValue(), 12);
    EXPECT_EQ(success.maybe(), 12);
    EXPECT_EQ(success.toOptional(), 12);
    EXPECT_FALSE(failure.maybe().has_value());
    EXPECT_FALSE(failure.toOptional().has_value());
}

TEST(ResultContractTest, StatusFactoriesKeepSuccessAndNormalizeMissingFailureCode)
{
    const auto ok = Status::ok();
    const auto failed = Status::fail(ErrorCode::None);
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_EQ(ok.error, ErrorCode::None);
    EXPECT_FALSE(static_cast<bool>(failed));
    EXPECT_EQ(failed.error, ErrorCode::Failed);
}

TEST(ResultContractTest, ErrorContextAndSourceLocationSurviveFailureConstruction)
{
    const SourceLocation location = sourceLocation("source.cpp", "operation", 42);
    const auto result = Result<int>::fail(ErrorCode::InvalidArgument,
                                          String("bad value"),
                                          String("parse"),
                                          String("item-7"),
                                          location);
    const auto& context = result.errorContext();
    EXPECT_EQ(context.code, ErrorCode::InvalidArgument);
    EXPECT_EQ(context.message, "bad value");
    EXPECT_EQ(context.operation, "parse");
    EXPECT_EQ(context.objectId, "item-7");
    EXPECT_TRUE(context.location.hasValue());
    EXPECT_EQ(context.location.line, 42);
}

TEST(ResultContractTest, ErrorCodeNamesAreStableAndUnknownValuesFallBack)
{
    EXPECT_STREQ(errorCodeName(ErrorCode::None), "none");
    EXPECT_STREQ(errorCodeName(ErrorCode::Cancelled), "cancelled");
    EXPECT_STREQ(errorCodeName(static_cast<ErrorCode>(255)), "unknown");
}

TEST(ResultContractTest, VoidResultIsUsableAsStatus)
{
    const VoidResult success = VoidResult::ok();
    const VoidResult failure = VoidResult::fail(ErrorCode::Busy);
    EXPECT_TRUE(static_cast<bool>(success));
    EXPECT_FALSE(static_cast<bool>(failure));
    EXPECT_EQ(failure.error, ErrorCode::Busy);
}

TEST(ResultContractTest, ConvenienceFactoriesAndOptionalConversionPreservePresence)
{
    const auto made = MakeResult(31);
    const auto missing = MakeFailedResult<int>(ErrorCode::AlreadyExists);
    const ResultOptional<int> present = 47;
    const ResultOptional<int> absent;
    const auto fromPresent = FromOptional(present, ErrorCode::NotFound);
    const auto fromAbsent = FromOptional(absent, ErrorCode::NotFound);
    EXPECT_EQ(made.value(), 31);
    EXPECT_EQ(missing.error(), ErrorCode::AlreadyExists);
    EXPECT_EQ(fromPresent.value(), 47);
    EXPECT_EQ(fromAbsent.error(), ErrorCode::NotFound);
}

TEST(ResultContractTest, FromOptionalOwnsCopiedValueAfterSourceIsCleared)
{
    ResultOptional<std::string> source = std::string("independent value");
    const auto result = FromOptional(source, ErrorCode::NotFound);
    source.reset();

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), "independent value");
}

TEST(ResultContractTest, StateAliasesAndErrorFallbackAgreeWithStatus)
{
    const auto success = Result<int>::ok(2);
    const auto failure = Result<int>::fail(ErrorCode::Cancelled);
    EXPECT_TRUE(success.isOk());
    EXPECT_TRUE(success.succeeded());
    EXPECT_TRUE(success.passed());
    EXPECT_TRUE(success.statusOk());
    EXPECT_EQ(success.errorOr(ErrorCode::Busy), ErrorCode::Busy);
    EXPECT_TRUE(failure.isFailed());
    EXPECT_TRUE(failure.failed());
    EXPECT_TRUE(failure.empty());
    EXPECT_EQ(failure.errorOr(ErrorCode::Busy), ErrorCode::Cancelled);
}

TEST(ResultContractTest, MutableValuePointerAndIfValueExposeOnlySuccessfulValue)
{
    auto success = Result<int>::ok(6);
    auto failure = Result<int>::fail();
    int callbackCalls = 0;
    EXPECT_TRUE(success.hasValuePtr());
    EXPECT_EQ(success.tryValue(), success.valuePtr());
    *success.tryValue() = 14;
    success.ifValue([&](int value) { callbackCalls += value; });
    failure.ifValue([&](int) { ++callbackCalls; });
    EXPECT_EQ(success.value(), 14);
    EXPECT_EQ(callbackCalls, 14);
    EXPECT_FALSE(failure.hasValuePtr());
    EXPECT_EQ(failure.tryValue(), nullptr);
}

TEST(ResultContractTest, MapIsAnAliasForTransform)
{
    const auto mapped = Result<std::string>::ok("frame")
                            .map([](const std::string& value) { return value.size(); });
    EXPECT_EQ(mapped.value(), 5u);
}

TEST(ResultContractTest, ExplicitErrorContextKeepsTraceAndNormalizesItsCode)
{
    ErrorContext context;
    context.code = ErrorCode::None;
    context.message = "detail";
    context.traceId = 9981;
    const auto status = Status::fail(std::move(context));
    EXPECT_EQ(status.error, ErrorCode::Failed);
    EXPECT_EQ(status.context.code, ErrorCode::Failed);
    EXPECT_EQ(status.context.message, "detail");
    EXPECT_EQ(status.context.traceId, 9981u);
}

TEST(ResultContractTest, SourceLocationRequiresFileAndPositiveLine)
{
    EXPECT_TRUE(sourceLocation("file.cpp", "fn", 1).hasValue());
    EXPECT_FALSE(sourceLocation("", "fn", 1).hasValue());
    EXPECT_FALSE(sourceLocation("file.cpp", "fn", 0).hasValue());
    EXPECT_FALSE(sourceLocation(nullptr, "fn", 4).hasValue());
}

TEST(ResultContractTest, UnwrapOrSelectsTheStoredValueOrFallback)
{
    const auto success = Result<std::string>::ok("stored");
    const auto failure = Result<std::string>::fail(ErrorCode::Busy);
    EXPECT_EQ(success.unwrapOr("fallback"), "stored");
    EXPECT_EQ(failure.unwrapOr("fallback"), "fallback");
}

TEST(ResultContractTest, FailureFromContextPreservesAllDiagnosticFields)
{
    ErrorContext context;
    context.code = ErrorCode::InvalidArgument;
    context.message = "bad input";
    context.operation = "decode";
    context.objectId = "asset-42";
    context.location = sourceLocation("decoder.cpp", "decodeAsset", 73);
    context.traceId = 0x1234;

    const auto result = Result<int>::fail(std::move(context));
    const auto& actual = result.errorContext();
    EXPECT_EQ(actual.code, ErrorCode::InvalidArgument);
    EXPECT_EQ(actual.message, "bad input");
    EXPECT_EQ(actual.operation, "decode");
    EXPECT_EQ(actual.objectId, "asset-42");
    ASSERT_TRUE(actual.location.hasValue());
    EXPECT_STREQ(actual.location.file, "decoder.cpp");
    EXPECT_STREQ(actual.location.function, "decodeAsset");
    EXPECT_EQ(actual.location.line, 73);
    EXPECT_EQ(actual.traceId, 0x1234u);
}

TEST(ResultContractTest, GetStatusAndStatusAreReferencesToTheStoredState)
{
    const auto failure = Result<int>::fail(ErrorCode::Cancelled);
    EXPECT_EQ(&failure.status(), &failure.getStatus());
    EXPECT_FALSE(static_cast<bool>(failure.status()));
    EXPECT_EQ(failure.getStatus().error, ErrorCode::Cancelled);
}

TEST(ResultContractTest, RvalueValueAccessorTransfersStoredValue)
{
    auto result = Result<std::string>::ok(std::string("payload"));
    const std::string extracted = std::move(result).value();
    EXPECT_EQ(extracted, "payload");
}

TEST(ResultContractTest, DefaultConstructionStartsAsFailure)
{
    const Result<int> result;
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error(), ErrorCode::Failed);
}

TEST(ResultContractTest, SuccessfulStatusCannotCreateValueLessResult)
{
    const Result<int> result(Status::ok());

    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
}

TEST(ResultContractTest, MutableValueAccessorUpdatesStoredValue)
{
    Result<int> result = Result<int>::ok(10);
    result.value() = 25;
    EXPECT_EQ(result.value(), 25);
    EXPECT_TRUE(result.success());
}

TEST(ResultContractTest, TransformDecaysReferenceResultsIntoOwnedValues)
{
    const Result<std::string> result = Result<std::string>::ok("owned");
    const auto transformed = result.transform([](const std::string& value) -> const std::string& {
        return value;
    });
    static_assert(std::is_same_v<decltype(transformed), const Result<std::string>>);
    ASSERT_TRUE(transformed.hasValue());
    EXPECT_EQ(transformed.value(), "owned");
}

TEST(ResultContractTest, TransformAndAndThenPreserveCompleteFailureContext)
{
    ErrorContext context;
    context.code = ErrorCode::Busy;
    context.message = "busy now";
    context.operation = "render";
    context.traceId = 517;
    const auto failure = Result<int>::fail(context);
    const auto transformed = failure.transform([](int value) { return value + 1; });
    const auto chained = failure.andThen([](int value) { return Result<std::string>::ok(std::to_string(value)); });
    EXPECT_EQ(transformed.errorContext().message, "busy now");
    EXPECT_EQ(transformed.errorContext().operation, "render");
    EXPECT_EQ(transformed.errorContext().traceId, 517u);
    EXPECT_EQ(chained.errorContext().traceId, 517u);
}

TEST(ResultContractTest, MoveOnlyValueCanBeStoredAndMovedOut)
{
    auto result = Result<std::unique_ptr<int>>::ok(std::make_unique<int>(81));
    auto extracted = std::move(result).value();
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(*extracted, 81);
}

TEST(ResultContractTest, ValueOrElseValueOverloadSelectsStoredOrFallbackValue)
{
    const auto success = Result<std::string>::ok("present");
    const auto failure = Result<std::string>::fail(ErrorCode::Busy);
    EXPECT_EQ(success.valueOrElse(std::string("fallback")), "present");
    EXPECT_EQ(failure.valueOrElse(std::string("fallback")), "fallback");
}

TEST(ResultContractTest, ValueFallbackAccessorsDoNotEraseFailureDiagnostics)
{
    ErrorContext context;
    context.code = ErrorCode::Busy;
    context.message = "resource busy";
    context.operation = "open";
    const auto failure = Result<std::string>::fail(std::move(context));

    EXPECT_EQ(failure.valueOr("fallback"), "fallback");
    EXPECT_EQ(failure.unwrapOr("fallback"), "fallback");
    EXPECT_EQ(failure.valueOrElse(std::string("fallback")), "fallback");
    EXPECT_FALSE(failure.hasValue());
    EXPECT_EQ(failure.errorContext().message, "resource busy");
    EXPECT_EQ(failure.errorContext().operation, "open");
}

TEST(ResultContractTest, FailedStatusContextCodeIsNormalizedWithOtherFieldsPreserved)
{
    ErrorContext context;
    context.code = ErrorCode::None;
    context.message = "normalized";
    context.traceId = 99;
    const Status status = Status::fail(std::move(context));
    EXPECT_EQ(status.error, ErrorCode::Failed);
    EXPECT_EQ(status.context.code, ErrorCode::Failed);
    EXPECT_EQ(status.context.message, "normalized");
    EXPECT_EQ(status.context.traceId, 99u);
}

TEST(ResultContractTest, TransformAndAndThenSupportMoveOnlyValues)
{
    const auto source = Result<int>::ok(91);
    const auto transformed = source.transform([](int value) {
        return std::make_unique<int>(value + 1);
    });
    const auto chained = source.andThen([](int value) {
        return Result<std::unique_ptr<int>>::ok(std::make_unique<int>(value + 2));
    });

    ASSERT_TRUE(transformed.hasValue());
    ASSERT_NE(transformed.value(), nullptr);
    EXPECT_EQ(*transformed.value(), 92);
    ASSERT_TRUE(chained.hasValue());
    ASSERT_NE(chained.value(), nullptr);
    EXPECT_EQ(*chained.value(), 93);
}

TEST(ResultContractTest, IfValueRunsOnlyForSuccessAndReturnsTheSameResult)
{
    const Result<int> success = Result<int>::ok(23);
    const Result<int> failure = Result<int>::fail(ErrorCode::Busy);
    int calls = 0;
    int observed = 0;

    const Result<int>& chained = success.ifValue([&](int value) {
        ++calls;
        observed = value;
    });
    const Result<int>& unchangedFailure = failure.ifValue([&](int) {
        ++calls;
    });

    EXPECT_EQ(&chained, &success);
    EXPECT_EQ(&unchangedFailure, &failure);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(observed, 23);
}
