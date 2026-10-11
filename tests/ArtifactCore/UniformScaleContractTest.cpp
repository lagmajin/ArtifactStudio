#include <gtest/gtest.h>

#include <type_traits>

import Core.UniformScale;

using ArtifactCore::UniformScale;

static_assert(std::is_same_v<decltype(std::declval<const UniformScale&>().scale()), float>);

TEST(UniformScaleContractTest, DefaultScaleIsOne)
{
    const UniformScale scale;
    EXPECT_FLOAT_EQ(scale.scale(), 1.0f);
}

TEST(UniformScaleContractTest, RawPimplOwnershipIsNotShallowCopyable)
{
    EXPECT_FALSE(std::is_copy_constructible_v<UniformScale>);
    EXPECT_FALSE(std::is_copy_assignable_v<UniformScale>);
    EXPECT_FALSE(std::is_move_constructible_v<UniformScale>);
}
