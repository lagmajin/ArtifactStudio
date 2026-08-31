#include <gtest/gtest.h>

#include <limits>

import Core.ArtifactSaturation;

using namespace ArtifactCore;

TEST(ArtifactSaturationTest, AvoidsIntegralOverflow)
{
    EXPECT_EQ(addSat<int>(std::numeric_limits<int>::max(), 1),
              std::numeric_limits<int>::max());
    EXPECT_EQ(subSat<int>(std::numeric_limits<int>::lowest(), 1),
              std::numeric_limits<int>::lowest());
    EXPECT_EQ(mulSat<int>(std::numeric_limits<int>::max(), 2),
              std::numeric_limits<int>::max());
    EXPECT_EQ(saturateCast<unsigned char>(-1), 0);
}
