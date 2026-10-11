#include <gtest/gtest.h>

#include <utility>

import Math.RotationTurns;

using ArtifactCore::RotationTurns;

TEST(RotationTurnsStandaloneTest, DefaultConstructionStartsAtZero)
{
    const RotationTurns value;
    EXPECT_EQ(value.rotations(), 0);
    EXPECT_DOUBLE_EQ(value.degrees(), 0.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 0.0);
}

TEST(RotationTurnsStandaloneTest, ConstructorCarriesPositiveExcessDegrees)
{
    const RotationTurns value(2, 450.0);
    EXPECT_EQ(value.rotations(), 3);
    EXPECT_DOUBLE_EQ(value.degrees(), 90.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 1170.0);
}

TEST(RotationTurnsStandaloneTest, ConstructorBorrowsForNegativeDegrees)
{
    const RotationTurns value(2, -90.0);
    EXPECT_EQ(value.rotations(), 1);
    EXPECT_DOUBLE_EQ(value.degrees(), 270.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 630.0);
}

TEST(RotationTurnsStandaloneTest, ConstructorNormalizesExactTurnBoundaries)
{
    const RotationTurns positive(4, 360.0);
    EXPECT_EQ(positive.rotations(), 5);
    EXPECT_DOUBLE_EQ(positive.degrees(), 0.0);

    const RotationTurns negative(4, -360.0);
    EXPECT_EQ(negative.rotations(), 3);
    EXPECT_DOUBLE_EQ(negative.degrees(), 0.0);
}

TEST(RotationTurnsStandaloneTest, TotalDegreesConstructorSplitsPositiveTurns)
{
    const RotationTurns value(1080.0);
    EXPECT_EQ(value.rotations(), 3);
    EXPECT_DOUBLE_EQ(value.degrees(), 0.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 1080.0);
}

TEST(RotationTurnsStandaloneTest, TotalDegreesConstructorRepresentsNegativeAngles)
{
    const RotationTurns value(-450.0);
    EXPECT_EQ(value.rotations(), -2);
    EXPECT_DOUBLE_EQ(value.degrees(), 270.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), -450.0);
}

TEST(RotationTurnsStandaloneTest, SetNormalizesRotationAndDegreePair)
{
    RotationTurns value;
    value.set(-1, 810.0);
    EXPECT_EQ(value.rotations(), 1);
    EXPECT_DOUBLE_EQ(value.degrees(), 90.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 450.0);
}

TEST(RotationTurnsStandaloneTest, SetTotalDegreesReplacesPreviousTurns)
{
    RotationTurns value(8, 120.0);
    value.setTotalDegrees(-90.0);
    EXPECT_EQ(value.rotations(), -1);
    EXPECT_DOUBLE_EQ(value.degrees(), 270.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), -90.0);
}

TEST(RotationTurnsStandaloneTest, SetRotationsPreservesNormalizedFraction)
{
    RotationTurns value(1, 45.0);
    value.setRotations(-3);
    EXPECT_EQ(value.rotations(), -3);
    EXPECT_DOUBLE_EQ(value.degrees(), 45.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), -1035.0);
}

TEST(RotationTurnsStandaloneTest, SetDegreesNormalizesAndCarriesIntoTurns)
{
    RotationTurns value(2, 30.0);
    value.setDegrees(750.0);
    EXPECT_EQ(value.rotations(), 4);
    EXPECT_DOUBLE_EQ(value.degrees(), 30.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 1470.0);
}

TEST(RotationTurnsStandaloneTest, MoveConstructionTransfersValue)
{
    RotationTurns source(-2, 725.0);
    RotationTurns moved(std::move(source));
    EXPECT_EQ(moved.rotations(), 0);
    EXPECT_DOUBLE_EQ(moved.degrees(), 5.0);
    EXPECT_DOUBLE_EQ(moved.getTotalDegrees(), 5.0);
}

TEST(RotationTurnsStandaloneTest, MoveAssignmentReplacesDestinationValue)
{
    RotationTurns source(3, -45.0);
    RotationTurns destination(9, 10.0);
    destination = std::move(source);
    EXPECT_EQ(destination.rotations(), 2);
    EXPECT_DOUBLE_EQ(destination.degrees(), 315.0);
    EXPECT_DOUBLE_EQ(destination.getTotalDegrees(), 1035.0);
}

TEST(RotationTurnsStandaloneTest, SelfMoveAssignmentPreservesValue)
{
    RotationTurns value(5, 90.0);
    value = std::move(value);
    EXPECT_EQ(value.rotations(), 5);
    EXPECT_DOUBLE_EQ(value.degrees(), 90.0);
    EXPECT_DOUBLE_EQ(value.getTotalDegrees(), 1890.0);
}
