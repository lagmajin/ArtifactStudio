#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

import Utils.Handle;

using namespace ArtifactCore;

namespace {

static_assert(std::is_same_v<AssetHandle::value_type, std::uint64_t>);
static_assert(!std::is_convertible_v<AssetHandle, ProjectHandle>);
static_assert(!std::is_convertible_v<ProjectHandle, LayerHandle>);
static_assert(!std::is_convertible_v<AssetHandle::value_type, AssetHandle>);
static_assert(std::is_trivially_copyable_v<AssetHandle>);
static_assert(std::is_trivially_copyable_v<ProjectHandle>);
static_assert(std::is_trivially_copyable_v<LayerHandle>);
static_assert(AssetHandle(std::numeric_limits<std::uint64_t>::max()).isValid());
static_assert(AssetHandle(std::numeric_limits<std::uint64_t>::max()).raw() ==
              std::numeric_limits<std::uint64_t>::max());

} // namespace

TEST(HandleContractTest, DefaultHandleIsNilAndInvalid)
{
    constexpr AssetHandle handle;
    EXPECT_TRUE(handle.isNil());
    EXPECT_FALSE(handle.isValid());
    EXPECT_FALSE(static_cast<bool>(handle));
    EXPECT_EQ(handle, nullptr);
    EXPECT_NE(handle, nullptr);
}

TEST(HandleContractTest, DefaultConstructionIsConsistentAcrossHandleTags)
{
    constexpr AssetHandle asset;
    constexpr ProjectHandle project;
    constexpr LayerHandle layer;

    EXPECT_EQ(asset.raw(), 0u);
    EXPECT_EQ(project.raw(), 0u);
    EXPECT_EQ(layer.raw(), 0u);
    EXPECT_FALSE(asset.isValid());
    EXPECT_FALSE(project.isValid());
    EXPECT_FALSE(layer.isValid());
}

TEST(HandleContractTest, ExplicitValueAndFactoriesPreserveRawValue)
{
    constexpr AssetHandle direct(73);
    constexpr AssetHandle lowerFactory = AssetHandle::fromValue(73);
    constexpr AssetHandle upperFactory = AssetHandle::FromNil();
    EXPECT_EQ(direct.value(), 73u);
    EXPECT_EQ(direct.raw(), 73u);
    EXPECT_EQ(lowerFactory, direct);
    EXPECT_EQ(upperFactory, AssetHandle::Nil());
}

TEST(HandleContractTest, NonzeroHandleIsValidAndConvertsExplicitlyToBool)
{
    constexpr ProjectHandle handle(1);
    EXPECT_TRUE(handle.isValid());
    EXPECT_FALSE(handle.isNil());
    EXPECT_TRUE(static_cast<bool>(handle));
    EXPECT_NE(handle, nullptr);
}

TEST(HandleContractTest, EqualityAndOrderingCompareNumericIdentity)
{
    constexpr LayerHandle lower(4);
    constexpr LayerHandle same(4);
    constexpr LayerHandle higher(9);
    EXPECT_EQ(lower, same);
    EXPECT_NE(lower, higher);
    EXPECT_LT(lower, higher);
    EXPECT_GT(higher, same);
}

TEST(HandleContractTest, DistinctTagsKeepEqualNumericValuesTypeDistinct)
{
    constexpr AssetHandle asset(12);
    constexpr ProjectHandle project(12);
    EXPECT_EQ(asset.value(), project.value());
    EXPECT_FALSE((std::is_same_v<AssetHandle, ProjectHandle>));
    EXPECT_FALSE((std::is_convertible_v<AssetHandle, ProjectHandle>));
}

TEST(HandleContractTest, FreeValidityHelperMatchesMember)
{
    EXPECT_TRUE(isValid(AssetHandle(8)));
    EXPECT_FALSE(isValid(AssetHandle::Nil()));
}

TEST(HandleContractTest, HashUsesUnderlyingValueAndSeed)
{
    constexpr AssetHandle handle(1234);
    EXPECT_EQ(qHash(handle, 17), qHash(handle.value(), 17));
    EXPECT_EQ(qHash(handle), qHash(handle.value()));
}

TEST(HandleContractTest, NilAliasesRemainEquivalent)
{
    EXPECT_EQ(AssetHandle::Nil(), AssetHandle::FromNil());
    EXPECT_EQ(ProjectHandle::Nil(), ProjectHandle{});
    EXPECT_EQ(LayerHandle::Nil().raw(), 0u);
}

TEST(HandleContractTest, MaximumRawValueRemainsValidAndOrdered)
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    const AssetHandle handle(maximum);
    const AssetHandle previous(maximum - 1);

    EXPECT_TRUE(handle.isValid());
    EXPECT_EQ(handle.value(), maximum);
    EXPECT_GT(handle, previous);
}

TEST(HandleContractTest, MaximumRawValueIsPreservedForEveryTag)
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    const ProjectHandle project(maximum);
    const LayerHandle layer(maximum);

    EXPECT_TRUE(project.isValid());
    EXPECT_TRUE(layer.isValid());
    EXPECT_EQ(project.raw(), maximum);
    EXPECT_EQ(layer.raw(), maximum);
}

TEST(HandleContractTest, CopyAndMovePreserveValueHandleIdentity)
{
    AssetHandle source(0x123456789abcdef0ULL);
    const AssetHandle copied(source);
    const AssetHandle moved(std::move(source));

    EXPECT_EQ(copied.raw(), 0x123456789abcdef0ULL);
    EXPECT_EQ(moved.raw(), 0x123456789abcdef0ULL);
    EXPECT_EQ(source.raw(), 0x123456789abcdef0ULL);
}

TEST(HandleContractTest, NullComparisonsWorkInBothOperandOrders)
{
    const AssetHandle nil;
    const AssetHandle valid(2);

    EXPECT_TRUE(nil == nullptr);
    EXPECT_TRUE(nullptr == nil);
    EXPECT_FALSE(nil != nullptr);
    EXPECT_FALSE(nullptr != nil);
    EXPECT_TRUE(valid != nullptr);
    EXPECT_TRUE(nullptr != valid);
    EXPECT_FALSE(valid == nullptr);
    EXPECT_FALSE(nullptr == valid);
}

TEST(HandleContractTest, NullComparisonsWorkForEveryHandleTag)
{
    const ProjectHandle nilProject;
    const ProjectHandle validProject(1);
    const LayerHandle nilLayer;
    const LayerHandle validLayer(1);

    EXPECT_TRUE(nilProject == nullptr);
    EXPECT_TRUE(nullptr == nilProject);
    EXPECT_FALSE(nilProject != nullptr);
    EXPECT_FALSE(nullptr != nilProject);
    EXPECT_TRUE(nilLayer == nullptr);
    EXPECT_TRUE(nullptr == nilLayer);
    EXPECT_FALSE(nilLayer != nullptr);
    EXPECT_FALSE(nullptr != nilLayer);
    EXPECT_FALSE(validProject == nullptr);
    EXPECT_FALSE(nullptr == validProject);
    EXPECT_FALSE(validLayer == nullptr);
    EXPECT_FALSE(nullptr == validLayer);
    EXPECT_TRUE(validProject != nullptr);
    EXPECT_TRUE(nullptr != validProject);
    EXPECT_TRUE(validLayer != nullptr);
    EXPECT_TRUE(nullptr != validLayer);
}

