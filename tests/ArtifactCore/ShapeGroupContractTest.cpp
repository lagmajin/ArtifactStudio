#include <gtest/gtest.h>

#include <QRectF>
#include <QString>

#include <memory>

import Shape.Group;
import Shape.Operator;
import Shape.Path;
import Shape.TrimPaths;
import Shape.Types;

namespace {

using ArtifactCore::PathShape;
using ArtifactCore::ShapeElement;
using ArtifactCore::ShapeGroup;
using ArtifactCore::ShapePath;

std::unique_ptr<PathShape> rectanglePath(const QRectF& rect)
{
    ShapePath path;
    path.setRectangle(rect);
    return std::make_unique<PathShape>(path);
}

} // namespace

TEST(ShapeGroupContractTest, ChildrenKeepInsertionOrderAndParentLinks)
{
    ShapeGroup group;
    auto first = rectanglePath(QRectF(0.0, 0.0, 2.0, 2.0));
    auto second = rectanglePath(QRectF(4.0, 0.0, 2.0, 2.0));
    auto inserted = rectanglePath(QRectF(8.0, 0.0, 2.0, 2.0));
    PathShape* firstRaw = first.get();
    PathShape* secondRaw = second.get();
    PathShape* insertedRaw = inserted.get();
    ShapeGroup otherGroup;
    auto outsider = rectanglePath(QRectF(20.0, 0.0, 2.0, 2.0));
    PathShape* outsiderRaw = outsider.get();
    otherGroup.addChild(std::move(outsider));

    group.addChild(std::move(first));
    group.addChild(std::move(second));
    group.insertChild(1, std::move(inserted));

    ASSERT_EQ(group.childCount(), 3);
    EXPECT_EQ(group.childAt(0), firstRaw);
    EXPECT_EQ(group.childAt(1), insertedRaw);
    EXPECT_EQ(group.childAt(2), secondRaw);
    EXPECT_EQ(group.indexOf(firstRaw), 0);
    EXPECT_EQ(group.indexOf(insertedRaw), 1);
    EXPECT_EQ(group.indexOf(secondRaw), 2);
    EXPECT_EQ(group.indexOf(outsiderRaw), -1);
    EXPECT_EQ(firstRaw->parent(), &group);
    EXPECT_EQ(insertedRaw->parent(), &group);
    EXPECT_EQ(secondRaw->parent(), &group);
    EXPECT_EQ(group.childAt(-1), nullptr);
    EXPECT_EQ(group.childAt(3), nullptr);
    EXPECT_EQ(group.indexOf(nullptr), -1);

    group.removeChild(1);
    EXPECT_EQ(group.childCount(), 2);
    EXPECT_EQ(group.childAt(1), secondRaw);

    group.removeChild(firstRaw);
    EXPECT_EQ(group.childCount(), 1);

    group.removeChild(-1);
    group.removeChild(99);
    group.removeChild(outsiderRaw);
    EXPECT_EQ(group.childCount(), 1);

    group.clearChildren();
    group.clearChildren();
    EXPECT_EQ(group.childCount(), 0);
}

TEST(ShapeGroupContractTest, InvalidAndNullChildrenDoNotCreateEntries)
{
    ShapeGroup group;
    group.addChild(nullptr);
    group.insertChild(0, nullptr);
    group.addOperator(std::unique_ptr<ArtifactCore::ShapeOperator>{});

    EXPECT_EQ(group.childCount(), 0);
    EXPECT_EQ(group.childAt(0), nullptr);
    EXPECT_EQ(group.indexOf(nullptr), -1);
    EXPECT_EQ(group.operatorCount(), 0);
    EXPECT_EQ(group.operatorAt(-1), nullptr);
    EXPECT_EQ(group.operatorAt(0), nullptr);
    EXPECT_EQ(group.addOperator(ArtifactCore::ShapeOperatorType::None), nullptr);
}

TEST(ShapeGroupContractTest, InsertOutsideExistingRangeAppends)
{
    ShapeGroup group;
    auto first = rectanglePath(QRectF(0.0, 0.0, 1.0, 1.0));
    auto second = rectanglePath(QRectF(2.0, 0.0, 1.0, 1.0));
    PathShape* firstRaw = first.get();
    PathShape* secondRaw = second.get();

    group.insertChild(-1, std::move(first));
    group.insertChild(99, std::move(second));

    ASSERT_EQ(group.childCount(), 2);
    EXPECT_EQ(group.childAt(0), firstRaw);
    EXPECT_EQ(group.childAt(1), secondRaw);
}

