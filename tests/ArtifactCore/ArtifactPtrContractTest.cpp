#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

import Core.ArtifactPtr;

using namespace ArtifactCore;

namespace {
struct Value {
    explicit Value(int initial, int* destructionCount = nullptr)
        : value(initial), destructionCount(destructionCount) {}
    ~Value() { if (destructionCount) ++*destructionCount; }

    int value;
    int* destructionCount;
};
}

static_assert(!std::is_copy_constructible_v<UniquePtr<int>>);
static_assert(!std::is_copy_assignable_v<UniquePtr<int>>);
static_assert(std::is_nothrow_move_constructible_v<UniquePtr<int>>);
static_assert(std::is_nothrow_move_assignable_v<UniquePtr<int>>);
static_assert(std::is_same_v<
    decltype(std::declval<const Ptr<int>&>().get()), Optional<const int&>>);
static_assert(std::is_same_v<
    decltype(std::declval<const Owned<int>&>().get()), Optional<const int&>>);

TEST(ArtifactPtrContractTest, PtrDefaultAndNullConstructionAreEmpty)
{
    Ptr<int> empty;
    Ptr<int> nullValue(nullptr);
    EXPECT_FALSE(empty.isValid());
    EXPECT_FALSE(nullValue.isValid());
    EXPECT_EQ(empty.useCount(), 0);
    EXPECT_FALSE(empty.get());
}

TEST(ArtifactPtrContractTest, PtrGetExposesMutableReference)
{
    Ptr<int> pointer(new int(20));
    auto value = pointer.get();
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 20);
    *value = 21;
    EXPECT_EQ(*pointer.get(), 21);
}

TEST(ArtifactPtrContractTest, PtrCopiesShareAndReleaseOwnership)
{
    Ptr<int> first(new int(3));
    Ptr<int> second(first);
    EXPECT_EQ(first.useCount(), 2);
    second.reset();
    EXPECT_EQ(first.useCount(), 1);
    EXPECT_TRUE(first.isValid());
}

TEST(ArtifactPtrContractTest, PtrCopyAssignmentReleasesOldAndSharesNew)
{
    int destructionCount = 0;
    Ptr<Value> source(new Value(12, &destructionCount));
    Ptr<Value> destination(new Value(13, &destructionCount));
    destination = source;
    EXPECT_EQ(destructionCount, 1);
    EXPECT_EQ(source.useCount(), 2);
    EXPECT_EQ(destination.get()->value, 12);
}

TEST(ArtifactPtrContractTest, PtrMoveAssignmentTransfersAndEmptiesSource)
{
    Ptr<int> source(new int(19));
    Ptr<int> destination(new int(20));
    int* transferred = source.get().value();
    destination = std::move(source);
    EXPECT_FALSE(source.isValid());
    EXPECT_TRUE(destination.isValid());
    EXPECT_EQ(destination.get().value(), transferred);
    EXPECT_EQ(*destination.get(), 19);
}

TEST(ArtifactPtrContractTest, PtrSelfCopyAndMoveAssignmentPreserveOwnership)
{
    Ptr<int> pointer(new int(22));
    pointer = pointer;
    EXPECT_EQ(pointer.useCount(), 1);
    pointer = std::move(pointer);
    ASSERT_TRUE(pointer.isValid());
    EXPECT_EQ(pointer.useCount(), 1);
    EXPECT_EQ(*pointer.get(), 22);
}

TEST(ArtifactPtrContractTest, PtrIfValidRunsOnlyForNonemptyPointer)
{
    Ptr<int> empty;
    int calls = 0;
    empty.ifValid([&](int&) { ++calls; });
    Ptr<int> pointer(new int(6));
    pointer.ifValid([&](int& value) { value += 2; ++calls; });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(*pointer.get(), 8);
}

TEST(ArtifactPtrContractTest, ConstPtrAccessAndCallbackRemainReadOnly)
{
    const Ptr<int> pointer(new int(24));
    auto value = pointer.get();
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 24);
    int observed = 0;
    pointer.ifValid([&](const int& current) { observed = current; });
    EXPECT_EQ(observed, 24);
}

TEST(ArtifactPtrContractTest, WeakObserverSurvivesOwnerMoveAndExpiresOnFinalReset)
{
    Ptr<Value> source(new Value(27));
    WeakPtr<Value> weak(source);
    Ptr<Value> destination(std::move(source));
    EXPECT_FALSE(weak.isExpired());
    EXPECT_FALSE(source.isValid());
    destination.reset();
    EXPECT_TRUE(weak.isExpired());
}

