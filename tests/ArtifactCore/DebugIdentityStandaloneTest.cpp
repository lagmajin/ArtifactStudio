#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <source_location>
#include <type_traits>
#include <vector>

#include <QString>

import Core.Diagnostics.DebugIdentity;

using Artifact::Diagnostics::DebugIdentity;
using Artifact::Diagnostics::DebugIdentitySnapshot;

TEST(DebugIdentityStandaloneTest, ConstructionAssignsUniqueNonzeroIds)
{
    DebugIdentity first(QStringLiteral("Widget"));
    DebugIdentity second(QStringLiteral("Widget"));
    EXPECT_NE(first.debugId(), 0u);
    EXPECT_GT(second.debugId(), first.debugId());
}

TEST(DebugIdentityStandaloneTest, ConstructionStoresTypeAndSourceLocation)
{
    const auto location = std::source_location::current();
    DebugIdentity identity(QStringLiteral("Renderer"), location);

    EXPECT_EQ(identity.debugTypeName(), QStringLiteral("Renderer"));
    EXPECT_EQ(identity.creationFile(), QString::fromUtf8(location.file_name()));
    EXPECT_EQ(identity.creationFunction(), QString::fromUtf8(location.function_name()));
    EXPECT_EQ(identity.creationLine(), location.line());
}

TEST(DebugIdentityStandaloneTest, DefaultConstructionUsesEmptyTypeAndObjectLabel)
{
    DebugIdentity identity;
    EXPECT_TRUE(identity.debugTypeName().isEmpty());
    EXPECT_EQ(identity.debugLabel(), QStringLiteral("Object#%1").arg(identity.debugId()));
}

TEST(DebugIdentityStandaloneTest, NamedIdentityAddsQuotedNameToLabel)
{
    DebugIdentity identity(QStringLiteral("Layer"));
    identity.setDebugName(QStringLiteral("Foreground"));

    EXPECT_EQ(identity.debugName(), QStringLiteral("Foreground"));
    EXPECT_EQ(identity.debugLabel(),
              QStringLiteral("Layer#%1(\"Foreground\")").arg(identity.debugId()));
}

TEST(DebugIdentityStandaloneTest, EmptyTypeFallsBackToObjectLabelAfterNameIsSet)
{
    DebugIdentity identity;
    identity.setDebugName(QStringLiteral("unnamed"));
    EXPECT_EQ(identity.debugLabel(),
              QStringLiteral("Object#%1(\"unnamed\")").arg(identity.debugId()));
}

TEST(DebugIdentityStandaloneTest, OwnerFieldsCanBeSetDirectly)
{
    DebugIdentity identity(QStringLiteral("Child"));
    identity.setDebugOwner(42, QStringLiteral("Parent"));

    EXPECT_EQ(identity.debugOwnerId(), 42u);
    EXPECT_EQ(identity.debugOwnerName(), QStringLiteral("Parent"));
}

TEST(DebugIdentityStandaloneTest, OwnerIdentityUsesItsDebugLabel)
{
    DebugIdentity owner(QStringLiteral("Composition"));
    owner.setDebugName(QStringLiteral("Main"));
    DebugIdentity child(QStringLiteral("Layer"));
    child.setDebugOwner(owner);

    EXPECT_EQ(child.debugOwnerId(), owner.debugId());
    EXPECT_EQ(child.debugOwnerName(), owner.debugLabel());
}

TEST(DebugIdentityStandaloneTest, SnapshotAllIncludesLiveIdentityMetadata)
{
    const auto before = DebugIdentity::snapshotAll();
    std::uint64_t id = 0;
    {
        DebugIdentity identity(QStringLiteral("SnapshotTarget"));
        identity.setDebugName(QStringLiteral("tracked"));
        identity.setDebugOwner(7, QStringLiteral("owner"));
        id = identity.debugId();

        const auto snapshots = DebugIdentity::snapshotAll();
        const auto found = std::find_if(snapshots.begin(), snapshots.end(),
            [id](const DebugIdentitySnapshot& value) { return value.id == id; });
        ASSERT_NE(found, snapshots.end());
        EXPECT_EQ(found->typeName, QStringLiteral("SnapshotTarget"));
        EXPECT_EQ(found->name, QStringLiteral("tracked"));
        EXPECT_EQ(found->ownerId, 7u);
        EXPECT_EQ(found->ownerName, QStringLiteral("owner"));
        EXPECT_FALSE(found->creationFile.isEmpty());
        EXPECT_GT(found->creationLine, 0u);
    }

    const auto after = DebugIdentity::snapshotAll();
    EXPECT_EQ(after.size(), before.size());
    EXPECT_EQ(std::find_if(after.begin(), after.end(),
                  [id](const DebugIdentitySnapshot& value) { return value.id == id; }),
              after.end());
}

TEST(DebugIdentityStandaloneTest, SnapshotReflectsChangesMadeBeforeCapture)
{
    DebugIdentity identity(QStringLiteral("Node"));
    identity.setDebugName(QStringLiteral("before"));
    const auto id = identity.debugId();
    identity.setDebugName(QStringLiteral("after"));

    const auto snapshots = DebugIdentity::snapshotAll();
    const auto found = std::find_if(snapshots.begin(), snapshots.end(),
        [id](const DebugIdentitySnapshot& value) { return value.id == id; });
    ASSERT_NE(found, snapshots.end());
    EXPECT_EQ(found->name, QStringLiteral("after"));
}

TEST(DebugIdentityStandaloneTest, IdentityIsNeitherCopyableNorMovable)
{
    EXPECT_FALSE(std::is_copy_constructible_v<DebugIdentity>);
    EXPECT_FALSE(std::is_copy_assignable_v<DebugIdentity>);
    EXPECT_FALSE(std::is_move_constructible_v<DebugIdentity>);
    EXPECT_FALSE(std::is_move_assignable_v<DebugIdentity>);
}
