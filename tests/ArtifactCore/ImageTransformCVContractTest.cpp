#include <gtest/gtest.h>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include <stdexcept>

import Image:ImageTransform;

using ArtifactCore::convertToFloat32RGBA;

TEST(ImageTransformCVContractTest, ConvertsEightBitBgrToNormalizedFloatRgba)
{
    cv::Mat input(1, 1, CV_8UC3);
    input.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 128, 255);

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    const auto pixel = output.at<cv::Vec4f>(0, 0);
    EXPECT_FLOAT_EQ(pixel[0], 1.0f);
    EXPECT_NEAR(pixel[1], 128.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(pixel[2], 0.0f);
    EXPECT_FLOAT_EQ(pixel[3], 1.0f);
}

TEST(ImageTransformCVContractTest, ConvertsEightBitGrayscaleToEqualRgbAndOpaqueAlpha)
{
    cv::Mat input(1, 1, CV_8UC1, cv::Scalar(128));

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    const auto pixel = output.at<cv::Vec4f>(0, 0);
    EXPECT_NEAR(pixel[0], 128.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(pixel[0], pixel[1]);
    EXPECT_FLOAT_EQ(pixel[1], pixel[2]);
    EXPECT_FLOAT_EQ(pixel[3], 1.0f);
}

TEST(ImageTransformCVContractTest, Float32InputIsNotRescaled)
{
    cv::Mat input(1, 1, CV_32FC3);
    input.at<cv::Vec3f>(0, 0) = cv::Vec3f(0.25f, 0.5f, 2.0f);

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    const auto pixel = output.at<cv::Vec4f>(0, 0);
    EXPECT_FLOAT_EQ(pixel[0], 2.0f);
    EXPECT_FLOAT_EQ(pixel[1], 0.5f);
    EXPECT_FLOAT_EQ(pixel[2], 0.25f);
    EXPECT_FLOAT_EQ(pixel[3], 1.0f);
}

TEST(ImageTransformCVContractTest, Float32FourChannelInputIsPreservedWithoutCopy)
{
    cv::Mat input(1, 1, CV_32FC4);
    input.at<cv::Vec4f>(0, 0) = cv::Vec4f(0.1f, 0.2f, 0.3f, 0.4f);

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    EXPECT_EQ(output.data, input.data);
    const auto actual = output.at<cv::Vec4f>(0, 0);
    const auto expected = input.at<cv::Vec4f>(0, 0);
    for (int channel = 0; channel < 4; ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], expected[channel]);
    }
}

TEST(ImageTransformCVContractTest, UnsupportedChannelCountThrows)
{
    const cv::Mat input(1, 1, CV_8UC2, cv::Scalar(1, 2));
    EXPECT_THROW(convertToFloat32RGBA(input), std::runtime_error);
}