TEST(ArtifactPtrContractTest, WeakPtrLockExtendsLifetimeThenExpires)
{
    int destructionCount = 0;
    Ptr<Value> owner(new Value(14, &destructionCount));
    WeakPtr<Value> weak(owner);
    {
        EXPECT_FALSE(weak.isExpired());
        auto locked = weak.lock();
        ASSERT_TRUE(locked.isValid());
        EXPECT_EQ(locked.get()->value, 14);
        owner.reset();
        EXPECT_EQ(destructionCount, 0);
        locked.reset();
    }
    EXPECT_EQ(destructionCount, 1);
    EXPECT_TRUE(weak.isExpired());
    EXPECT_FALSE(weak.lock().isValid());
}

TEST(ArtifactPtrContractTest, WeakPtrCopyKeepsControlBlockUntilLastWeakOwner)
{
    Ptr<int> owner(new int(17));
    WeakPtr<int> first(owner);
    {
        WeakPtr<int> second(first);
        EXPECT_FALSE(first.isExpired());
        owner.reset();
        EXPECT_TRUE(first.isExpired());
        EXPECT_TRUE(second.isExpired());
    }
    EXPECT_TRUE(first.isExpired());
}

TEST(ArtifactPtrContractTest, DefaultWeakPtrIsExpiredAndCannotLock)
{
    WeakPtr<int> weak;
    EXPECT_TRUE(weak.isExpired());
    EXPECT_FALSE(weak.lock().isValid());
}

TEST(ArtifactPtrContractTest, RefConstructedFromPtrKeepsObjectAlive)
{
    int destructionCount = 0;
    {
        Ptr<Value> owner(new Value(25, &destructionCount));
        Ref<Value> reference(owner);
        owner.reset();
        EXPECT_EQ(reference->value, 25);
        reference->value = 26;
        EXPECT_EQ((*reference).value, 26);
    }
    EXPECT_EQ(destructionCount, 1);
}

TEST(ArtifactPtrContractTest, RefCopyAndMoveTransferSharedLifetime)
{
    int destructionCount = 0;
    {
        Ref<Value> original(new Value(30, &destructionCount));
        {
            Ref<Value> copy(original);
            EXPECT_EQ(copy->value, 30);
            Ref<Value> moved(std::move(copy));
            EXPECT_EQ(moved->value, 30);
        }
        EXPECT_EQ(destructionCount, 0);
    }
    EXPECT_EQ(destructionCount, 1);
}

TEST(ArtifactPtrContractTest, RefCopyAndMoveAssignmentKeepControlBlockBalanced)
{
    int destructionCount = 0;
    {
        Ref<Value> first(new Value(32, &destructionCount));
        Ref<Value> second(new Value(33, &destructionCount));
        second = first;
        EXPECT_EQ(destructionCount, 1);
        EXPECT_EQ(second->value, 32);
        Ref<Value> third(new Value(34, &destructionCount));
        third = std::move(second);
        EXPECT_EQ(destructionCount, 2);
        EXPECT_EQ(third->value, 32);
    }
    EXPECT_EQ(destructionCount, 3);
}

TEST(ArtifactPtrContractTest, RefSelfCopyAndMoveAssignmentPreserveValue)
{
    Ref<int> reference(new int(36));
    reference = reference;
    reference = std::move(reference);
    EXPECT_EQ(*reference, 36);
}

TEST(ArtifactPtrContractTest, OwnedGetResetAndTakeManageExclusiveOwnership)
{
    int destructionCount = 0;
    Owned<Value> owner(new Value(40, &destructionCount));
    auto value = owner.get();
    ASSERT_TRUE(value);
    EXPECT_EQ((*value).value, 40);
    owner.reset(new Value(41, &destructionCount));
    EXPECT_EQ(destructionCount, 1);
    Value* raw = owner.take();
    EXPECT_FALSE(owner.isValid());
    EXPECT_EQ(raw->value, 41);
    delete raw;
    EXPECT_EQ(destructionCount, 2);
}

TEST(ArtifactPtrContractTest, OwnedMoveAssignmentDestroysReplacedObject)
{
    int destructionCount = 0;
    Owned<Value> destination(new Value(50, &destructionCount));
    Owned<Value> source(new Value(51, &destructionCount));
    destination = std::move(source);
    EXPECT_EQ(destructionCount, 1);
    EXPECT_FALSE(source.isValid());
    EXPECT_TRUE(destination.isValid());
    auto value = destination.get();
    ASSERT_TRUE(value);
    EXPECT_EQ((*value).value, 51);
}

TEST(ArtifactPtrContractTest, OwnedDefaultAndIfValidRespectEmptyState)
{
    Owned<int> empty;
    int calls = 0;
    empty.ifValid([&](int&) { ++calls; });
    EXPECT_FALSE(empty.isValid());
    EXPECT_FALSE(empty.get());
    Owned<int> owner(new int(28));
    owner.ifValid([&](int& value) { value += 1; ++calls; });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(*owner.get(), 29);
}

TEST(ArtifactPtrContractTest, OwnedResetToNullReleasesCurrentValue)
{
    int destructionCount = 0;
    Owned<Value> owner(new Value(42, &destructionCount));
    owner.reset();
    EXPECT_FALSE(owner.isValid());
    EXPECT_FALSE(owner.get());
    EXPECT_EQ(destructionCount, 1);
}

