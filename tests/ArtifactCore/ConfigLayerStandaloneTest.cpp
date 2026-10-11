#include <gtest/gtest.h>

import Configuration.ConfigLayer;

using namespace ArtifactCore;

TEST(ConfigLayerStandaloneTest, DeclaredLayersKeepStableUnderlyingValues)
{
    EXPECT_EQ(static_cast<int>(ConfigLayer::None), -1);
    EXPECT_EQ(static_cast<int>(ConfigLayer::System), 0);
    EXPECT_EQ(static_cast<int>(ConfigLayer::User), 1);
    EXPECT_EQ(static_cast<int>(ConfigLayer::Project), 2);
    EXPECT_EQ(static_cast<int>(ConfigLayer::Session), 3);
}

TEST(ConfigLayerStandaloneTest, NoneIsNotPersistent)
{
    EXPECT_FALSE(isPersistentConfigLayer(ConfigLayer::None));
}

TEST(ConfigLayerStandaloneTest, SystemLayerIsPersistent)
{
    EXPECT_TRUE(isPersistentConfigLayer(ConfigLayer::System));
}

TEST(ConfigLayerStandaloneTest, UserLayerIsPersistent)
{
    EXPECT_TRUE(isPersistentConfigLayer(ConfigLayer::User));
}

TEST(ConfigLayerStandaloneTest, ProjectLayerIsPersistent)
{
    EXPECT_TRUE(isPersistentConfigLayer(ConfigLayer::Project));
}

TEST(ConfigLayerStandaloneTest, SessionLayerIsNotPersistent)
{
    EXPECT_FALSE(isPersistentConfigLayer(ConfigLayer::Session));
}

TEST(ConfigLayerStandaloneTest, PriorityMatchesDeclaredLayerOrder)
{
    EXPECT_EQ(configLayerPriority(ConfigLayer::None), -1);
    EXPECT_LT(configLayerPriority(ConfigLayer::System), configLayerPriority(ConfigLayer::User));
    EXPECT_LT(configLayerPriority(ConfigLayer::User), configLayerPriority(ConfigLayer::Project));
    EXPECT_LT(configLayerPriority(ConfigLayer::Project), configLayerPriority(ConfigLayer::Session));
}

TEST(ConfigLayerStandaloneTest, HelpersAreUsableInConstantExpressions)
{
    static_assert(configLayerPriority(ConfigLayer::Project) == 2);
    static_assert(isPersistentConfigLayer(ConfigLayer::Project));
    static_assert(!isPersistentConfigLayer(ConfigLayer::Session));
    SUCCEED();
}
