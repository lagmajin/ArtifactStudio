#include <gtest/gtest.h>

#include <QString>
#include <string>
#include <type_traits>
#include <utility>

import Utils.String.UniString;
import Utils.Tag;

using namespace ArtifactCore;

static_assert(std::is_copy_constructible_v<Tag>);
static_assert(std::is_move_constructible_v<Tag>);

TEST(TagContractTest, DefaultTagHasAnEmptyName)
{
    const Tag tag;
    EXPECT_TRUE(tag.name().toQString().isEmpty());
}

TEST(TagContractTest, ConstructionPreservesUnicodeName)
{
    const Tag tag(UniString(QStringLiteral("素材 🎨")));
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("素材 🎨"));
}

TEST(TagContractTest, Utf16AndUtf32UniStringNamesRoundTrip)
{
    const Tag utf16Tag(UniString(std::u16string(u"東京🧩")));
    const Tag utf32Tag(UniString(std::u32string(U"東京🧩")));
    const QString expected = QStringLiteral("東京🧩");

    EXPECT_EQ(utf16Tag.name().toQString(), expected);
    EXPECT_EQ(utf32Tag.name().toQString(), expected);
}

TEST(TagContractTest, LengthAwareUtf8NamePreservesEmbeddedNull)
{
    const std::string utf8("left\0right", 10);
    const Tag tag{UniString(utf8)};
    const QString name = tag.name().toQString();

    ASSERT_EQ(name.size(), 10);
    EXPECT_EQ(name, QString::fromUtf8("left\0right", 10));
    EXPECT_EQ(name.at(4).unicode(), QChar(u'\0').unicode());
}

TEST(TagContractTest, UniStringSetterReplacesTheName)
{
    Tag tag(UniString("before"));
    tag.setName(UniString("after"));
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("after"));
}

TEST(TagContractTest, StringLikeSetterStoresStdStringName)
{
    Tag tag(UniString("before"));
    tag.setName(std::string("after"));
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("after"));
}

TEST(TagContractTest, StringLikeSetterStoresQStringName)
{
    Tag tag(UniString("before"));
    tag.setName(QStringLiteral("after"));
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("after"));
}

TEST(TagContractTest, UniStringSetterInitializesDefaultTag)
{
    Tag tag;
    tag.setName(UniString("initialized"));
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("initialized"));
}

TEST(TagContractTest, EmptyNameCanClearAnExistingTag)
{
    Tag tag(UniString("named"));

    tag.setName(UniString{});

    EXPECT_TRUE(tag.name().toQString().isEmpty());
}

TEST(TagContractTest, CopyConstructionOwnsIndependentNameStorage)
{
    Tag original(UniString("original"));
    Tag copy(original);
    original.setName(UniString("changed"));

    EXPECT_EQ(copy.name().toQString(), QStringLiteral("original"));
    EXPECT_EQ(original.name().toQString(), QStringLiteral("changed"));
}

TEST(TagContractTest, CopyAssignmentReplacesThePreviousName)
{
    Tag source(UniString("source"));
    Tag destination(UniString("old"));
    destination = source;
    source.setName(UniString("updated source"));

    EXPECT_EQ(destination.name().toQString(), QStringLiteral("source"));
    EXPECT_EQ(source.name().toQString(), QStringLiteral("updated source"));
}

TEST(TagContractTest, CopyAssignmentReinitializesMovedFromDestination)
{
    Tag destination(UniString("old"));
    Tag source(UniString("copy source"));
    Tag moved(std::move(destination));

    EXPECT_TRUE(destination.name().toQString().isEmpty());
    destination = source;
    EXPECT_EQ(destination.name().toQString(), QStringLiteral("copy source"));
    EXPECT_EQ(moved.name().toQString(), QStringLiteral("old"));
}

TEST(TagContractTest, MoveAssignmentReinitializesMovedFromDestination)
{
    Tag destination(UniString("old"));
    Tag emptied(std::move(destination));
    Tag source(UniString("move source"));

    EXPECT_TRUE(destination.name().toQString().isEmpty());
    destination = std::move(source);
    EXPECT_EQ(destination.name().toQString(), QStringLiteral("move source"));
    EXPECT_TRUE(source.name().toQString().isEmpty());
    EXPECT_EQ(emptied.name().toQString(), QStringLiteral("old"));
}

TEST(TagContractTest, SelfCopyAssignmentPreservesName)
{
    Tag tag(UniString("self copy"));
    tag = tag;
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("self copy"));
}

TEST(TagContractTest, SelfMoveAssignmentPreservesName)
{
    Tag tag(UniString("self move"));
    tag = std::move(tag);
    EXPECT_EQ(tag.name().toQString(), QStringLiteral("self move"));
}

TEST(TagContractTest, MoveConstructionLeavesSourceEmptyAndReassignable)
{
    Tag source(UniString("moving"));
    Tag moved(std::move(source));

    EXPECT_EQ(moved.name().toQString(), QStringLiteral("moving"));
    EXPECT_TRUE(source.name().toQString().isEmpty());
    source = Tag(UniString("reused"));
    EXPECT_EQ(source.name().toQString(), QStringLiteral("reused"));
}

TEST(TagContractTest, MoveAssignmentReplacesDestinationAndLeavesSourceEmpty)
{
    Tag source(UniString("replacement"));
    Tag destination(UniString("old"));
    destination = std::move(source);

    EXPECT_EQ(destination.name().toQString(), QStringLiteral("replacement"));
    EXPECT_TRUE(source.name().toQString().isEmpty());
}

TEST(TagContractTest, MoveAssignmentFromMovedFromTagEmptiesDestination)
{
    Tag source(UniString("original"));
    Tag moved(std::move(source));
    Tag destination(UniString("destination"));

    destination = std::move(source);

    EXPECT_TRUE(destination.name().toQString().isEmpty());
    EXPECT_TRUE(source.name().toQString().isEmpty());
    EXPECT_EQ(moved.name().toQString(), QStringLiteral("original"));
}