TEST(ArtifactPtrContractTest, OwnedMoveConstructionTransfersExclusiveOwnership)
{
    Owned<int> source(new int(35));
    int* address = source.get().value();
    Owned<int> destination(std::move(source));
    EXPECT_FALSE(source.isValid());
    EXPECT_TRUE(destination.isValid());
    EXPECT_EQ(destination.get().value(), address);
    EXPECT_EQ(*destination.get(), 35);
}

TEST(ArtifactPtrContractTest, OwnedSelfMoveAssignmentPreservesExclusiveOwnership)
{
    int destructionCount = 0;
    {
        Owned<Value> owner(new Value(39, &destructionCount));
        auto original = owner.get();
        Value* const address = &original.value();

        owner = std::move(owner);

        ASSERT_TRUE(owner.isValid());
        auto current = owner.get();
        ASSERT_TRUE(current);
        EXPECT_EQ(&current.value(), address);
        EXPECT_EQ(current.value().value, 39);
        EXPECT_EQ(destructionCount, 0);
    }
    EXPECT_EQ(destructionCount, 1);
}

TEST(ArtifactPtrContractTest, DefaultConstructionIsEmpty)
{
    UniquePtr<int> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.get(), nullptr);
}

TEST(ArtifactPtrContractTest, FactoryForwardsConstructorArguments)
{
    auto value = makeUnique<Value>(37);
    ASSERT_TRUE(value);
    EXPECT_EQ(value->value, 37);
    value->value = 38;
    EXPECT_EQ((*value).value, 38);
}

TEST(ArtifactPtrContractTest, RawPointerConstructionTakesOwnership)
{
    auto* raw = new int(12);
    UniquePtr<int> owner(raw);
    EXPECT_EQ(owner.get(), raw);
    EXPECT_EQ(*owner, 12);
}

TEST(ArtifactPtrContractTest, MoveConstructionTransfersOwnership)
{
    auto source = makeUnique<int>(8);
    int* address = source.get();
    auto destination = std::move(source);
    EXPECT_EQ(source.get(), nullptr);
    EXPECT_EQ(destination.get(), address);
    EXPECT_EQ(*destination, 8);
}

TEST(ArtifactPtrContractTest, MoveAssignmentDeletesPreviousValueAndTakesSource)
{
    int destructionCount = 0;
    UniquePtr<Value> destination(new Value(1, &destructionCount));
    auto source = makeUnique<Value>(2);
    Value* transferred = source.get();
    destination = std::move(source);
    EXPECT_EQ(destructionCount, 1);
    EXPECT_EQ(source.get(), nullptr);
    EXPECT_EQ(destination.get(), transferred);
    EXPECT_EQ(destination->value, 2);
}

TEST(ArtifactPtrContractTest, TakeReleasesPointerWithoutDeletingIt)
{
    int destructionCount = 0;
    UniquePtr<Value> owner(new Value(4, &destructionCount));
    Value* raw = owner.take();
    EXPECT_FALSE(owner);
    EXPECT_EQ(destructionCount, 0);
    EXPECT_EQ(raw->value, 4);
    delete raw;
    EXPECT_EQ(destructionCount, 1);
}

TEST(ArtifactPtrContractTest, ResetDeletesOldValueAndStoresReplacement)
{
    int destructionCount = 0;
    UniquePtr<Value> owner(new Value(5, &destructionCount));
    auto* replacement = new Value(6, &destructionCount);
    owner.reset(replacement);
    EXPECT_EQ(destructionCount, 1);
    EXPECT_EQ(owner.get(), replacement);
    EXPECT_EQ(owner->value, 6);
    owner.reset();
    EXPECT_EQ(destructionCount, 2);
    EXPECT_FALSE(owner);
}

TEST(ArtifactPtrContractTest, ResetToNullReleasesUniqueValue)
{
    int destructionCount = 0;
    UniquePtr<Value> owner(new Value(7, &destructionCount));
    owner.reset();
    EXPECT_FALSE(owner);
    EXPECT_EQ(owner.get(), nullptr);
    EXPECT_EQ(destructionCount, 1);
}

TEST(ArtifactPtrContractTest, SwapExchangesOwnedValues)
{
    auto first = makeUnique<int>(9);
    auto second = makeUnique<int>(10);
    first.swap(second);
    EXPECT_EQ(*first, 10);
    EXPECT_EQ(*second, 9);
}

TEST(ArtifactPtrContractTest, SelfMoveAssignmentPreservesOwnership)
{
    auto owner = makeUnique<int>(15);
    int* address = owner.get();
    owner = std::move(owner);
    EXPECT_EQ(owner.get(), address);
    EXPECT_EQ(*owner, 15);
}
