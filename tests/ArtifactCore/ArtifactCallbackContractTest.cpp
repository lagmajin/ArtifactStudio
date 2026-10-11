#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>

import Core.ArtifactCallback;

using ArtifactCore::Action;
using ArtifactCore::Callback;

static_assert(std::is_copy_constructible_v<Callback<int(int)>>);
static_assert(std::is_move_constructible_v<Callback<int(int)>>);

TEST(ArtifactCallbackContractTest, DefaultCallbackIsInvalid)
{
    const Callback<void()> callback;
    EXPECT_FALSE(callback.isValid());
    EXPECT_FALSE(static_cast<bool>(callback));
}

TEST(ArtifactCallbackContractTest, InvokesCallableWithArguments)
{
    Callback<int(int, int)> callback([](int left, int right) { return left + right; });
    ASSERT_TRUE(callback.isValid());
    EXPECT_EQ(callback.invoke(7, 8), 15);
}

TEST(ArtifactCallbackContractTest, ForwardsMoveOnlyArgument)
{
    Callback<int(std::unique_ptr<int>)> callback(
        [](std::unique_ptr<int> value) { return *value; });
    EXPECT_EQ(callback.invoke(std::make_unique<int>(8)), 8);
}

TEST(ArtifactCallbackContractTest, ForwardsMoveOnlyReturnValue)
{
    Callback<std::unique_ptr<int>()> callback(
        [] { return std::make_unique<int>(73); });

    std::unique_ptr<int> result = callback.invoke();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 73);
}

TEST(ArtifactCallbackContractTest, PropagatesCallableException)
{
    Callback<void()> callback([] { throw std::runtime_error("callback failure"); });

    EXPECT_THROW(callback.invoke(), std::runtime_error);
}

TEST(ArtifactCallbackContractTest, PreservesMutableCallableState)
{
    int calls = 0;
    Callback<int()> callback([calls]() mutable { return ++calls; });
    EXPECT_EQ(callback.invoke(), 1);
    EXPECT_EQ(callback.invoke(), 2);
}

TEST(ArtifactCallbackContractTest, ReturnsReferenceWithoutCopying)
{
    int value = 21;
    Callback<int&()> callback([&value]() -> int& { return value; });
    callback.invoke() = 42;
    EXPECT_EQ(value, 42);
}

TEST(ArtifactCallbackContractTest, ClearMakesCallbackInvalid)
{
    Callback<int()> callback([] { return 1; });
    ASSERT_TRUE(callback);
    callback.clear();
    EXPECT_FALSE(callback.isValid());
}

TEST(ArtifactCallbackContractTest, ClearReleasesOwnedCallableCapture)
{
    auto target = std::make_shared<int>(7);
    std::weak_ptr<int> observed = target;
    Callback<void()> callback([owned = std::move(target)] { (void)owned; });
    ASSERT_FALSE(target);
    ASSERT_FALSE(observed.expired());

    callback.clear();

    EXPECT_TRUE(observed.expired());
}

TEST(ArtifactCallbackContractTest, ResetReplacesExistingTarget)
{
    Callback<int()> callback([] { return 1; });
    callback.reset([] { return 2; });
    ASSERT_TRUE(callback);
    EXPECT_EQ(callback.invoke(), 2);
}

TEST(ArtifactCallbackContractTest, ResetReleasesOldCaptureAndOwnsReplacement)
{
    auto oldTarget = std::make_shared<int>(1);
    std::weak_ptr<int> oldObserved = oldTarget;
    Callback<void()> callback([owned = std::move(oldTarget)] { (void)owned; });
    auto newTarget = std::make_shared<int>(2);
    std::weak_ptr<int> newObserved = newTarget;

    callback.reset([owned = std::move(newTarget)] { (void)owned; });

    EXPECT_TRUE(oldObserved.expired());
    EXPECT_FALSE(newObserved.expired());
    callback.invoke();
}

TEST(ArtifactCallbackContractTest, ResetCanRestoreClearedCallback)
{
    Callback<int()> callback([] { return 1; });
    callback.clear();
    callback.reset([] { return 9; });
    EXPECT_EQ(callback.invoke(), 9);
}

TEST(ArtifactCallbackContractTest, CopyHasIndependentCallableState)
{
    Callback<int()> original([value = 0]() mutable { return ++value; });
    Callback<int()> copy(original);
    EXPECT_EQ(original.invoke(), 1);
    EXPECT_EQ(original.invoke(), 2);
    EXPECT_EQ(copy.invoke(), 1);
}

TEST(ArtifactCallbackContractTest, ActionInvokesNoArgumentCallable)
{
    int calls = 0;
    Action action([&calls] { ++calls; });
    action.invoke();
    action.invoke();
    EXPECT_EQ(calls, 2);
}

TEST(ArtifactCallbackContractTest, EmptyStdFunctionConstructsInvalidCallback)
{
    const std::function<int()> empty;
    const Callback<int()> callback(empty);
    EXPECT_FALSE(callback.isValid());
    EXPECT_FALSE(static_cast<bool>(callback));
}

TEST(ArtifactCallbackContractTest, ResetToEmptyStdFunctionClearsTarget)
{
    Callback<int()> callback([] { return 42; });
    const std::function<int()> empty;
    callback.reset(empty);
    EXPECT_FALSE(callback.isValid());
}

TEST(ArtifactCallbackContractTest, ClearIsIdempotent)
{
    Callback<void()> callback([] {});
    callback.clear();
    callback.clear();
    EXPECT_FALSE(callback);
}

TEST(ArtifactCallbackContractTest, VoidCallbackForwardsReferenceAndMoveOnlyValue)
{
    int observed = 0;
    Callback<void(int&, std::unique_ptr<int>)> callback(
        [&observed](int& value, std::unique_ptr<int> owned) {
            value += *owned;
            observed = value;
        });
    int value = 11;
    callback.invoke(value, std::make_unique<int>(31));
    EXPECT_EQ(value, 42);
    EXPECT_EQ(observed, 42);
}

TEST(ArtifactCallbackContractTest, CopyAssignmentReplacesAndOwnsTargetCapture)
{
    auto oldTarget = std::make_shared<int>(1);
    std::weak_ptr<int> oldObserved = oldTarget;
    Callback<void()> destination([owned = std::move(oldTarget)] { (void)owned; });
    auto newTarget = std::make_shared<int>(2);
    std::weak_ptr<int> newObserved = newTarget;
    Callback<void()> source([owned = std::move(newTarget)] { (void)owned; });

    destination = source;

    EXPECT_TRUE(oldObserved.expired());
    EXPECT_FALSE(newObserved.expired());
    source.clear();
    EXPECT_FALSE(newObserved.expired());
    destination.invoke();
    destination.clear();
    EXPECT_TRUE(newObserved.expired());
}

TEST(ArtifactCallbackContractTest, CopyAssignmentFromEmptyClearsExistingTarget)
{
    Callback<int()> destination([] { return 42; });
    const Callback<int()> empty;

    destination = empty;

    EXPECT_FALSE(destination.isValid());
    EXPECT_FALSE(static_cast<bool>(destination));
}
