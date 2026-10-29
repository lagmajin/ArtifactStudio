#include <gtest/gtest.h>

#include <QImage>
#include <QMatrix3x3>
#include <QVector3D>
#include <QtGui/QRgb>

import Video.Stabilizer;

TEST(VideoStabilizerContractTest,
     FrameMotionMatrixAndInverseRoundTripPointWithRotationAndScale)
{
    ArtifactCore::FrameMotion motion;
    motion.x = 4.0;
    motion.y = -3.0;
    motion.rotation = 1.5707963267948966;
    motion.scale = 2.0;

    const QMatrix3x3 forward = motion.toMatrix();
    const QMatrix3x3 inverse = motion.inverted().toMatrix();
    const QVector3D source(1.25f, -2.0f, 1.0f);
    const QVector3D mapped(
        forward(0, 0) * source.x() + forward(0, 1) * source.y() + forward(0, 2),
        forward(1, 0) * source.x() + forward(1, 1) * source.y() + forward(1, 2),
        1.0f);
    const QVector3D restored(
        inverse(0, 0) * mapped.x() + inverse(0, 1) * mapped.y() + inverse(0, 2),
        inverse(1, 0) * mapped.x() + inverse(1, 1) * mapped.y() + inverse(1, 2),
        1.0f);

    EXPECT_NEAR(mapped.x(), 8.0f, 1.0e-5f);
    EXPECT_NEAR(mapped.y(), -0.5f, 1.0e-5f);
    EXPECT_NEAR(restored.x(), source.x(), 1.0e-5f);
    EXPECT_NEAR(restored.y(), source.y(), 1.0e-5f);
}

TEST(VideoStabilizerContractTest,
     LiveStabilizerPassesThroughFirstFrameAndResetKeepsHistoryLimit)
{
    ArtifactCore::LiveStabilizer stabilizer;
    stabilizer.setMaxHistorySize(4);

    QImage source(3, 2, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(12, 34, 56, 78));
    source.setPixel(1, 0, qRgba(90, 87, 65, 43));
    source.setPixel(2, 1, qRgba(210, 180, 150, 120));

    const auto first = stabilizer.processFrame(source);
    ASSERT_EQ(first.size(), source.size());
    EXPECT_EQ(first.pixel(0, 0), source.pixel(0, 0));
    EXPECT_EQ(first.pixel(1, 0), source.pixel(1, 0));
    EXPECT_EQ(first.pixel(2, 1), source.pixel(2, 1));

    stabilizer.reset();
    EXPECT_EQ(stabilizer.maxHistorySize(), 4);

    const auto afterReset = stabilizer.processFrame(source);
    ASSERT_EQ(afterReset.size(), source.size());
    EXPECT_EQ(afterReset.pixel(0, 0), source.pixel(0, 0));
    EXPECT_EQ(afterReset.pixel(1, 0), source.pixel(1, 0));
    EXPECT_EQ(afterReset.pixel(2, 1), source.pixel(2, 1));
}
