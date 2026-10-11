#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

import Core.ArtifactFunctionRef;

using ArtifactCore::ArtifactFunctionRef;

namespace {
int addThree(int value) { return value + 3; }
}

static_assert(std::is_default_constructible_v<ArtifactFunctionRef<int(int)>>);
static_assert(!std::is_constructible_v<ArtifactFunctionRef<int(int)>, decltype([offset = 1](int value) { return value + offset; })>);

TEST(ArtifactFunctionRefContractTest, DefaultConstructionIsInvalid)
{
    const ArtifactFunctionRef<int(int)> ref;
    EXPECT_FALSE(ref.isValid());
    EXPECT_FALSE(static_cast<bool>(ref));
}

TEST(ArtifactFunctionRefContractTest, NullptrConstructionIsInvalid)
{
    const ArtifactFunctionRef<int(int)> ref(nullptr);
    EXPECT_FALSE(ref.isValid());
}

TEST(ArtifactFunctionRefContractTest, NullFunctionPointerConstructionIsInvalid)
{
    int (*function)(int) = nullptr;
    const ArtifactFunctionRef<int(int)> ref(function);

    EXPECT_FALSE(ref.isValid());
    EXPECT_FALSE(static_cast<bool>(ref));
}

TEST(ArtifactFunctionRefContractTest, FunctionPointerInvokesThroughCallOperator)
{
    const ArtifactFunctionRef<int(int)> ref(&addThree);
    ASSERT_TRUE(ref);
    EXPECT_EQ(ref(9), 12);
}

TEST(ArtifactFunctionRefContractTest, CallableReferenceIsBorrowed)
{
    int offset = 4;
    auto callable = [&offset](int value) { return value + offset; };
    const ArtifactFunctionRef<int(int)> ref(callable);
    EXPECT_EQ(ref.invoke(2), 6);
    offset = 10;
    EXPECT_EQ(ref(2), 12);
}

TEST(ArtifactFunctionRefContractTest, ConstCallableLvalueCanBeBorrowed)
{
    const auto callable = [factor = 3](int value) { return value * factor; };
    const ArtifactFunctionRef<int(int)> ref(callable);

    EXPECT_EQ(ref(7), 21);
}

TEST(ArtifactFunctionRefContractTest, MutableCallableStateIsPreserved)
{
    int count = 0;
    auto callable = [&count](int amount) { count += amount; return count; };
    ArtifactFunctionRef<int(int)> ref(callable);
    EXPECT_EQ(ref(2), 2);
    EXPECT_EQ(ref(5), 7);
    EXPECT_EQ(count, 7);
}

TEST(ArtifactFunctionRefContractTest, VoidReturnCallableExecutes)
{
    int value = 0;
    auto callable = [&value](int next) { value = next; };
    const ArtifactFunctionRef<void(int)> ref(callable);
    ref(17);
    EXPECT_EQ(value, 17);
}

TEST(ArtifactFunctionRefContractTest, ClearInvalidatesReference)
{
    auto callable = [](int value) { return value * 2; };
    ArtifactFunctionRef<int(int)> ref(callable);
    ASSERT_TRUE(ref.isValid());
    ref.clear();
    EXPECT_FALSE(ref.isValid());
    EXPECT_FALSE(static_cast<bool>(ref));
}

TEST(ArtifactFunctionRefContractTest, ClearedReferenceCanBeReboundAndClearedAgain)
{
    auto firstCallable = [](int value) { return value + 1; };
    auto secondCallable = [](int value) { return value * 4; };
    ArtifactFunctionRef<int(int)> ref(firstCallable);
    ref.clear();
    ref.clear();

    const ArtifactFunctionRef<int(int)> replacement(secondCallable);
    ref = replacement;

    ASSERT_TRUE(ref.isValid());
    EXPECT_EQ(ref(6), 24);
    ref.clear();
    EXPECT_FALSE(ref.isValid());
}

TEST(ArtifactFunctionRefContractTest, CopyKeepsCallableBinding)
{
    auto callable = [](int value) { return value - 1; };
    ArtifactFunctionRef<int(int)> first(callable);
    const auto second = first;
    EXPECT_EQ(second(8), 7);
}

TEST(ArtifactFunctionRefContractTest, CopyAssignmentRebindsToOtherBorrowedCallable)
{
    auto firstCallable = [](int value) { return value + 1; };
    auto secondCallable = [](int value) { return value * 3; };
    ArtifactFunctionRef<int(int)> first(firstCallable);
    ArtifactFunctionRef<int(int)> second(secondCallable);

    first = second;

    EXPECT_EQ(first(5), 15);
    EXPECT_EQ(second(5), 15);
}

TEST(ArtifactFunctionRefContractTest, NullptrAssignmentInvalidatesReference)
{
    auto callable = [](int value) { return value - 2; };
    ArtifactFunctionRef<int(int)> ref(callable);
    ASSERT_TRUE(ref.isValid());

    ref = nullptr;

    EXPECT_FALSE(ref.isValid());
    EXPECT_FALSE(static_cast<bool>(ref));
}

TEST(ArtifactFunctionRefContractTest, NonCopyableLvalueCallableCanBeBorrowed)
{
    struct NonCopyable {
        NonCopyable() = default;
        NonCopyable(const NonCopyable&) = delete;
        int operator()(int value) { return value + 1; }
    } callable;
    ArtifactFunctionRef<int(int)> ref(callable);
    EXPECT_EQ(ref(4), 5);
}

TEST(ArtifactFunctionRefContractTest, ReferenceReturnPreservesAliasing)
{
    int value = 12;
    auto callable = [&value]() -> int& { return value; };
    const ArtifactFunctionRef<int&()> ref(callable);

    int& result = ref();
    result = 29;
    EXPECT_EQ(value, 29);
}

TEST(ArtifactFunctionRefContractTest, MoveOnlyValueArgumentIsForwarded)
{
    int calls = 0;
    auto callable = [&calls](std::unique_ptr<int> value) {
        ++calls;
        return *value + 1;
    };
    const ArtifactFunctionRef<int(std::unique_ptr<int>)> ref(callable);

    EXPECT_EQ(ref(std::make_unique<int>(41)), 42);
    EXPECT_EQ(calls, 1);
}

TEST(ArtifactFunctionRefContractTest, ReferenceArgumentMutatesCallerValue)
{
    auto callable = [](int& value) { value += 5; };
    const ArtifactFunctionRef<void(int&)> ref(callable);
    int value = 8;

    ref(value);
    EXPECT_EQ(value, 13);
}
