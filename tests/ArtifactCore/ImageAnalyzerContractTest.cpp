#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

import Analyze.Histogram;
import Graphics.SurfaceColorContract;
import Image.ImageSurfaceView;

using namespace ArtifactCore;

TEST(ImageAnalyzerContractTest, SamplesFloatBgraViewWithPaddedRowsAsLogicalRgba)
{
    const std::array<float, 12> pixels = {
        0.1f, 0.2f, 0.3f, 0.4f, 99.0f, 99.0f, 99.0f, 99.0f,
        0.5f, 0.6f, 0.7f, 0.8f,
    };
    auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    const ImageSurfaceView image{pixels.data(), 1, 2, 8u * sizeof(float),
                                 SurfacePrecision::Float32, descriptor};
    ImagePixelSample sample;

    ASSERT_TRUE(ImageAnalyzer::samplePixel(image, 0, 1, sample));
    EXPECT_EQ(sample.x, 0);
    EXPECT_EQ(sample.y, 1);
    EXPECT_FLOAT_EQ(sample.rgba[0], 0.7f);
    EXPECT_FLOAT_EQ(sample.rgba[1], 0.6f);
    EXPECT_FLOAT_EQ(sample.rgba[2], 0.5f);
    EXPECT_FLOAT_EQ(sample.rgba[3], 0.8f);
    EXPECT_FALSE(ImageAnalyzer::samplePixel(image, 1, 1, sample));
}

TEST(ImageAnalyzerContractTest, SamplesByteBgraViewNormalizedToZeroOne)
{
    const std::array<std::uint8_t, 4> pixels = {10, 20, 30, 40};
    auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    const ImageByteSurfaceView image{pixels.data(), 1, 1, pixels.size(), descriptor};
    ImagePixelSample sample;

    ASSERT_TRUE(ImageAnalyzer::samplePixel(image, 0, 0, sample));
    EXPECT_NEAR(sample.rgba[0], 30.0f / 255.0f, 1e-7f);
    EXPECT_NEAR(sample.rgba[1], 20.0f / 255.0f, 1e-7f);
    EXPECT_NEAR(sample.rgba[2], 10.0f / 255.0f, 1e-7f);
    EXPECT_NEAR(sample.rgba[3], 40.0f / 255.0f, 1e-7f);
}

TEST(ImageAnalyzerContractTest, SamplesFloat16ValuesFromHalfStorage)
{
    const std::array<std::uint16_t, 4> pixels = {
        0x3800u, 0x3400u, 0x3000u, 0x3800u};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba16Float();
    const ImageSurfaceView image{pixels.data(), 1, 1, 4u * sizeof(std::uint16_t),
                                 SurfacePrecision::Float16, descriptor};
    ImagePixelSample sample;

    ASSERT_TRUE(ImageAnalyzer::samplePixel(image, 0, 0, sample));
    EXPECT_FLOAT_EQ(sample.rgba[0], 0.5f);
    EXPECT_FLOAT_EQ(sample.rgba[1], 0.25f);
    EXPECT_FLOAT_EQ(sample.rgba[2], 0.125f);
    EXPECT_FLOAT_EQ(sample.rgba[3], 0.5f);
}

