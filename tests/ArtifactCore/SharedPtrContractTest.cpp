#include <gtest/gtest.h>

#include <memory>

import Memory.SharedPtr;

using namespace ArtifactCore;

namespace {
struct PolymorphicBase {
    virtual ~PolymorphicBase() = default;
    int value = 0;
};

struct Derived final : PolymorphicBase {
    explicit Derived(int input) { value = input; }
    int extra = 17;
};

struct Other final : PolymorphicBase {};
}

TEST(SharedPtrContractTest, DefaultAndNullptrAreEmpty)
{
    SharedPtr<int> empty;
    SharedPtr<int> nullValue(nullptr);
    EXPECT_FALSE(empty);
    EXPECT_FALSE(nullValue);
    EXPECT_EQ(empty, nullptr);
    EXPECT_EQ(nullValue, nullptr);
    EXPECT_EQ(empty.useCount(), 0);
}

TEST(SharedPtrContractTest, FactoryConstructsValueAndSupportsMutation)
{
    auto value = makeShared<int>(41);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 41);
    *value = 42;
    EXPECT_EQ(*value, 42);
}

TEST(SharedPtrContractTest, StandardSharedPtrInteropPreservesSharedOwnership)
{
    std::shared_ptr<int> standard = std::make_shared<int>(67);
    SharedPtr<int> wrapped = standard;
    EXPECT_EQ(wrapped.useCount(), 2);

    std::shared_ptr<int> exported = wrapped;
    EXPECT_EQ(exported.get(), standard.get());
    EXPECT_EQ(wrapped.useCount(), 3);

    wrapped.reset();
    standard.reset();
    ASSERT_NE(exported, nullptr);
    EXPECT_EQ(*exported, 67);
}

TEST(SharedPtrContractTest, MovingStandardSharedPtrTransfersItsOwnership)
{
    std::shared_ptr<int> standard = std::make_shared<int>(71);

    auto wrapped = makeShared(std::move(standard));

    EXPECT_FALSE(standard);
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(*wrapped, 71);
    EXPECT_EQ(wrapped.useCount(), 1);
}

TEST(SharedPtrContractTest, FactoryFromStandardSharedPtrSharesExistingControlBlock)
{
    std::shared_ptr<int> standard = std::make_shared<int>(73);

    auto wrapped = makeShared(standard);

    EXPECT_EQ(wrapped.get(), standard.get());
    EXPECT_EQ(wrapped.useCount(), 2);
    standard.reset();
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(*wrapped, 73);
}

TEST(SharedPtrContractTest, CopiesShareOwnershipAndCompareByPointer)
{
    auto original = makeShared<int>(5);
    auto copy = original;
    EXPECT_EQ(original, copy);
    EXPECT_EQ(original.useCount(), 2);
    EXPECT_EQ(copy.useCount(), 2);
}

TEST(SharedPtrContractTest, SelfCopyAssignmentPreservesSoleOwnership)
{
    auto owner = makeShared<int>(13);
    int* const originalAddress = owner.get();

    owner = owner;

    EXPECT_EQ(owner.get(), originalAddress);
    EXPECT_EQ(owner.useCount(), 1);
    ASSERT_TRUE(owner);
    EXPECT_EQ(*owner, 13);
}

TEST(SharedPtrContractTest, MoveTransfersOwnership)
{
    auto original = makeShared<int>(9);
    auto moved = std::move(original);
    EXPECT_FALSE(original);
    ASSERT_TRUE(moved);
    EXPECT_EQ(*moved, 9);
    EXPECT_EQ(moved.useCount(), 1);
}

TEST(SharedPtrContractTest, ResetAndNullAssignmentReleaseOwnership)
{
    auto owner = makeShared<int>(3);
    auto observer = owner;
    owner.reset();
    EXPECT_FALSE(owner);
    EXPECT_EQ(observer.useCount(), 1);
    observer = nullptr;
    EXPECT_FALSE(observer);
}

TEST(SharedPtrContractTest, ConvertingCopyPreservesDerivedObject)
{
    auto derived = makeShared<Derived>(23);
    SharedPtr<PolymorphicBase> base = derived;
    EXPECT_EQ(base.get(), derived.get());
    EXPECT_EQ(base->value, 23);
    EXPECT_EQ(base.useCount(), 2);
}

TEST(SharedPtrContractTest, ConvertingCopyAssignmentPreservesDerivedOwnership)
{
    auto derived = makeShared<Derived>(29);
    SharedPtr<PolymorphicBase> base;
    base = derived;

    EXPECT_EQ(base.get(), derived.get());
    EXPECT_EQ(base->value, 29);
    EXPECT_EQ(base.useCount(), 2);
}

