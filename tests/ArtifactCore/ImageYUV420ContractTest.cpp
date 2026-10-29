#include <gtest/gtest.h>

#include <QImage>

#include <opencv2/core.hpp>

#include <cstdint>

import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Image.ImageYUV420;

using namespace ArtifactCore;

TEST(ImageYUV420ContractTest, OddSizedColorImageUsesCeilingSubsampledChromaPlanes)
{
    QImage source(3, 3, QImage::Format_ARGB32);
    source.fill(qRgb(255, 0, 0));

    const ImageYUV420 converted(source);
    const cv::Mat y = converted.yPlane();
    const cv::Mat u = converted.uPlane();
    const cv::Mat v = converted.vPlane();

    ASSERT_EQ(y.type(), CV_8UC1);
    ASSERT_EQ(y.rows, 3);
    ASSERT_EQ(y.cols, 3);
    ASSERT_EQ(u.type(), CV_8UC1);
    ASSERT_EQ(u.rows, 2);
    ASSERT_EQ(u.cols, 2);
    ASSERT_EQ(v.type(), CV_8UC1);
    ASSERT_EQ(v.rows, 2);
    ASSERT_EQ(v.cols, 2);
    EXPECT_EQ(y.at<std::uint8_t>(1, 1), 76);
    EXPECT_EQ(u.at<std::uint8_t>(0, 0), 85);
    EXPECT_EQ(v.at<std::uint8_t>(1, 1), 255);

    y.at<std::uint8_t>(0, 0) = 0;
    EXPECT_EQ(converted.yPlane().at<std::uint8_t>(0, 0), 76);
}

TEST(ImageYUV420ContractTest, DecodedImageUsesDocumentedBgraBackingAndOpaqueAlpha)
{
    QImage source(2, 2, QImage::Format_ARGB32);
    source.fill(qRgb(255, 0, 0));
    const ImageYUV420 converted(source);

    const ImageF32x4_RGBA decoded = converted.toImage32xRGBA();
    ASSERT_EQ(decoded.width(), 2);
    ASSERT_EQ(decoded.height(), 2);
    EXPECT_EQ(decoded.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);

    const float* pixel = decoded.rgba32fData();
    ASSERT_NE(pixel, nullptr);
    EXPECT_NEAR(pixel[0], 0.0f, 1.0e-6f);
    EXPECT_NEAR(pixel[1], 0.0f, 1.0e-6f);
    EXPECT_NEAR(pixel[2], 254.0f / 255.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel[3], 1.0f);
}
