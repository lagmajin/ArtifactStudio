#include <gtest/gtest.h>

#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QTemporaryDir>

#include <limits>

import Image.DepthMap;

using ArtifactCore::DepthMap;

TEST(DepthMapContractTest, ResizeFillsEverySampleWithClampedValue)
{
    DepthMap map;

    map.resize(3, 2, 1.5f);

    ASSERT_FALSE(map.isEmpty());
    ASSERT_EQ(map.width(), 3);
    ASSERT_EQ(map.height(), 2);
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            EXPECT_FLOAT_EQ(map.value(x, y), 1.0f);
        }
    }
}

TEST(DepthMapContractTest, ResizeClampsNegativeInitialValueToZero)
{
    DepthMap map;

    map.resize(2, 2, -0.5f);

    ASSERT_FALSE(map.isEmpty());
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            EXPECT_FLOAT_EQ(map.value(x, y), 0.0f);
        }
    }
}

TEST(DepthMapContractTest, ResizeUsesZeroForDefaultInitialValue)
{
    DepthMap map;

    map.resize(2, 2);

    ASSERT_FALSE(map.isEmpty());
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            EXPECT_FLOAT_EQ(map.value(x, y), 0.0f);
        }
    }
}

TEST(DepthMapContractTest, ResizeReplacesShapeAndRefillsEverySample)
{
    DepthMap map;
    map.resize(3, 2, 0.8f);
    map.setValue(2, 1, 0.1f);

    map.resize(2, 1, 0.25f);

    ASSERT_EQ(map.width(), 2);
    ASSERT_EQ(map.height(), 1);
    EXPECT_FLOAT_EQ(map.value(0, 0), 0.25f);
    EXPECT_FLOAT_EQ(map.value(1, 0), 0.25f);
}

