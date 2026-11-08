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

TEST(ArtifactFunctionRefTest, ConvertsReturnValueFromBorrowedCallableAndKeepsItsState)
{
    int calls = 0;
    auto nextValue = [&calls]() { return ++calls; };
    ArtifactFunctionRef<double()> ref(nextValue);

    EXPECT_TRUE(ref.isValid());
    EXPECT_DOUBLE_EQ(ref(), 1.0);
    EXPECT_DOUBLE_EQ(ref(), 2.0);
    EXPECT_EQ(calls, 2);
}
