#include <gtest/gtest.h>

import Core.ArtifactExpected;

using namespace ArtifactCore;

TEST(ArtifactExpectedTest, ValueAndErrorPathsRemainExplicit)
{
    ArtifactExpected<int> value(4);
    EXPECT_TRUE(value.hasValue());
    EXPECT_EQ(value.value(), 4);

    ArtifactExpected<int> error(
        ArtifactExpectedError{ArtifactExpectedErrorCode::Failed, "failed"});
    EXPECT_TRUE(error.hasError());
    EXPECT_EQ(error.valueOr(9), 9);
    EXPECT_EQ(error.error().code, ArtifactExpectedErrorCode::Failed);
}
