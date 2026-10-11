#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

import Core.ArtifactFunction;

using namespace ArtifactCore;

static_assert(!std::is_copy_constructible_v<ArtifactFunction<int()>>);
static_assert(!std::is_copy_assignable_v<ArtifactFunction<int()>>);
static_assert(std::is_nothrow_move_constructible_v<ArtifactFunction<int()>>);
static_assert(std::is_nothrow_move_assignable_v<ArtifactFunction<int()>>);

namespace {
struct CountedCallable {
    explicit CountedCallable(int& destroyCount) : destroyCount(&destroyCount) {}
    CountedCallable(CountedCallable&& other) noexcept
        : destroyCount(other.destroyCount) { other.destroyCount = nullptr; }
    ~CountedCallable() { if (destroyCount) ++*destroyCount; }

    int operator()() const { return 73; }
    int* destroyCount;
};
}

TEST(ArtifactFunctionContractTest, DefaultFunctionIsEmpty)
{
    ArtifactFunction<int()> function;
    EXPECT_FALSE(function);
    EXPECT_FALSE(static_cast<bool>(function));
}

TEST(ArtifactFunctionContractTest, StoresAndInvokesFunctionPointer)
{
    const auto add = [](int left, int right) { return left + right; };
    using FunctionPointer = int (*)(int, int);
    FunctionPointer pointer = +add;
    ArtifactFunction<int(int, int)> function(pointer);
    ASSERT_TRUE(function);
    EXPECT_EQ(function(17, 25), 42);
}

TEST(ArtifactFunctionContractTest, InvokesSmallCapturedCallable)
{
    int base = 9;
    ArtifactFunction<int(int)> function([base](int value) { return base + value; });
    EXPECT_EQ(function(4), 13);
}

TEST(ArtifactFunctionContractTest, StoresAndInvokesMoveOnlyInlineCallable)
{
    auto value = std::make_unique<int>(64);
    ArtifactFunction<int()> function(
        [owned = std::move(value)] { return *owned + 2; });

    EXPECT_FALSE(value);
    ASSERT_TRUE(function);
    EXPECT_EQ(function(), 66);
}

TEST(ArtifactFunctionContractTest, StoresAndInvokesMoveOnlyHeapCallable)
{
    auto value = std::make_unique<int>(67);
    std::array<int, 64> padding{};
    padding[29] = 3;
    ArtifactFunction<int()> function(
        [owned = std::move(value), padding] { return *owned + padding[29]; });

    EXPECT_FALSE(value);
    ASSERT_TRUE(function);
    EXPECT_EQ(function(), 70);
}

TEST(ArtifactFunctionContractTest, MutableCallableRetainsStateAcrossCalls)
{
    ArtifactFunction<int()> function([value = 0]() mutable { return ++value; });
    EXPECT_EQ(function(), 1);
    EXPECT_EQ(function(), 2);
    EXPECT_EQ(function(), 3);
}

TEST(ArtifactFunctionContractTest, ForwardsMutableReferenceArguments)
{
    ArtifactFunction<void(int&)> function([](int& value) { value *= 3; });
    int value = 14;
    function(value);
    EXPECT_EQ(value, 42);
}

TEST(ArtifactFunctionContractTest, SupportsVoidReturnSignature)
{
    int calls = 0;
    ArtifactFunction<void(int)> function([&calls](int amount) { calls += amount; });
    function(6);
    EXPECT_EQ(calls, 6);
}

TEST(ArtifactFunctionContractTest, AcceptsCallableWithConvertibleReturn)
{
    ArtifactFunction<double()> function([] { return 2; });
    EXPECT_DOUBLE_EQ(function(), 2.0);
}

TEST(ArtifactFunctionContractTest, ReferenceReturnPreservesAliasing)
{
    int value = 21;
    ArtifactFunction<int&()> function([&value]() -> int& { return value; });

    function() = 42;

    EXPECT_EQ(value, 42);
}

