#include <gtest/gtest.h>

#include <QString>

import Property.Path;

using namespace ArtifactCore;

TEST(PropertyPathTest, NormalizesRepeatedLeadingAndTrailingSeparators)
{
    const PropertyPath path(QStringLiteral("..layer...transform.x.."));

    EXPECT_TRUE(path.isValid());
    EXPECT_FALSE(path.isEmpty());
    EXPECT_EQ(path.toString(), QStringLiteral("layer.transform.x"));
    EXPECT_EQ(path.depth(), 3);
}

TEST(PropertyPathTest, ExposesHierarchyAndSafeSegmentBounds)
{
    const PropertyPath path(QStringLiteral("effect.color.hue"));

    EXPECT_EQ(path.segment(0), QStringLiteral("effect"));
    EXPECT_EQ(path.segment(1), QStringLiteral("color"));
    EXPECT_EQ(path.segment(2), QStringLiteral("hue"));
    EXPECT_TRUE(path.segment(-1).isEmpty());
    EXPECT_TRUE(path.segment(3).isEmpty());
    EXPECT_EQ(path.ownerPath(), QStringLiteral("effect.color"));
    EXPECT_EQ(path.propertyName(), QStringLiteral("hue"));
    EXPECT_EQ(path.parent().toString(), QStringLiteral("effect.color"));
}

TEST(PropertyPathTest, PrefixAndRelativeOperationsUseWholeSegments)
{
    const PropertyPath path(QStringLiteral("effect.color.hue"));
    const PropertyPath prefix(QStringLiteral("effect.color"));
    const PropertyPath partial(QStringLiteral("effect.col"));

    EXPECT_TRUE(path.startsWith(prefix));
    EXPECT_FALSE(path.startsWith(partial));
    EXPECT_EQ(path.relativeTo(prefix).toString(), QStringLiteral("hue"));
    EXPECT_FALSE(path.relativeTo(partial).isValid());
    EXPECT_TRUE(prefix.relativeTo(prefix).isEmpty());
}

TEST(PropertyPathTest, AppendAndEmptyPathPreserveValueSemantics)
{
    const PropertyPath path(QStringLiteral("layer.transform"));
    const PropertyPath appended = path.appended(QStringLiteral("rotation"));
    const PropertyPath unchanged = path.appended(QString());
    const PropertyPath fromEmpty = PropertyPath().appended(QStringLiteral("opacity"));

    EXPECT_EQ(appended.toString(), QStringLiteral("layer.transform.rotation"));
    EXPECT_EQ(unchanged, path);
    EXPECT_EQ(path.toString(), QStringLiteral("layer.transform"));
    EXPECT_EQ(fromEmpty.toString(), QStringLiteral("opacity"));
}

TEST(PropertyPathTest, SingleSegmentIsAValidLeafWithoutOwnerOrParent)
{
    const PropertyPath path(QStringLiteral("opacity"));

    ASSERT_TRUE(path.isValid());
    EXPECT_EQ(path.depth(), 1);
    EXPECT_EQ(path.propertyName(), QStringLiteral("opacity"));
    EXPECT_TRUE(path.ownerPath().isEmpty());
    EXPECT_TRUE(path.parent().isEmpty());
    EXPECT_EQ(path.parent().depth(), 0);
}