TEST(ShapeGroupContractTest, ProcessedPathsComposeNestedGroupAndPathTransforms)
{
    ShapeGroup root;
    root.transform().position = {10.0, 0.0};

    auto nested = std::make_unique<ShapeGroup>();
    nested->transform().position = {0.0, 20.0};
    auto path = rectanglePath(QRectF(0.0, 0.0, 10.0, 10.0));
    path->transform().position = {5.0, 0.0};
    nested->addChild(std::move(path));
    root.addChild(std::move(nested));

    const auto processed = root.processedPaths();
    ASSERT_EQ(processed.size(), 1u);
    const QRectF bounds = processed.front().boundingRect();
    EXPECT_NEAR(bounds.x(), 15.0, 1e-9);
    EXPECT_NEAR(bounds.y(), 20.0, 1e-9);
    EXPECT_NEAR(bounds.width(), 10.0, 1e-9);
    EXPECT_NEAR(bounds.height(), 10.0, 1e-9);
}

TEST(ShapeGroupContractTest, ProcessedPathsExcludeInvisibleChildrenAndGroups)
{
    ShapeGroup root;
    root.addChild(rectanglePath(QRectF(0.0, 0.0, 2.0, 2.0)));

    auto hiddenPath = rectanglePath(QRectF(10.0, 0.0, 2.0, 2.0));
    hiddenPath->setVisible(false);
    root.addChild(std::move(hiddenPath));

    auto hiddenGroup = std::make_unique<ShapeGroup>();
    hiddenGroup->setVisible(false);
    hiddenGroup->addChild(rectanglePath(QRectF(20.0, 0.0, 2.0, 2.0)));
    root.addChild(std::move(hiddenGroup));

    auto processed = root.processedPaths();
    ASSERT_EQ(processed.size(), 1u);
    EXPECT_NEAR(processed.front().boundingRect().x(), 0.0, 1e-9);

    root.setVisible(false);
    EXPECT_TRUE(root.processedPaths().empty());
}

TEST(ShapeGroupContractTest, CloneDeepCopiesHierarchyTransformsAndOperatorState)
{
    ShapeGroup source;
    source.setName(QStringLiteral("source"));
    source.setLocked(true);
    source.transform().position = {3.0, 4.0};
    auto* trim = dynamic_cast<ArtifactCore::TrimPaths*>(
        source.addOperator(ArtifactCore::ShapeOperatorType::TrimPaths));
    ASSERT_NE(trim, nullptr);
    trim->setStart(10.0f);
    trim->setEnd(70.0f);

    ShapePath line;
    line.moveTo(0.0, 0.0);
    line.lineTo(100.0, 0.0);
    auto child = std::make_unique<PathShape>(line);
    child->transform().position = {2.0, 0.0};
    PathShape* sourcePath = child.get();
    source.addChild(std::move(child));

    std::unique_ptr<ShapeElement> clonedElement = source.clone();
    auto* cloned = dynamic_cast<ShapeGroup*>(clonedElement.get());
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->name(), QStringLiteral("source"));
    EXPECT_TRUE(cloned->isLocked());
    EXPECT_EQ(cloned->transform().position.x, 3.0);
    ASSERT_EQ(cloned->childCount(), 1);
    ASSERT_EQ(cloned->operatorCount(), 1);
    EXPECT_NE(cloned->childAt(0), sourcePath);
    EXPECT_EQ(cloned->childAt(0)->parent(), cloned);

    auto* clonedTrim = dynamic_cast<ArtifactCore::TrimPaths*>(cloned->operatorAt(0));
    ASSERT_NE(clonedTrim, nullptr);
    EXPECT_FLOAT_EQ(clonedTrim->start(), 10.0f);
    EXPECT_FLOAT_EQ(clonedTrim->end(), 70.0f);
    const auto sourcePaths = source.processedPaths();
    const auto clonedPaths = cloned->processedPaths();
    ASSERT_EQ(sourcePaths.size(), 1u);
    ASSERT_EQ(clonedPaths.size(), 1u);
    EXPECT_NEAR(sourcePaths.front().boundingRect().x(), 15.0, 1e-6);
    EXPECT_NEAR(sourcePaths.front().boundingRect().width(), 60.0, 1e-6);
    EXPECT_NEAR(clonedPaths.front().boundingRect().width(), 60.0, 1e-6);

    clonedTrim->setEnd(35.0f);
    EXPECT_FLOAT_EQ(trim->end(), 70.0f);
    EXPECT_NEAR(cloned->processedPaths().front().boundingRect().width(), 25.0, 1e-6);
    EXPECT_NEAR(source.processedPaths().front().boundingRect().width(), 60.0, 1e-6);

    auto* clonedPath = dynamic_cast<PathShape*>(cloned->childAt(0));
    ASSERT_NE(clonedPath, nullptr);
    clonedPath->path().clear();
    EXPECT_FALSE(sourcePath->path().isEmpty());
}