TEST(ArtifactFunctionContractTest, CallableExceptionPropagatesToCaller)
{
    ArtifactFunction<void()> function([] { throw std::runtime_error("function failure"); });

    EXPECT_THROW(function(), std::runtime_error);
}

TEST(ArtifactFunctionContractTest, LvalueCallableObservesExternalState)
{
    int calls = 0;
    auto callable = [&calls] { return ++calls; };
    ArtifactFunction<int()> function(callable);
    EXPECT_EQ(function(), 1);
    EXPECT_EQ(function(), 2);
    EXPECT_EQ(calls, 2);
}

TEST(ArtifactFunctionContractTest, MoveConstructionTransfersSmallCallable)
{
    ArtifactFunction<int()> source([value = 29] { return value; });
    ArtifactFunction<int()> destination(std::move(source));
    EXPECT_FALSE(source);
    ASSERT_TRUE(destination);
    EXPECT_EQ(destination(), 29);
}

TEST(ArtifactFunctionContractTest, MoveAssignmentClearsPreviousTarget)
{
    int destroyCount = 0;
    ArtifactFunction<int()> destination{CountedCallable(destroyCount)};
    ArtifactFunction<int()> source([value = 31] { return value; });
    destination = std::move(source);
    EXPECT_EQ(destroyCount, 1);
    EXPECT_FALSE(source);
    ASSERT_TRUE(destination);
    EXPECT_EQ(destination(), 31);
}

TEST(ArtifactFunctionContractTest, MoveAssignmentFromEmptyClearsTarget)
{
    ArtifactFunction<int()> destination([value = 37] { return value; });
    ArtifactFunction<int()> empty;
    destination = std::move(empty);
    EXPECT_FALSE(destination);
    EXPECT_FALSE(empty);
}

TEST(ArtifactFunctionContractTest, SelfMoveAssignmentPreservesCallable)
{
    ArtifactFunction<int()> function([value = 41] { return value; });
    function = std::move(function);
    ASSERT_TRUE(function);
    EXPECT_EQ(function(), 41);
}

TEST(ArtifactFunctionContractTest, HeapStoredCallableCanBeInvokedAndDestroyed)
{
    std::array<int, 64> values{};
    values[37] = 89;
    {
        ArtifactFunction<int()> function([values] { return values[37]; });
        ASSERT_TRUE(function);
        EXPECT_EQ(function(), 89);
    }
}

TEST(ArtifactFunctionContractTest, ClearDestroysCallableAndMakesFunctionEmpty)
{
    int destroyCount = 0;
    {
        ArtifactFunction<int()> function{CountedCallable(destroyCount)};
        EXPECT_TRUE(function);
        EXPECT_EQ(function(), 73);
        function = ArtifactFunction<int()>();
        EXPECT_FALSE(function);
    }
    EXPECT_EQ(destroyCount, 1);
}

TEST(ArtifactFunctionContractTest, SwapExchangesSmallCallables)
{
    ArtifactFunction<int()> first([value = 3] { return value; });
    ArtifactFunction<int()> second([value = 8] { return value; });
    first.swap(second);
    EXPECT_EQ(first(), 8);
    EXPECT_EQ(second(), 3);
}

TEST(ArtifactFunctionContractTest, SwapExchangesInlineAndHeapStoredCallables)
{
    ArtifactFunction<int()> inlineFunction([value = 12] { return value; });
    std::array<int, 64> values{};
    values[41] = 94;
    ArtifactFunction<int()> heapFunction([values] { return values[41]; });

    inlineFunction.swap(heapFunction);

    ASSERT_TRUE(inlineFunction);
    ASSERT_TRUE(heapFunction);
    EXPECT_EQ(inlineFunction(), 94);
    EXPECT_EQ(heapFunction(), 12);
}

TEST(ArtifactFunctionContractTest, SwapWithEmptyFunctionTransfersCallable)
{
    ArtifactFunction<int()> function([value = 52] { return value; });
    ArtifactFunction<int()> empty;

    function.swap(empty);

    EXPECT_FALSE(function);
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty(), 52);
}
