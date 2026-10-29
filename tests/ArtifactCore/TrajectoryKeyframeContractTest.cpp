#include <gtest/gtest.h>

#include <QPointF>
#include <QVector>

#include <limits>

import Animation.KeyframePatternGenerator;

using ArtifactCore::KeyframePatternGenerator;

TEST(TrajectoryKeyframeContractTest, EmptyOrEntirelyInvalidTrajectoryProducesNoKeys)
{
    const QVector<QPointF> empty;
    const QVector<QPointF> invalid = {
        QPointF(std::numeric_limits<double>::quiet_NaN(), 1.0),
        QPointF(2.0, std::numeric_limits<double>::infinity()),
        QPointF(-std::numeric_limits<double>::infinity(), 3.0),
    };

    EXPECT_TRUE(KeyframePatternGenerator::generateFromTrajectory(empty, 0, 4).isEmpty());
    EXPECT_TRUE(KeyframePatternGenerator::generateFromTrajectory(invalid, 0, 4).isEmpty());
}

TEST(TrajectoryKeyframeContractTest, NonPositiveFrameCountProducesNoKeys)
{
    const QVector<QPointF> trajectory = {QPointF(1.0, 2.0), QPointF(3.0, 4.0)};
    EXPECT_TRUE(KeyframePatternGenerator::generateFromTrajectory(trajectory, 7, 0).isEmpty());
    EXPECT_TRUE(KeyframePatternGenerator::generateFromTrajectory(trajectory, 7, -3).isEmpty());
}

TEST(TrajectoryKeyframeContractTest, SingleOutputFrameUsesFirstValidSample)
{
    const QVector<QPointF> trajectory = {
        QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0),
        QPointF(4.5, -2.0),
        QPointF(9.0, 8.0),
    };

    const auto result = KeyframePatternGenerator::generateFromTrajectory(
        trajectory, -12, 1, 24);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.front().time.value(), -12);
    EXPECT_EQ(result.front().time.scale(), 24);
    EXPECT_EQ(result.front().value, QPointF(4.5, -2.0));
}

TEST(TrajectoryKeyframeContractTest, SingleValidSampleIsRepeatedAcrossRequestedFrames)
{
    const QVector<QPointF> trajectory = {
        QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0),
        QPointF(7.0, -3.0),
        QPointF(0.0, std::numeric_limits<double>::infinity()),
    };

    const auto result = KeyframePatternGenerator::generateFromTrajectory(
        trajectory, 5, 3, 30);

    ASSERT_EQ(result.size(), 3);
    for (qsizetype index = 0; index < result.size(); ++index) {
        EXPECT_EQ(result[index].time.value(), 5 + index);
        EXPECT_EQ(result[index].time.scale(), 30);
        EXPECT_EQ(result[index].value, QPointF(7.0, -3.0));
    }
}

TEST(TrajectoryKeyframeContractTest, InvalidFrameScaleIsClampedToOne)
{
    const QVector<QPointF> trajectory = {QPointF(0.0, 0.0), QPointF(2.0, 4.0)};
    const auto zeroScale = KeyframePatternGenerator::generateFromTrajectory(
        trajectory, 3, 2, 0);
    const auto negativeScale = KeyframePatternGenerator::generateFromTrajectory(
        trajectory, 3, 2, -24);

    ASSERT_EQ(zeroScale.size(), 2);
    ASSERT_EQ(negativeScale.size(), 2);
    EXPECT_EQ(zeroScale[0].time.scale(), 1);
    EXPECT_EQ(negativeScale[1].time.scale(), 1);
}

TEST(TrajectoryKeyframeContractTest, NegativeStartFrameAndRequestedCountArePreserved)
{
    const QVector<QPointF> trajectory = {
        QPointF(0.0, 0.0), QPointF(10.0, 20.0), QPointF(20.0, 40.0)};
    const auto result = KeyframePatternGenerator::generateFromTrajectory(
        trajectory, -5, 4, 48);

    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result.front().time.value(), -5);
    EXPECT_EQ(result.back().time.value(), -2);
    EXPECT_EQ(result.front().time.scale(), 48);
    EXPECT_EQ(result.back().time.scale(), 48);
    EXPECT_NEAR(result[1].value.x(), 20.0 / 3.0, 1e-12);
    EXPECT_NEAR(result[1].value.y(), 40.0 / 3.0, 1e-12);
    EXPECT_NEAR(result[2].value.x(), 40.0 / 3.0, 1e-12);
    EXPECT_NEAR(result[2].value.y(), 80.0 / 3.0, 1e-12);
}
