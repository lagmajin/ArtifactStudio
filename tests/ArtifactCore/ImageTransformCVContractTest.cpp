#include <gtest/gtest.h>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include <cstdint>
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

TEST(ImageTransformCVContractTest, ConvertsStridedBgrRoiAcrossRows)
{
    cv::Mat backing(2, 4, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::Mat input = backing(cv::Rect(1, 0, 2, 2));
    input.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 64, 255);
    input.at<cv::Vec3b>(0, 1) = cv::Vec3b(32, 128, 224);
    input.at<cv::Vec3b>(1, 0) = cv::Vec3b(16, 80, 240);
    input.at<cv::Vec3b>(1, 1) = cv::Vec3b(48, 160, 192);
    ASSERT_GT(input.step, static_cast<std::size_t>(input.cols * input.elemSize()));

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    ASSERT_EQ(output.rows, 2);
    ASSERT_EQ(output.cols, 2);
    const auto first = output.at<cv::Vec4f>(0, 0);
    EXPECT_FLOAT_EQ(first[0], 1.0f);
    EXPECT_NEAR(first[1], 64.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(first[2], 0.0f);
    EXPECT_FLOAT_EQ(first[3], 1.0f);
    const auto last = output.at<cv::Vec4f>(1, 1);
    EXPECT_NEAR(last[0], 192.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(last[1], 160.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(last[2], 48.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(last[3], 1.0f);
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

TEST(ImageTransformCVContractTest, NormalizesSixteenBitGrayscaleAcrossItsFullRange)
{
    cv::Mat input(1, 3, CV_16UC1);
    input.at<std::uint16_t>(0, 0) = 0;
    input.at<std::uint16_t>(0, 1) = 32768;
    input.at<std::uint16_t>(0, 2) = 65535;

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    EXPECT_NEAR(output.at<cv::Vec4f>(0, 0)[0], 0.0f, 1e-6f);
    EXPECT_NEAR(output.at<cv::Vec4f>(0, 1)[0], 32768.0f / 65535.0f, 1e-6f);
    EXPECT_NEAR(output.at<cv::Vec4f>(0, 2)[0], 1.0f, 1e-6f);
    for (int column = 0; column < input.cols; ++column) {
        const auto pixel = output.at<cv::Vec4f>(0, column);
        EXPECT_FLOAT_EQ(pixel[0], pixel[1]);
        EXPECT_FLOAT_EQ(pixel[1], pixel[2]);
        EXPECT_FLOAT_EQ(pixel[3], 1.0f);
    }
}

TEST(ImageTransformCVContractTest, NormalizesStridedSixteenBitRgbaChannelsAcrossRows)
{
    cv::Mat backing(2, 4, CV_16UC4, cv::Scalar(65535, 65535, 65535, 65535));
    cv::Mat input = backing(cv::Rect(1, 0, 2, 2));
    input.at<cv::Vec<std::uint16_t, 4>>(0, 0) = {0, 1, 32768, 65535};
    input.at<cv::Vec<std::uint16_t, 4>>(0, 1) = {65535, 32768, 1, 0};
    input.at<cv::Vec<std::uint16_t, 4>>(1, 0) = {257, 514, 771, 1028};
    input.at<cv::Vec<std::uint16_t, 4>>(1, 1) = {32767, 32768, 32769, 65534};
    ASSERT_GT(input.step, static_cast<std::size_t>(input.cols * input.elemSize()));

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    ASSERT_EQ(output.rows, 2);
    ASSERT_EQ(output.cols, 2);
    const std::uint16_t expected[2][2][4] = {
        {{0, 1, 32768, 65535}, {65535, 32768, 1, 0}},
        {{257, 514, 771, 1028}, {32767, 32768, 32769, 65534}},
    };
    for (int row = 0; row < input.rows; ++row) {
        for (int column = 0; column < input.cols; ++column) {
            const auto actual = output.at<cv::Vec4f>(row, column);
            for (int channel = 0; channel < 4; ++channel) {
                EXPECT_NEAR(actual[channel],
                            static_cast<float>(expected[row][column][channel]) / 65535.0f,
                            1.0e-6f)
                    << "row=" << row << " column=" << column
                    << " channel=" << channel;
            }
        }
    }
}

TEST(ImageTransformCVContractTest, ConvertsEightBitFourChannelInputWithoutReordering)
{
    cv::Mat input(1, 1, CV_8UC4);
    input.at<cv::Vec4b>(0, 0) = cv::Vec4b(10, 20, 30, 40);

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    const auto pixel = output.at<cv::Vec4f>(0, 0);
    EXPECT_NEAR(pixel[0], 10.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(pixel[1], 20.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(pixel[2], 30.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(pixel[3], 40.0f / 255.0f, 1e-6f);
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

TEST(ImageTransformCVContractTest, StridedFloat32RoiPreservesViewAndStorageAliasing)
{
    cv::Mat backing(2, 4, CV_32FC4, cv::Scalar(0.0f, 0.0f, 0.0f, 1.0f));
    cv::Mat input = backing(cv::Rect(1, 0, 2, 2));
    input.at<cv::Vec4f>(0, 0) = cv::Vec4f(0.1f, 0.2f, 0.3f, 0.4f);
    input.at<cv::Vec4f>(1, 1) = cv::Vec4f(0.5f, 0.6f, 0.7f, 0.8f);
    ASSERT_GT(input.step, static_cast<std::size_t>(input.cols * input.elemSize()));

    const cv::Mat output = convertToFloat32RGBA(input);

    ASSERT_EQ(output.type(), CV_32FC4);
    EXPECT_EQ(output.data, input.data);
    EXPECT_EQ(output.step, input.step);
    EXPECT_EQ(output.rows, input.rows);
    EXPECT_EQ(output.cols, input.cols);
    EXPECT_FLOAT_EQ(output.at<cv::Vec4f>(1, 1)[2], 0.7f);

    input.at<cv::Vec4f>(0, 0)[1] = 0.9f;
    EXPECT_FLOAT_EQ(output.at<cv::Vec4f>(0, 0)[1], 0.9f);
}

TEST(ImageTransformCVContractTest, UnsupportedChannelCountThrows)
{
    const cv::Mat input(1, 1, CV_8UC2, cv::Scalar(1, 2));
    EXPECT_THROW(convertToFloat32RGBA(input), std::runtime_error);
}
