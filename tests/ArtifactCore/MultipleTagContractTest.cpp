#include <gtest/gtest.h>

#include <QString>
#include <QStringList>

#include <utility>

import Utils.MultipleTag;

using namespace ArtifactCore;

TEST(MultipleTagContractTest, AddTrimsAndDeduplicatesWhileIgnoringWhitespaceOnlyTags)
{
    MultipleTag tags;
    tags.addTag(QStringLiteral("  alpha  "));
    tags.addTag(QStringLiteral("alpha"));
    tags.addTag(QStringLiteral(" \t "));
    EXPECT_EQ(tags.count(), 1);
    EXPECT_TRUE(tags.hasTag(QStringLiteral(" alpha ")));
}

TEST(MultipleTagContractTest, TagIdentityPreservesCase)
{
    MultipleTag tags;
    tags.addTag(QStringLiteral("Render"));
    tags.addTag(QStringLiteral("render"));

    EXPECT_EQ(tags.count(), 2);
    EXPECT_TRUE(tags.hasTag(QStringLiteral("Render")));
    EXPECT_TRUE(tags.hasTag(QStringLiteral("render")));
    EXPECT_FALSE(tags.hasTag(QStringLiteral("RENDER")));
}

TEST(MultipleTagContractTest, BulkAddAndRemoveUseNormalizedTags)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral(" one "), QStringLiteral("two"), QStringLiteral("one")});
    EXPECT_EQ(tags.count(), 2);
    tags.removeTags({QStringLiteral(" two "), QStringLiteral("missing")});
    EXPECT_EQ(tags.sortedTags(), QStringList{QStringLiteral("one")});
}

TEST(MultipleTagContractTest, AnyAndAllQueriesHandleEmptyAndMixedLists)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("red"), QStringLiteral("blue")});
    EXPECT_TRUE(tags.hasAnyTag({QStringLiteral("green"), QStringLiteral(" blue ")}));
    EXPECT_FALSE(tags.hasAnyTag({QStringLiteral("green"), QStringLiteral("yellow")}));
    EXPECT_TRUE(tags.hasAllTags({QStringLiteral("red"), QStringLiteral(" blue ")}));
    EXPECT_FALSE(tags.hasAllTags({QStringLiteral("red"), QStringLiteral("missing")}));
    EXPECT_TRUE(tags.hasAllTags({}));
    EXPECT_FALSE(tags.hasAnyTag({}));
}

TEST(MultipleTagContractTest, SortedTagsAndStringOutputAreDeterministic)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("gamma"), QStringLiteral("alpha"), QStringLiteral("beta")});
    EXPECT_EQ(tags.sortedTags(), (QStringList{QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")}));
    EXPECT_EQ(tags.toString(), QStringLiteral("alpha, beta, gamma"));
    EXPECT_EQ(tags.toString(QStringLiteral("|")), QStringLiteral("alpha|beta|gamma"));
}

TEST(MultipleTagContractTest, PrefixFilterIsCaseInsensitive)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("Render.Fast"), QStringLiteral("render.slow"), QStringLiteral("Audio")});
    QStringList result = tags.filterByPrefix(QStringLiteral("RENDER."));
    result.sort();
    EXPECT_EQ(result, (QStringList{QStringLiteral("Render.Fast"), QStringLiteral("render.slow")}));
}

TEST(MultipleTagContractTest, EmptyPrefixMatchesEveryTag)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("alpha"), QStringLiteral("Beta")});
    QStringList result = tags.filterByPrefix(QString());
    result.sort();

    EXPECT_EQ(result, (QStringList{QStringLiteral("Beta"), QStringLiteral("alpha")}));
}

TEST(MultipleTagContractTest, PatternFilterUsesRegularExpressionMatching)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("item-01"), QStringLiteral("item-12"), QStringLiteral("other-12")});
    QStringList result = tags.filterByPattern(QStringLiteral("^item-\\d+$"));
    result.sort();
    EXPECT_EQ(result, (QStringList{QStringLiteral("item-01"), QStringLiteral("item-12")}));
}

TEST(MultipleTagContractTest, PatternFilterUsesCaseSensitiveSubstringMatching)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("Alpha"), QStringLiteral("alphabet"),
                  QStringLiteral("beta")});

    EXPECT_EQ(tags.filterByPattern(QStringLiteral("alpha")),
              QStringList{QStringLiteral("alphabet")});
    EXPECT_EQ(tags.filterByPattern(QStringLiteral("eta")),
              QStringList{QStringLiteral("beta")});
}

TEST(MultipleTagContractTest, InvalidPatternMatchesNoTags)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("alpha"), QStringLiteral("beta")});

    EXPECT_TRUE(tags.filterByPattern(QStringLiteral("[")).isEmpty());
}