TEST(ImageAnalyzerContractTest, RejectsOutOfRangeCoordinatesAndIncompatibleViews)
{
    const std::array<float, 4> floatPixels = {0.1f, 0.2f, 0.3f, 0.4f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    const ImageSurfaceView validFloat{floatPixels.data(), 1, 1,
                                      4u * sizeof(float),
                                      SurfacePrecision::Float32, descriptor};
    ImagePixelSample sample;
    EXPECT_FALSE(ImageAnalyzer::samplePixel(validFloat, -1, 0, sample));
    EXPECT_FALSE(ImageAnalyzer::samplePixel(validFloat, 1, 0, sample));
    EXPECT_FALSE(ImageAnalyzer::samplePixel(validFloat, 0, 1, sample));

    auto wrongPrecision = validFloat;
    wrongPrecision.precision = SurfacePrecision::Float16;
    EXPECT_FALSE(ImageAnalyzer::samplePixel(wrongPrecision, 0, 0, sample));
    auto shortStride = validFloat;
    shortStride.rowStride = 3u * sizeof(float);
    EXPECT_FALSE(ImageAnalyzer::samplePixel(shortStride, 0, 0, sample));
    auto unsupportedOrder = validFloat;
    unsupportedOrder.descriptor.channelOrder = SurfaceChannelOrder::Gray;
    EXPECT_FALSE(ImageAnalyzer::samplePixel(unsupportedOrder, 0, 0, sample));

    const std::array<std::uint8_t, 4> bytePixels = {1, 2, 3, 4};
    const ImageByteSurfaceView wrongByteStorage{
        bytePixels.data(), 1, 1, bytePixels.size(), descriptor};
    EXPECT_FALSE(ImageAnalyzer::samplePixel(wrongByteStorage, 0, 0, sample));
}

TEST(ImageAnalyzerContractTest, HistogramsHonorRowsAndNormalizeSampleCounts)
{
    const std::array<float, 16> pixels = {
        0.0f, 0.5f, 1.0f, 1.0f, 99.0f, 99.0f, 99.0f, 99.0f,
        1.0f, 0.5f, 0.0f, 0.0f, 99.0f, 99.0f, 99.0f, 99.0f,
    };
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    const ImageSurfaceView image{pixels.data(), 1, 2, 8u * sizeof(float),
                                 SurfacePrecision::Float32, descriptor};
    ImageStatistics statistics;

    ASSERT_TRUE(ImageAnalyzer::analyze(image, statistics));
    EXPECT_EQ(statistics.red.totalPixels, 2);
    EXPECT_EQ(statistics.red.rawHistogram[0], 1);
    EXPECT_EQ(statistics.red.rawHistogram[255], 1);
    EXPECT_FLOAT_EQ(statistics.red.histogram[0], 0.5f);
    EXPECT_FLOAT_EQ(statistics.red.histogram[255], 0.5f);
    EXPECT_EQ(statistics.green.rawHistogram[127], 2);
    EXPECT_EQ(statistics.alpha.rawHistogram[0], 1);
    EXPECT_EQ(statistics.alpha.rawHistogram[255], 1);
}

TEST(ImageAnalyzerContractTest, AnalyzesPaddedByteViewHistograms)
{
    const std::array<std::uint8_t, 16> pixels = {
        0, 127, 255, 255, 99, 99, 99, 99,
        255, 127, 0, 0, 99, 99, 99, 99,
    };
    const auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
    const ImageByteSurfaceView image{pixels.data(), 1, 2, 8u, descriptor};
    ImageStatistics statistics;

    ASSERT_TRUE(ImageAnalyzer::analyze(image, statistics));
    EXPECT_EQ(statistics.red.totalPixels, 2);
    EXPECT_EQ(statistics.red.rawHistogram[0], 1);
    EXPECT_EQ(statistics.red.rawHistogram[255], 1);
    EXPECT_EQ(statistics.green.rawHistogram[127], 2);
    EXPECT_EQ(statistics.blue.rawHistogram[0], 1);
    EXPECT_EQ(statistics.blue.rawHistogram[255], 1);
    EXPECT_EQ(statistics.alpha.rawHistogram[0], 1);
    EXPECT_EQ(statistics.alpha.rawHistogram[255], 1);
}

TEST(ImageAnalyzerContractTest, AnalysisRejectsNonFiniteFloatSamples)
{
    const std::array<float, 4> pixels = {
        std::numeric_limits<float>::quiet_NaN(), 0.2f, 0.3f, 1.0f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    const ImageSurfaceView image{pixels.data(), 1, 1, 4u * sizeof(float),
                                 SurfacePrecision::Float32, descriptor};
    ImageStatistics statistics;

    EXPECT_FALSE(ImageAnalyzer::analyze(image, statistics));
}

TEST(ImageAnalyzerContractTest, FailedAnalysisLeavesCallerStatisticsUnchanged)
{
    const std::array<float, 4> pixels = {
        0.1f, std::numeric_limits<float>::infinity(), 0.3f, 1.0f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    const ImageSurfaceView image{pixels.data(), 1, 1, 4u * sizeof(float),
                                 SurfacePrecision::Float32, descriptor};
    ImageStatistics statistics{};
    statistics.red.mean = 0.42f;
    statistics.red.totalPixels = 7;
    statistics.red.rawHistogram[17] = 5;
    statistics.alpha.percentile95 = 0.91f;

    EXPECT_FALSE(ImageAnalyzer::analyze(image, statistics));
    EXPECT_FLOAT_EQ(statistics.red.mean, 0.42f);
    EXPECT_EQ(statistics.red.totalPixels, 7);
    EXPECT_EQ(statistics.red.rawHistogram[17], 5);
    EXPECT_FLOAT_EQ(statistics.alpha.percentile95, 0.91f);
}

TEST(ImageAnalyzerContractTest, LegacyFloatBufferAnalysisClampsChannelsAndExposesPercentiles)
{
    const std::array<float, 8> pixels = {
        -0.5f, 0.25f, 1.5f, 0.0f,
        1.5f, 0.75f, -0.5f, 1.0f,
    };
    const ImageStatistics statistics = ImageAnalyzer::analyze(pixels.data(), 2, 1);

    EXPECT_EQ(statistics.red.totalPixels, 2);
    EXPECT_FLOAT_EQ(statistics.red.min, 0.0f);
    EXPECT_FLOAT_EQ(statistics.red.max, 1.0f);
    EXPECT_EQ(statistics.red.rawHistogram[0], 1);
    EXPECT_EQ(statistics.red.rawHistogram[255], 1);
    EXPECT_NEAR(statistics.red.histogram[0], 0.5f, 1e-7f);
    EXPECT_NEAR(statistics.red.histogram[255], 0.5f, 1e-7f);
    EXPECT_FLOAT_EQ(ImageAnalyzer::percentile(statistics.red, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(ImageAnalyzer::percentile(statistics.red, 1.0f), 1.0f);
}

TEST(ImageAnalyzerContractTest, AutoExposureAndWhiteBalanceUseDocumentedTargets)
{
    const std::array<float, 8> middleGray = {
        0.18f, 0.18f, 0.18f, 1.0f,
        0.18f, 0.18f, 0.18f, 0.5f,
    };
    EXPECT_NEAR(ImageAnalyzer::autoExposureEV(middleGray.data(), 2, 1), 0.0f,
                1e-6f);

    const std::array<float, 8> color = {
        0.4f, 0.2f, 0.1f, 1.0f,
        0.4f, 0.2f, 0.1f, 1.0f,
    };
    const auto multipliers = ImageAnalyzer::autoWhiteBalance(color.data(), 2, 1);
    EXPECT_NEAR(multipliers[0], 0.5f, 1e-6f);
    EXPECT_FLOAT_EQ(multipliers[1], 1.0f);
    EXPECT_NEAR(multipliers[2], 2.0f, 1e-6f);
}

TEST(ImageAnalyzerContractTest, AutoAdjustmentsStayFiniteForBlackPixels)
{
    const std::array<float, 4> black = {0.0f, 0.0f, 0.0f, 0.0f};

    const float exposure = ImageAnalyzer::autoExposureEV(black.data(), 1, 1);
    const auto whiteBalance = ImageAnalyzer::autoWhiteBalance(black.data(), 1, 1);

    EXPECT_TRUE(std::isfinite(exposure));
    EXPECT_NEAR(exposure, std::log2(0.18f / 0.0001f), 1e-5f);
    for (const float multiplier : whiteBalance) {
        EXPECT_TRUE(std::isfinite(multiplier));
        EXPECT_FLOAT_EQ(multiplier, 1.0f);
    }
}

TEST(ImageAnalyzerContractTest, SpatialFrequencyReportsRequiredCountAndNormalizedDc)
{
    const std::array<float, 16> pixels = {
        0.5f, 0.0f, 0.0f, 1.0f,  0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.0f, 0.0f, 1.0f,  0.5f, 0.0f, 0.0f, 1.0f,
    };
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    const ImageSurfaceView image{pixels.data(), 2, 2, 8u * sizeof(float),
                                 SurfacePrecision::Float32, descriptor};
    std::array<float, 4> magnitudes{};
    std::size_t count = 0;

    ASSERT_TRUE(ImageAnalyzer::analyzeSpatialFrequency(
        image, ImageSpectrumChannel::Red, magnitudes.data(), magnitudes.size(), count));
    EXPECT_EQ(count, 4u);
    EXPECT_NEAR(magnitudes[0], 0.5f, 1e-6f);
    EXPECT_NEAR(magnitudes[1], 0.0f, 1e-6f);
    EXPECT_NEAR(magnitudes[2], 0.0f, 1e-6f);
    EXPECT_NEAR(magnitudes[3], 0.0f, 1e-6f);

    magnitudes.fill(-1.0f);
    count = 0;
    EXPECT_FALSE(ImageAnalyzer::analyzeSpatialFrequency(
        image, ImageSpectrumChannel::Red, magnitudes.data(), 3u, count));
    EXPECT_EQ(count, 4u);
    for (const float magnitude : magnitudes) {
        EXPECT_FLOAT_EQ(magnitude, -1.0f);
    }
}

TEST(ImageAnalyzerContractTest, SpatialFrequencySelectsChannelsAndRejectsUnknownChannel)
{
    const std::array<float, 4> pixels = {0.2f, 0.4f, 0.6f, 0.8f};
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    const ImageSurfaceView image{pixels.data(), 1, 1, 4u * sizeof(float),
                                 SurfacePrecision::Float32, descriptor};
    const struct ChannelCase {
        ImageSpectrumChannel channel;
        float expected;
    } cases[] = {
        {ImageSpectrumChannel::Alpha, 0.8f},
        {ImageSpectrumChannel::Rec709WeightedRgb, 0.37192f},
    };
    float magnitude = 0.0f;
    std::size_t count = 0;
    for (const auto& testCase : cases) {
        ASSERT_TRUE(ImageAnalyzer::analyzeSpatialFrequency(
            image, testCase.channel, &magnitude, 1u, count));
        EXPECT_EQ(count, 1u);
        EXPECT_NEAR(magnitude, testCase.expected, 1e-6f);
    }

    magnitude = -1.0f;
    EXPECT_FALSE(ImageAnalyzer::analyzeSpatialFrequency(
        image, static_cast<ImageSpectrumChannel>(255), &magnitude, 1u, count));
    EXPECT_EQ(count, 0u);
    EXPECT_FLOAT_EQ(magnitude, -1.0f);
}

TEST(ImageAnalyzerContractTest, SpatialFrequencyAcceptsByteSurfaceViews)
{
    const std::array<std::uint8_t, 4> pixels = {255, 128, 0, 255};
    const auto descriptor = SurfaceColorDescriptor::encodedSrgbRgba8Straight();
    const ImageByteSurfaceView image{pixels.data(), 1, 1, pixels.size(), descriptor};
    float magnitude = 0.0f;
    std::size_t count = 0;

    ASSERT_TRUE(ImageAnalyzer::analyzeSpatialFrequency(
        image, ImageSpectrumChannel::Green, &magnitude, 1u, count));
    EXPECT_EQ(count, 1u);
    EXPECT_NEAR(magnitude, 128.0f / 255.0f, 1e-6f);
}
