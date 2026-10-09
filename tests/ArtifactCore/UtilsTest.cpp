#include <gtest/gtest.h>
#include <QString>
#include <string>
#include <string_view>

import Utils.String;
import Utils.Path;
import Utils.Convertor.String;
import Utils.String.Like;

using namespace ArtifactCore;

TEST(UtilsTest, LevenshteinDistance) {
    // 完全一致
    EXPECT_EQ(calculateLevenshteinDistance("test", "test"), 0);
    
    // 1文字違い
    EXPECT_EQ(calculateLevenshteinDistance("test", "tesx"), 1);
    
    // 1文字削除
    EXPECT_EQ(calculateLevenshteinDistance("test", "tes"), 1);
    
    // 1文字追加
    EXPECT_EQ(calculateLevenshteinDistance("test", "testx"), 1);
    
    // 空文字列
    EXPECT_EQ(calculateLevenshteinDistance("", "test"), 4);
    EXPECT_EQ(calculateLevenshteinDistance("test", ""), 4);
    EXPECT_EQ(calculateLevenshteinDistance("", ""), 0);
    
    // 完全に異なる
    EXPECT_EQ(calculateLevenshteinDistance("abc", "def"), 3);
}

TEST(UtilsTest, StringConvertor) {
    EXPECT_EQ(toStdString(QString("test")), "test");
    EXPECT_EQ(toStdU16String(QString("test")), u"test");
}

TEST(UtilsTest, PathUtils) {
    EXPECT_FALSE(getAppPath().isEmpty());
    EXPECT_FALSE(getExeDir().isEmpty());
    EXPECT_FALSE(getCurrentDir().isEmpty());
    EXPECT_FALSE(getIconPath().isEmpty());
    EXPECT_FALSE(getIconResourceRoot().isEmpty());
    
    // 解像度テスト
    EXPECT_TRUE(resolveIconPath("visibility.svg").contains("Icon"));
    EXPECT_TRUE(resolveIconResourcePath("visibility.svg").startsWith(":/icons"));
}

TEST(UtilsTest, StringLike) {
    EXPECT_EQ(toQString("test"), "test");
    EXPECT_EQ(toQString(std::string("test")), "test");
    EXPECT_EQ(toQString(std::string_view("test")), "test");
}

TEST(UtilsTest, StringConversionsPreserveEmbeddedNullBytes)
{
    const std::string bytes("a\0b", 3);
    const QString fromString = toQString(bytes);
    const QString fromView = toQString(std::string_view(bytes.data(), bytes.size()));

    ASSERT_EQ(fromString.size(), 3);
    ASSERT_EQ(fromView.size(), 3);
    EXPECT_EQ(fromString.at(0).unicode(), u'a');
    EXPECT_TRUE(fromString.at(1).isNull());
    EXPECT_EQ(fromString.at(2).unicode(), u'b');
    EXPECT_EQ(fromView, fromString);

    const std::string roundTrip = toStdString(fromView);
    ASSERT_EQ(roundTrip.size(), bytes.size());
    EXPECT_EQ(roundTrip, bytes);
}
