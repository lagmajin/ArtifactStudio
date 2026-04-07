#include <gtest/gtest.h>
#include <QString>
#include "Utils/String.ixx"

using namespace ArtifactCore;

TEST(StringTest, LevenshteinDistance) {
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