TEST(DepthMapContractTest, InvalidResizeClearsDimensionsAndSamples)
{
    DepthMap map;
    map.resize(2, 2, 0.75f);

    map.resize(0, 2, 0.25f);

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(DepthMapContractTest, NegativeWidthResizeClearsExistingSamples)
{
    DepthMap map;
    map.resize(2, 2, 0.75f);

    map.resize(-1, 2, 0.25f);

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(DepthMapContractTest, NegativeHeightResizeClearsExistingSamples)
{
    DepthMap map;
    map.resize(2, 2, 0.75f);

    map.resize(2, -1, 0.25f);

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(DepthMapContractTest, ZeroHeightResizeClearsExistingSamples)
{
    DepthMap map;
    map.resize(2, 2, 0.75f);

    map.resize(2, 0, 0.25f);

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(DepthMapContractTest, SetValueClampsAndIgnoresOutOfBoundsCoordinates)
{
    DepthMap map;
    map.resize(2, 1, 0.4f);

    map.setValue(0, 0, -0.5f);
    map.setValue(1, 0, 1.5f);
    map.setValue(-1, 0, 0.8f);
    map.setValue(2, 0, 0.8f);
    map.setValue(0, -1, 0.8f);
    map.setValue(0, 1, 0.8f);

    EXPECT_FLOAT_EQ(map.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.value(1, 0), 1.0f);
}

TEST(DepthMapContractTest, ResizeAndSetValueClampInfiniteDepthSamples)
{
    const float infinity = std::numeric_limits<float>::infinity();
    DepthMap map;
    map.resize(2, 1, infinity);
    EXPECT_FLOAT_EQ(map.value(0, 0), 1.0f);

    map.setValue(0, 0, -infinity);
    map.setValue(1, 0, infinity);

    EXPECT_FLOAT_EQ(map.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.value(1, 0), 1.0f);
}

TEST(DepthMapContractTest, SetValueOnEmptyMapIsNoOp)
{
    DepthMap map;

    map.setValue(0, 0, 0.75f);
    map.setValue(-1, -1, 1.0f);

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(DepthMapContractTest, CopyMutationDoesNotChangeOriginalSamples)
{
    DepthMap original;
    original.resize(2, 2, 0.25f);
    original.setValue(1, 1, 0.75f);

    DepthMap copy = original;
    copy.setValue(0, 0, 1.0f);
    copy.setValue(1, 1, 0.0f);

    EXPECT_FLOAT_EQ(original.value(0, 0), 0.25f);
    EXPECT_FLOAT_EQ(original.value(1, 1), 0.75f);
    EXPECT_FLOAT_EQ(copy.value(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(copy.value(1, 1), 0.0f);
}

TEST(DepthMapContractTest, CopyAssignmentPreservesShapeAndIndependentStorage)
{
    DepthMap original;
    original.resize(3, 1, 0.4f);
    original.setValue(2, 0, 0.9f);

    DepthMap assigned;
    assigned.resize(1, 2, 0.1f);
    assigned = original;
    assigned.resize(1, 1, 0.2f);

    EXPECT_EQ(original.width(), 3);
    EXPECT_EQ(original.height(), 1);
    EXPECT_FLOAT_EQ(original.value(0, 0), 0.4f);
    EXPECT_FLOAT_EQ(original.value(2, 0), 0.9f);
    EXPECT_EQ(assigned.width(), 1);
    EXPECT_EQ(assigned.height(), 1);
    EXPECT_FLOAT_EQ(assigned.value(0, 0), 0.2f);
}

TEST(DepthMapContractTest, BilinearSamplingInterpolatesBothAxes)
{
    DepthMap map;
    map.resize(2, 2, 0.0f);
    map.setValue(1, 0, 0.8f);
    map.setValue(0, 1, 0.4f);
    map.setValue(1, 1, 1.0f);

    EXPECT_FLOAT_EQ(map.sampleBilinear(0.25f, 0.5f), 0.375f);
}

TEST(DepthMapContractTest, BilinearSamplingMatchesNonSquareThreeByTwoGrid)
{
    DepthMap map;
    map.resize(3, 2, 0.0f);
    map.setValue(1, 0, 0.25f);
    map.setValue(2, 0, 0.75f);
    map.setValue(0, 1, 0.5f);
    map.setValue(1, 1, 0.75f);
    map.setValue(2, 1, 1.0f);

    EXPECT_FLOAT_EQ(map.sampleBilinear(0.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.25f, 0.5f), 0.375f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.75f, 0.25f), 0.59375f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(1.0f, 1.0f), 1.0f);
}

TEST(DepthMapContractTest, BilinearSamplingReproducesAffinePlaneAcrossDenseGrid)
{
    constexpr int width = 5;
    constexpr int height = 4;
    DepthMap map;
    map.resize(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(width - 1);
            const float v = static_cast<float>(y) / static_cast<float>(height - 1);
            map.setValue(x, y, 0.1f + 0.3f * u + 0.5f * v);
        }
    }

    for (int yStep = 0; yStep <= 20; ++yStep) {
        for (int xStep = 0; xStep <= 20; ++xStep) {
            const float u = static_cast<float>(xStep) / 20.0f;
            const float v = static_cast<float>(yStep) / 20.0f;
            EXPECT_NEAR(map.sampleBilinear(u, v), 0.1f + 0.3f * u + 0.5f * v,
                        2e-6f);
        }
    }
}

TEST(DepthMapContractTest, ClearIsIdempotentAndResetsDimensions)
{
    DepthMap map;
    map.resize(3, 2, 0.5f);

    map.clear();
    map.clear();

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(DepthMapContractTest, BilinearSamplingClampsCoordinatesToEdges)
{
    DepthMap map;
    map.resize(2, 2, 0.0f);
    map.setValue(1, 0, 0.4f);
    map.setValue(0, 1, 0.7f);
    map.setValue(1, 1, 1.0f);

    EXPECT_FLOAT_EQ(map.sampleBilinear(-2.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(3.0f, 1.0f), 1.0f);
}

TEST(DepthMapContractTest, BilinearSamplingClampsInfiniteCoordinatesToEdges)
{
    DepthMap map;
    map.resize(2, 2, 0.0f);
    map.setValue(1, 0, 0.4f);
    map.setValue(0, 1, 0.7f);
    map.setValue(1, 1, 1.0f);
    const float infinity = std::numeric_limits<float>::infinity();

    EXPECT_FLOAT_EQ(map.sampleBilinear(-infinity, infinity), 0.7f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(infinity, -infinity), 0.4f);
}

TEST(DepthMapContractTest, BilinearSamplingOfSinglePixelIsCoordinateInvariant)
{
    DepthMap map;
    map.resize(1, 1, 0.625f);

    EXPECT_FLOAT_EQ(map.sampleBilinear(0.0f, 0.0f), 0.625f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.37f, 0.81f), 0.625f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(1.0f, 1.0f), 0.625f);
}

TEST(DepthMapContractTest, SingleRowSamplingInterpolatesXAndClampsY)
{
    DepthMap map;
    map.resize(2, 1, 0.0f);
    map.setValue(1, 0, 1.0f);

    EXPECT_FLOAT_EQ(map.sampleBilinear(0.25f, -5.0f), 0.25f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.25f, 5.0f), 0.25f);
}

TEST(DepthMapContractTest, SingleColumnSamplingInterpolatesYAndClampsX)
{
    DepthMap map;
    map.resize(1, 2, 0.0f);
    map.setValue(0, 1, 1.0f);

    EXPECT_FLOAT_EQ(map.sampleBilinear(-5.0f, 0.25f), 0.25f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(5.0f, 0.25f), 0.25f);
}

TEST(DepthMapContractTest, LoadConvertsGrayscalePixelsToNormalizedSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QImage source(2, 1, QImage::Format_Grayscale8);
    source.scanLine(0)[0] = 0;
    source.scanLine(0)[1] = 128;
    const QString path = directory.filePath(QStringLiteral("depth.png"));
    ASSERT_TRUE(source.save(path));

    DepthMap map;
    ASSERT_TRUE(map.load(path));

    ASSERT_EQ(map.width(), 2);
    ASSERT_EQ(map.height(), 1);
    EXPECT_FLOAT_EQ(map.value(0, 0), 0.0f);
    EXPECT_NEAR(map.value(1, 0), 128.0f / 255.0f, 1e-6f);
}

TEST(DepthMapContractTest, LoadConvertsRgbInputUsingQtGrayscaleReference)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QImage source(3, 1, QImage::Format_RGB32);
    source.setPixel(0, 0, qRgb(255, 0, 0));
    source.setPixel(1, 0, qRgb(0, 255, 0));
    source.setPixel(2, 0, qRgb(0, 0, 255));
    const QImage expectedGray = source.convertToFormat(QImage::Format_Grayscale8);
    const QString path = directory.filePath(QStringLiteral("rgb-depth.png"));
    ASSERT_TRUE(source.save(path));

    DepthMap map;
    ASSERT_TRUE(map.load(path));

    ASSERT_EQ(map.width(), 3);
    ASSERT_EQ(map.height(), 1);
    for (int x = 0; x < 3; ++x) {
        EXPECT_NEAR(map.value(x, 0), expectedGray.constScanLine(0)[x] / 255.0f,
                    1e-6f);
    }
}

TEST(DepthMapContractTest, SuccessfulLoadReplacesExistingDimensionsAndSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QImage source(1, 2, QImage::Format_Grayscale8);
    source.scanLine(0)[0] = 64;
    source.scanLine(1)[0] = 192;
    const QString path = directory.filePath(QStringLiteral("replacement.png"));
    ASSERT_TRUE(source.save(path));

    DepthMap map;
    map.resize(3, 2, 0.9f);
    ASSERT_TRUE(map.load(path));

    ASSERT_EQ(map.width(), 1);
    ASSERT_EQ(map.height(), 2);
    EXPECT_NEAR(map.value(0, 0), 64.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(map.value(0, 1), 192.0f / 255.0f, 1e-6f);
}

TEST(DepthMapContractTest, FailedLoadClearsPreviouslyLoadedSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    DepthMap map;
    map.resize(2, 1, 0.6f);

    EXPECT_FALSE(map.load(directory.filePath(QStringLiteral("missing.png"))));

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.0f, 0.0f), 0.0f);
}

TEST(DepthMapContractTest, CorruptImageLoadFailsAndClearsPreviouslyLoadedSamples)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("corrupt.png"));
    QFile corruptFile(path);
    ASSERT_TRUE(corruptFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(corruptFile.write("not an image", 12), 12);
    corruptFile.close();

    DepthMap map;
    map.resize(2, 2, 0.6f);
    EXPECT_FALSE(map.load(path));

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
}