TEST(SharedPtrContractTest, ConstConvertingCopySharesObjectAndControlBlock)
{
    auto mutableValue = makeShared<int>(83);
    SharedPtr<const int> readOnly = mutableValue;

    EXPECT_EQ(*readOnly, 83);
    EXPECT_EQ(readOnly.get(), mutableValue.get());
    EXPECT_EQ(readOnly.useCount(), 2);
    *mutableValue = 84;
    EXPECT_EQ(*readOnly, 84);
}

TEST(SharedPtrContractTest, StaticPointerCastPreservesControlBlock)
{
    SharedPtr<PolymorphicBase> base = makeShared<Derived>(31);
    auto derived = staticPointerCast<Derived>(base);
    ASSERT_TRUE(derived);
    EXPECT_EQ(derived->value, 31);
    EXPECT_EQ(derived.useCount(), 2);
}

TEST(SharedPtrContractTest, DynamicPointerCastSucceedsForMatchingType)
{
    SharedPtr<PolymorphicBase> base = makeShared<Derived>(37);
    auto derived = dynamicPointerCast<Derived>(base);
    ASSERT_TRUE(derived);
    EXPECT_EQ(derived->extra, 17);
    EXPECT_EQ(derived.useCount(), 2);
}

TEST(SharedPtrContractTest, DynamicPointerCastReturnsEmptyForDifferentType)
{
    SharedPtr<PolymorphicBase> base = makeShared<Other>();
    auto derived = dynamicPointerCast<Derived>(base);
    EXPECT_FALSE(derived);
    EXPECT_EQ(base.useCount(), 1);
}

TEST(SharedPtrContractTest, WeakPointerLocksWhileOwnerExistsAndExpiresAfterward)
{
    WeakPtr<int> weak;
    {
        auto owner = makeShared<int>(53);
        weak = owner;
        EXPECT_FALSE(weak.expired());
        auto locked = weak.lock();
        ASSERT_TRUE(locked);
        EXPECT_EQ(*locked, 53);
        EXPECT_EQ(weak.useCount(), 2);
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
    EXPECT_EQ(weak.useCount(), 0);
}

TEST(SharedPtrContractTest, WeakPointerWrapsStandardWeakOwnership)
{
    std::weak_ptr<int> standardWeak;
    {
        auto owner = std::make_shared<int>(57);
        standardWeak = owner;
        WeakPtr<int> wrapped = standardWeak;
        EXPECT_FALSE(wrapped.expired());
        EXPECT_EQ(wrapped.useCount(), 1);
        ASSERT_TRUE(wrapped.lock());
        EXPECT_EQ(*wrapped.lock(), 57);
    }

    WeakPtr<int> wrapped = standardWeak;
    EXPECT_TRUE(wrapped.expired());
    EXPECT_FALSE(wrapped.lock());
}

TEST(SharedPtrContractTest, WeakPointerAssignmentFromStandardOwnerCanResetToNull)
{
    auto standard = std::make_shared<int>(79);
    WeakPtr<int> wrapped;
    wrapped = standard;

    EXPECT_FALSE(wrapped.expired());
    EXPECT_EQ(wrapped.useCount(), 1);
    ASSERT_TRUE(wrapped.lock());
    EXPECT_EQ(*wrapped.lock(), 79);

    wrapped = nullptr;
    EXPECT_TRUE(wrapped.expired());
    EXPECT_EQ(wrapped.useCount(), 0);
    EXPECT_EQ(*standard, 79);
}

TEST(SharedPtrContractTest, ResettingWeakPointerDoesNotReleaseSharedOwner)
{
    auto owner = makeShared<int>(59);
    WeakPtr<int> weak = owner;
    ASSERT_FALSE(weak.expired());

    weak.reset();

    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(weak.useCount(), 0);
    EXPECT_EQ(owner.useCount(), 1);
    EXPECT_EQ(*owner, 59);
}

TEST(SharedPtrContractTest, CustomDeleterRunsExactlyOnce)
{
    int deleteCount = 0;
    {
        auto owner = makeShared(new int(61), [&deleteCount](int* value) {
            ++deleteCount;
            delete value;
        });
        auto copy = owner;
        EXPECT_EQ(deleteCount, 0);
        EXPECT_EQ(*copy, 61);
    }
    EXPECT_EQ(deleteCount, 1);
}

TEST(SharedPtrContractTest, SwapExchangesOwnedObjects)
{
    auto first = makeShared<int>(1);
    auto second = makeShared<int>(2);
    first.swap(second);
    EXPECT_EQ(*first, 2);
    EXPECT_EQ(*second, 1);
}
