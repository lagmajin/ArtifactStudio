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

TEST(ImageYUV420ContractTest, FromPlanesKeepsFullResolutionLumaAndCeilingSubsampledChroma)
{
    const float yValues[] = {
        0.0f, 0.25f, 0.5f,
        0.75f, 1.0f, 0.125f,
        0.375f, 0.625f, 0.875f
    };
    const float uValues[] = { 0.0f, 0.5f, 1.0f, 0.25f };
    const float vValues[] = { 1.0f, 0.5f, 0.0f, 0.75f };
    const cv::Mat y(3, 3, CV_32FC1, const_cast<float*>(yValues));
    const cv::Mat u(2, 2, CV_32FC1, const_cast<float*>(uValues));
    const cv::Mat v(2, 2, CV_32FC1, const_cast<float*>(vValues));

    const ImageYUV420 converted = ImageYUV420::fromPlanes(y, u, v);
    const cv::Mat actualY = converted.yPlane();
    const cv::Mat actualU = converted.uPlane();
    const cv::Mat actualV = converted.vPlane();

    ASSERT_EQ(actualY.type(), CV_8UC1);
    ASSERT_EQ(actualY.rows, 3);
    ASSERT_EQ(actualY.cols, 3);
    ASSERT_EQ(actualU.type(), CV_8UC1);
    ASSERT_EQ(actualU.rows, 2);
    ASSERT_EQ(actualU.cols, 2);
    ASSERT_EQ(actualV.type(), CV_8UC1);
    ASSERT_EQ(actualV.rows, 2);
    ASSERT_EQ(actualV.cols, 2);
    EXPECT_EQ(actualY.at<std::uint8_t>(0, 0), 0);
    EXPECT_EQ(actualY.at<std::uint8_t>(0, 1), 63);
    EXPECT_EQ(actualY.at<std::uint8_t>(1, 1), 255);
    EXPECT_EQ(actualY.at<std::uint8_t>(2, 2), 223);
    EXPECT_EQ(actualU.at<std::uint8_t>(0, 1), 127);
    EXPECT_EQ(actualU.at<std::uint8_t>(1, 0), 255);
    EXPECT_EQ(actualV.at<std::uint8_t>(0, 0), 255);
    EXPECT_EQ(actualV.at<std::uint8_t>(1, 1), 191);
}

TEST(ImageYUV420ContractTest, FloatImageIngressRespectsRgbaAndBgraDescriptors)
{
    ImageF32x4_RGBA rgba;
    rgba.resize(1, 1);
    rgba.setColorDescriptor(SurfaceColorDescriptor::unknownRgba32Float());
    float* rgbaPixel = rgba.rgba32fData();
    ASSERT_NE(rgbaPixel, nullptr);
    rgbaPixel[0] = 1.0f;
    rgbaPixel[1] = 0.0f;
    rgbaPixel[2] = 0.0f;
    rgbaPixel[3] = 1.0f;

    ImageF32x4_RGBA bgra;
    bgra.resize(1, 1);
    bgra.setColorDescriptor(SurfaceColorDescriptor::unknownBgra32Float());
    float* bgraPixel = bgra.rgba32fData();
    ASSERT_NE(bgraPixel, nullptr);
    bgraPixel[0] = 0.0f;
    bgraPixel[1] = 0.0f;
    bgraPixel[2] = 1.0f;
    bgraPixel[3] = 1.0f;

    const ImageYUV420 fromRgba = ImageYUV420::fromImage32xRGBA(rgba);
    const ImageYUV420 fromBgra = ImageYUV420::fromImage32xRGBA(bgra);
    EXPECT_EQ(fromRgba.yPlane().at<std::uint8_t>(0, 0), 76);
    EXPECT_EQ(fromRgba.uPlane().at<std::uint8_t>(0, 0), 85);
    EXPECT_EQ(fromRgba.vPlane().at<std::uint8_t>(0, 0), 255);
    EXPECT_EQ(fromBgra.yPlane().at<std::uint8_t>(0, 0), 76);
    EXPECT_EQ(fromBgra.uPlane().at<std::uint8_t>(0, 0), 85);
    EXPECT_EQ(fromBgra.vPlane().at<std::uint8_t>(0, 0), 255);
}

TEST(ImageYUV420ContractTest, FloatImageIngressRejectsUnknownChannelOrder)
{
    ImageF32x4_RGBA source;
    source.resize(1, 1);
    float* pixel = source.rgba32fData();
    ASSERT_NE(pixel, nullptr);
    pixel[0] = 1.0f;
    pixel[1] = 0.0f;
    pixel[2] = 0.0f;
    pixel[3] = 1.0f;

    const ImageYUV420 converted = ImageYUV420::fromImage32xRGBA(source);

    EXPECT_EQ(converted.yPlane().total(), 0u);
    EXPECT_EQ(converted.uPlane().total(), 0u);
    EXPECT_EQ(converted.vPlane().total(), 0u);
}
