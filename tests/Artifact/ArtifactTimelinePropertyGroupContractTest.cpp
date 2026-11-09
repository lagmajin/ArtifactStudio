#include <gtest/gtest.h>

#include <initializer_list>

#include <QString>

import Artifact.Timeline.KeyframeModel;
import Memory.SharedPtr;
import Property.Abstract;
import Property.Group;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

PropertyGroup makeGroup(const QString& name,
                        std::initializer_list<QString> propertyPaths)
{
    PropertyGroup group(name);
    for (const auto& path : propertyPaths) {
        auto property = makeShared<AbstractProperty>();
        property->setName(path);
        group.addProperty(property);
    }
    return group;
}

} // namespace

TEST(ArtifactTimelinePropertyGroupContractTest,
     ShowsOnlyGroupsWhoseEveryPropertyUsesTextAnimatorPath)
{
    const auto valid = makeGroup(
        QStringLiteral("Animator Controls"),
        {QStringLiteral("text.animators.0.opacity"),
         QStringLiteral("text.animators.0.tracking.amount")});
    const auto matchingLabelOnly = makeGroup(
        QStringLiteral("Text Animator 1"),
        {QStringLiteral("components.cloner.amount")});
    const auto mixed = makeGroup(
        QStringLiteral("Animator Controls"),
        {QStringLiteral("text.animators.0.opacity"),
         QStringLiteral("transform.position.x")});
    const auto invalidIndex = makeGroup(
        QStringLiteral("Animator Controls"),
        {QStringLiteral("text.animators.first.opacity")});
    const auto empty = makeGroup(QStringLiteral("Animator Controls"), {});

    EXPECT_FALSE(ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(valid));
    EXPECT_TRUE(
        ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(matchingLabelOnly));
    EXPECT_TRUE(ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(mixed));
    EXPECT_TRUE(
        ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(invalidIndex));
    EXPECT_TRUE(ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(empty));
}
