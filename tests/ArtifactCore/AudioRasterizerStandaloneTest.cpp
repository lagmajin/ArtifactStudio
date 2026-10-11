#include <gtest/gtest.h>

#include <QVector>

#include <limits>

import Audio.Rasterizer;

using namespace ArtifactCore;

TEST(AudioRasterizerStandaloneTest, RasterizesEachBinToItsSampleMinimumAndMaximum)
{
    const QVector<float> samples{0.1f, -0.2f, 0.3f, -0.4f,
                                 0.5f, -0.6f, 0.7f, -0.8f};

    const auto result = AudioRasterizer::rasterize(samples, 4);

    ASSERT_EQ(result.binCount(), 4);
    EXPECT_EQ(result.minValues, (QVector<float>{-0.2f, -0.4f, -0.6f, -0.8f}));
    EXPECT_EQ(result.maxValues, (QVector<float>{0.1f, 0.3f, 0.5f, 0.7f}));
    EXPECT_FALSE(result.isEmpty());
}

TEST(AudioRasterizerStandaloneTest, UnevenBinsCoverTheEntireSampleRange)
{
    const QVector<float> samples{1.0f, 2.0f, 3.0f, 4.0f, -9.0f};

    const auto result = AudioRasterizer::rasterize(samples, 2);

    ASSERT_EQ(result.binCount(), 2);
    EXPECT_FLOAT_EQ(result.minValues[0], 1.0f);
    EXPECT_FLOAT_EQ(result.maxValues[0], 2.0f);
    EXPECT_FLOAT_EQ(result.minValues[1], -9.0f);
    EXPECT_FLOAT_EQ(result.maxValues[1], 4.0f);
}

TEST(AudioRasterizerStandaloneTest, BinCountAboveSampleCountProducesOneBinPerSample)
{
    const auto result = AudioRasterizer::rasterize(QVector<float>{-1.0f, 0.25f}, 10);

    ASSERT_EQ(result.binCount(), 2);
    EXPECT_EQ(result.minValues, (QVector<float>{-1.0f, 0.25f}));
    EXPECT_EQ(result.maxValues, (QVector<float>{-1.0f, 0.25f}));
}

TEST(AudioRasterizerStandaloneTest, EmptyInputOrNonPositiveBinCountReturnsEmptyData)
{
    EXPECT_TRUE(AudioRasterizer::rasterize({}, 4).isEmpty());
    EXPECT_TRUE(AudioRasterizer::rasterize(QVector<float>{1.0f}, 0).isEmpty());
    EXPECT_TRUE(AudioRasterizer::rasterize(QVector<float>{1.0f}, -2).isEmpty());
}

TEST(AudioRasterizerStandaloneTest, NonFiniteSamplesAreIgnoredWithinEachBin)
{
    const QVector<float> samples{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.5f};

    const auto result = AudioRasterizer::rasterize(samples, 2);

    ASSERT_EQ(result.binCount(), 2);
    EXPECT_FLOAT_EQ(result.minValues[0], 0.0f);
    EXPECT_FLOAT_EQ(result.maxValues[0], 0.0f);
    EXPECT_FLOAT_EQ(result.minValues[1], 0.5f);
    EXPECT_FLOAT_EQ(result.maxValues[1], 0.5f);
}

TEST(AudioRasterizerStandaloneTest, InterleavedStereoBinsIncludeBothChannels)
{
    const QVector<float> interleaved{0.1f, -0.9f, 0.4f, 0.2f,
                                     -0.3f, 0.8f, 0.7f, -0.5f};

    const auto result = AudioRasterizer::rasterizeInterleaved(interleaved, 2, 2);

    ASSERT_EQ(result.binCount(), 2);
    EXPECT_FLOAT_EQ(result.minValues[0], -0.9f);
    EXPECT_FLOAT_EQ(result.maxValues[0], 0.4f);
    EXPECT_FLOAT_EQ(result.minValues[1], -0.5f);
    EXPECT_FLOAT_EQ(result.maxValues[1], 0.8f);
}

TEST(AudioRasterizerStandaloneTest, InterleavedUnevenBinsCoverAllCompleteFrames)
{
    const QVector<float> interleaved{-0.1f, 0.2f, 0.3f, -0.4f,
                                     0.5f, 0.6f, -0.9f, 0.8f,
                                     0.7f, -0.2f};

    const auto result = AudioRasterizer::rasterizeInterleaved(interleaved, 2, 2);

    ASSERT_EQ(result.binCount(), 2);
    EXPECT_FLOAT_EQ(result.minValues[0], -0.4f);
    EXPECT_FLOAT_EQ(result.maxValues[0], 0.3f);
    EXPECT_FLOAT_EQ(result.minValues[1], -0.9f);
    EXPECT_FLOAT_EQ(result.maxValues[1], 0.8f);
}

TEST(AudioRasterizerStandaloneTest, InvalidChannelOrBinCountReturnsEmptyData)
{
    const QVector<float> samples{0.1f, -0.2f, 0.3f, -0.4f};
    EXPECT_TRUE(AudioRasterizer::rasterizeInterleaved(samples, 0, 2).isEmpty());
    EXPECT_TRUE(AudioRasterizer::rasterizeInterleaved(samples, -1, 2).isEmpty());
    EXPECT_TRUE(AudioRasterizer::rasterizeInterleaved(samples, 2, 0).isEmpty());
    EXPECT_TRUE(AudioRasterizer::rasterizeInterleaved(samples, 2, -1).isEmpty());
}

TEST(AudioRasterizerStandaloneTest, IncompleteTrailingInterleavedFrameIsIgnored)
{
    const QVector<float> samples{-0.25f, 0.5f, 0.75f};

    const auto result = AudioRasterizer::rasterizeInterleaved(samples, 2, 4);

    ASSERT_EQ(result.binCount(), 1);
    EXPECT_FLOAT_EQ(result.minValues[0], -0.25f);
    EXPECT_FLOAT_EQ(result.maxValues[0], 0.5f);
}

TEST(AudioRasterizerStandaloneTest, InputShorterThanOneInterleavedFrameReturnsEmpty)
{
    EXPECT_TRUE(AudioRasterizer::rasterizeInterleaved(QVector<float>{0.5f}, 2, 1).isEmpty());
}

