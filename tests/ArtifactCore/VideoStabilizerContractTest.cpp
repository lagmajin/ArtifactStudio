#include <gtest/gtest.h>

#include <QImage>
#include <QMatrix3x3>
#include <QPointF>
#include <QSize>
#include <QTemporaryDir>
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
     FrameMotionInverseRoundTripsAcrossRotationScaleAndTranslationDomain)
{
    const double rotations[] = {
        -3.141592653589793, -1.2, -0.01, 0.0, 0.7, 1.5707963267948966,
        3.141592653589793
    };
    const double scales[] = {0.125, 0.5, 1.0, 3.75};
    const QPointF points[] = {
        QPointF(0.0, 0.0), QPointF(1.25, -2.0), QPointF(-17.0, 8.5)
    };

    for (const double rotation : rotations) {
        for (const double scale : scales) {
            ArtifactCore::FrameMotion motion;
            motion.x = -23.5;
            motion.y = 11.25;
            motion.rotation = rotation;
            motion.scale = scale;

            const QMatrix3x3 forward = motion.toMatrix();
            const QMatrix3x3 inverse = motion.inverted().toMatrix();
            for (const QPointF& point : points) {
                const float mappedX = forward(0, 0) * point.x()
                    + forward(0, 1) * point.y() + forward(0, 2);
                const float mappedY = forward(1, 0) * point.x()
                    + forward(1, 1) * point.y() + forward(1, 2);
                const float restoredX = inverse(0, 0) * mappedX
                    + inverse(0, 1) * mappedY + inverse(0, 2);
                const float restoredY = inverse(1, 0) * mappedX
                    + inverse(1, 1) * mappedY + inverse(1, 2);

                EXPECT_NEAR(restoredX, point.x(), 2.0e-4f)
                    << "rotation=" << rotation << " scale=" << scale;
                EXPECT_NEAR(restoredY, point.y(), 2.0e-4f)
                    << "rotation=" << rotation << " scale=" << scale;
            }
        }
    }
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

TEST(VideoStabilizerContractTest, SingleFrameHistoryReturnsEachCurrentFrame)
{
    ArtifactCore::LiveStabilizer stabilizer;
    stabilizer.setMaxHistorySize(1);

    QImage first(2, 1, QImage::Format_ARGB32);
    first.setPixel(0, 0, qRgba(10, 20, 30, 40));
    first.setPixel(1, 0, qRgba(50, 60, 70, 80));
    QImage second(2, 1, QImage::Format_ARGB32);
    second.setPixel(0, 0, qRgba(90, 100, 110, 120));
    second.setPixel(1, 0, qRgba(130, 140, 150, 160));

    EXPECT_EQ(stabilizer.processFrame(first), first);
    EXPECT_EQ(stabilizer.processFrame(second), second);
}

TEST(VideoStabilizerContractTest, LiveStabilizerStoresParameterSnapshotByValue)
{
    ArtifactCore::LiveStabilizer stabilizer;
    ArtifactCore::StabilizerParams requested;
    requested.smoothingWindowSize = 7;
    requested.stabilizeTranslation = false;
    requested.featureParams.maxFeatures = 42;
    requested.outputSize = QSize(640, 360);

    stabilizer.setParams(requested);
    requested.smoothingWindowSize = 99;
    requested.stabilizeTranslation = true;
    requested.featureParams.maxFeatures = 1000;
    requested.outputSize = QSize(1, 1);

    const auto& stored = stabilizer.params();
    EXPECT_EQ(stored.smoothingWindowSize, 7);
    EXPECT_FALSE(stored.stabilizeTranslation);
    EXPECT_EQ(stored.featureParams.maxFeatures, 42);
    EXPECT_EQ(stored.outputSize, QSize(640, 360));
}

TEST(VideoStabilizerContractTest,
     BatchStabilizerProcessesSingleImageAndWritesPixelPreservingOutput)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString inputPath = temporaryDirectory.filePath("source.png");
    const QString outputPath = temporaryDirectory.filePath("nested/result.png");
    QImage source(4, 3, QImage::Format_ARGB32);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixel(x, y, qRgba(20 + x * 30, 40 + y * 40,
                                       15 + x * 7 + y * 9, 255));
        }
    }
    ASSERT_TRUE(source.save(inputPath));

    ArtifactCore::BatchStabilizer stabilizer;
    stabilizer.setInputFile(inputPath);
    stabilizer.setOutputFile(outputPath);
    ArtifactCore::StabilizerParams params;
    params.outputSize = source.size();
    stabilizer.setParams(params);

    ASSERT_TRUE(stabilizer.process());
    const QImage output(outputPath);
    ASSERT_FALSE(output.isNull());
    ASSERT_EQ(output.size(), source.size());
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            EXPECT_EQ(output.pixel(x, y), source.pixel(x, y))
                << "pixel (" << x << ", " << y << ")";
        }
    }
}
