#include <gtest/gtest.h>

#include <limits>

#include <opencv2/core.hpp>

import ImageProcessing.ChromaSpread;

using namespace ArtifactCore;

namespace {

cv::Mat makeTaggedRow()
{
    cv::Mat image(1, 5, CV_32FC4);
    for (int x = 0; x < image.cols; ++x) {
        image.at<cv::Vec4f>(0, x) = cv::Vec4f(
            static_cast<float>(x + 10),
            static_cast<float>(x + 20),
            static_cast<float>(x + 30),
            static_cast<float>(x + 40));
    }
    return image;
}

}

TEST(ChromaSpreadEffectContractTest, UnitScalesAndZeroShiftPreserveAllChannels)
{
    cv::Mat image = makeTaggedRow();
    const cv::Mat original = image.clone();
    ChromaSpreadSettings settings;
    settings.dispersionSteps = 1;
    ChromaSpread effect;
    effect.processMat(&image, settings);

    for (int x = 0; x < image.cols; ++x) {
        const cv::Vec4f actual = image.at<cv::Vec4f>(0, x);
        const cv::Vec4f expected = original.at<cv::Vec4f>(0, x);
        for (int channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(actual[channel], expected[channel])
                << "x=" << x << ", channel=" << channel;
        }
    }
}

TEST(ChromaSpreadEffectContractTest, HorizontalShiftMovesRedAndBlueInOppositeDirections)
{
    cv::Mat image = makeTaggedRow();
    ChromaSpreadSettings settings;
    settings.shiftAmount = 1.0f;
    settings.shiftAngle = 0.0f;
    settings.dispersionSteps = 1;
    ChromaSpread effect;
    effect.processMat(&image, settings);

    const cv::Vec4f center = image.at<cv::Vec4f>(0, 2);
    EXPECT_FLOAT_EQ(center[0], 11.0f);
    EXPECT_FLOAT_EQ(center[1], 22.0f);
    EXPECT_FLOAT_EQ(center[2], 33.0f);
    EXPECT_FLOAT_EQ(center[3], 42.0f);
}

TEST(ChromaSpreadEffectContractTest, BorderRemappingReplicatesEdgeSamples)
{
    cv::Mat image = makeTaggedRow();
    ChromaSpreadSettings settings;
    settings.shiftAmount = 100.0f;
    settings.dispersionSteps = 1;
    ChromaSpread effect;
    effect.processMat(&image, settings);

    const cv::Vec4f left = image.at<cv::Vec4f>(0, 0);
    const cv::Vec4f right = image.at<cv::Vec4f>(0, 4);
    EXPECT_FLOAT_EQ(left[2], 34.0f);
    EXPECT_FLOAT_EQ(left[0], 10.0f);
    EXPECT_FLOAT_EQ(right[2], 34.0f);
    EXPECT_FLOAT_EQ(right[0], 10.0f);
    EXPECT_FLOAT_EQ(left[3], 40.0f);
    EXPECT_FLOAT_EQ(right[3], 44.0f);
}

TEST(ChromaSpreadEffectContractTest, NonFiniteControlsUseIdentityFallbacks)
{
    cv::Mat image = makeTaggedRow();
    const cv::Mat original = image.clone();
    ChromaSpreadSettings settings;
    settings.redScale = std::numeric_limits<float>::quiet_NaN();
    settings.greenScale = std::numeric_limits<float>::infinity();
    settings.blueScale = -std::numeric_limits<float>::infinity();
    settings.shiftAmount = std::numeric_limits<float>::quiet_NaN();
    settings.shiftAngle = std::numeric_limits<float>::infinity();
    settings.dispersionSteps = 1;
    ChromaSpread effect;
    effect.processMat(&image, settings);

    for (int x = 0; x < image.cols; ++x) {
        const cv::Vec4f actual = image.at<cv::Vec4f>(0, x);
        const cv::Vec4f expected = original.at<cv::Vec4f>(0, x);
        for (int channel = 0; channel < 4; ++channel) {
            EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
        }
    }
}

TEST(ChromaSpreadEffectContractTest, NullEmptyAndUnsupportedMatricesAreNoOps)
{
    ChromaSpread effect;
    ChromaSpreadSettings settings;
    effect.processMat(nullptr, settings);
    cv::Mat empty;
    effect.processMat(&empty, settings);

    cv::Mat unsupported(2, 2, CV_8UC4, cv::Scalar(1, 2, 3, 4));
    const cv::Mat original = unsupported.clone();
    effect.processMat(&unsupported, settings);
    EXPECT_EQ(cv::countNonZero(unsupported.reshape(1) != original.reshape(1)), 0);
}

