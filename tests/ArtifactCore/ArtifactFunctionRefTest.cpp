#include <gtest/gtest.h>

import Core.ArtifactFunctionRef;

using namespace ArtifactCore;

TEST(ArtifactFunctionRefTest, InvokesBorrowedCallable)
{
    int multiplier = 2;
    auto multiply = [&multiplier](const int value) { return value * multiplier; };
    ArtifactFunctionRef<int(int)> ref(multiply);

    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ(ref(3), 6);
    multiplier = 4;
    EXPECT_EQ(ref(3), 12);
}