TEST(MultipleTagContractTest, SetOperationsReturnExpectedSetsWithoutChangingOperands)
{
    MultipleTag left;
    left.addTags({QStringLiteral("a"), QStringLiteral("b")});
    MultipleTag right;
    right.addTags({QStringLiteral("b"), QStringLiteral("c")});

    const MultipleTag united = left.unite(right);
    const MultipleTag common = left.intersect(right);
    const MultipleTag difference = left.subtract(right);
    EXPECT_EQ(united.sortedTags(), (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
    EXPECT_EQ(common.sortedTags(), QStringList{QStringLiteral("b")});
    EXPECT_EQ(difference.sortedTags(), QStringList{QStringLiteral("a")});
    EXPECT_EQ(left.sortedTags(), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
    EXPECT_EQ(right.sortedTags(), (QStringList{QStringLiteral("b"), QStringLiteral("c")}));
}

TEST(MultipleTagContractTest, EmptySetIsIdentityForUnionAndSubtraction)
{
    MultipleTag empty;
    MultipleTag values;
    values.addTags({QStringLiteral("a"), QStringLiteral("b")});

    EXPECT_EQ(empty.unite(values), values);
    EXPECT_EQ(values.unite(empty), values);
    EXPECT_TRUE(empty.intersect(values).isEmpty());
    EXPECT_EQ(values.subtract(empty), values);
    EXPECT_TRUE(empty.subtract(values).isEmpty());
}

TEST(MultipleTagContractTest, SetOperationsAreIdempotentForTheSameOperand)
{
    MultipleTag values;
    values.addTags({QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("one")});

    EXPECT_EQ(values.unite(values), values);
    EXPECT_EQ(values.intersect(values), values);
    EXPECT_TRUE(values.subtract(values).isEmpty());
    EXPECT_EQ(values.sortedTags(), (QStringList{QStringLiteral("one"), QStringLiteral("two")}));
}

TEST(MultipleTagContractTest, FromStringTrimsSkipsEmptyFieldsAndReplacesExistingTags)
{
    MultipleTag tags;
    tags.addTag(QStringLiteral("old"));
    tags.fromString(QStringLiteral(" first, ,second,first "));
    EXPECT_EQ(tags.sortedTags(), (QStringList{QStringLiteral("first"), QStringLiteral("second")}));
}

TEST(MultipleTagContractTest, CustomSeparatorRoundTripsNormalizedTags)
{
    MultipleTag tags;
    tags.fromString(QStringLiteral(" red | blue |red| "), QStringLiteral("|"));

    EXPECT_EQ(tags.sortedTags(), (QStringList{QStringLiteral("blue"), QStringLiteral("red")}));
    EXPECT_EQ(tags.toString(QStringLiteral("|")), QStringLiteral("blue|red"));

    MultipleTag restored;
    restored.fromString(tags.toString(QStringLiteral("|")), QStringLiteral("|"));
    EXPECT_EQ(restored, tags);
}

TEST(MultipleTagContractTest, MultiCharacterSeparatorParsesAndSerializes)
{
    MultipleTag tags;
    tags.fromString(QStringLiteral("one::<empty>::two::one"), QStringLiteral("::"));

    EXPECT_EQ(tags.sortedTags(), (QStringList{QStringLiteral("<empty>"),
                                               QStringLiteral("one"),
                                               QStringLiteral("two")}));
    EXPECT_EQ(tags.toString(QStringLiteral("::")),
              QStringLiteral("<empty>::one::two"));
}

TEST(MultipleTagContractTest, EmptyStringParsingClearsExistingTags)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("first"), QStringLiteral("second")});

    tags.fromString(QString());

    EXPECT_TRUE(tags.isEmpty());
    EXPECT_EQ(tags.count(), 0);
}

TEST(MultipleTagContractTest, ClearLeavesAnEmptyReusableSet)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("first"), QStringLiteral("second")});
    tags.clear();
    tags.clear();

    EXPECT_TRUE(tags.isEmpty());
    EXPECT_EQ(tags.count(), 0);
    EXPECT_TRUE(tags.tags().isEmpty());
    tags.addTag(QStringLiteral("reused"));
    EXPECT_TRUE(tags.hasTag(QStringLiteral("reused")));
}

TEST(MultipleTagContractTest, CopyConstructionAndAssignmentAreIndependent)
{
    MultipleTag original;
    original.addTags({QStringLiteral("keep"), QStringLiteral("remove")});
    MultipleTag copied(original);
    copied.removeTag(QStringLiteral("remove"));
    EXPECT_TRUE(original.hasTag(QStringLiteral("remove")));

    MultipleTag assigned;
    assigned = original;
    assigned.clear();
    EXPECT_EQ(original.count(), 2);
}

TEST(MultipleTagContractTest, SelfCopyAndMoveAssignmentPreserveTheTagSet)
{
    MultipleTag tags;
    tags.addTags({QStringLiteral("alpha"), QStringLiteral("beta")});

    tags = tags;
    EXPECT_EQ(tags.sortedTags(),
              (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));

    tags = std::move(tags);
    EXPECT_EQ(tags.sortedTags(),
              (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));
}

TEST(MultipleTagContractTest, MoveConstructionAndAssignmentTransferContents)
{
    MultipleTag source;
    source.addTags({QStringLiteral("x"), QStringLiteral("y")});
    MultipleTag moved(std::move(source));
    EXPECT_EQ(moved.sortedTags(), (QStringList{QStringLiteral("x"), QStringLiteral("y")}));

    MultipleTag destination;
    destination.addTag(QStringLiteral("old"));
    destination = std::move(moved);
    EXPECT_EQ(destination.sortedTags(), (QStringList{QStringLiteral("x"), QStringLiteral("y")}));
}

TEST(MultipleTagContractTest, MoveAssignmentReinitializesMovedFromDestination)
{
    MultipleTag destination;
    destination.addTag(QStringLiteral("old"));
    MultipleTag moved(std::move(destination));
    MultipleTag source;
    source.addTags({QStringLiteral("new-a"), QStringLiteral("new-b")});

    destination = std::move(source);

    EXPECT_EQ(destination.sortedTags(),
              (QStringList{QStringLiteral("new-a"), QStringLiteral("new-b")}));
    EXPECT_TRUE(source.isEmpty());
    EXPECT_EQ(moved.sortedTags(), QStringList{QStringLiteral("old")});
}

