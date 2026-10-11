#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <gtest/gtest.h>
#include <QTemporaryDir>

#include <array>
#include <cstddef>

import ImageProcessing;
import Image;
import FloatRGBA;
import Particle;

using namespace ArtifactCore::Keying;
using ArtifactCore::float4;
using ArtifactCore::ImageF32x4_RGBA;
using ArtifactCore::FloatRGBA;

namespace {

constexpr int kWidth = 96;
constexpr int kHeight = 64;
constexpr int kFrameCount = 3;
constexpr int kFramesPerSecond = 24;
constexpr int kSubjectX = 48;
constexpr int kSubjectY = 32;

cv::Mat makeGreenScreenFrame(int frameIndex) {
    cv::Mat frame(kHeight, kWidth, CV_8UC3, cv::Scalar(0, 255, 0));
    const int offset = frameIndex;
    cv::rectangle(frame, cv::Rect(30 + offset, 10, 36, 46),
                  cv::Scalar(20, 25, 235), cv::FILLED, cv::LINE_AA);
    cv::ellipse(frame, cv::Point(48 + offset, 20), cv::Size(9, 7), 0, 0, 360,
                cv::Scalar(45, 70, 210), cv::FILLED, cv::LINE_AA);
    return frame;
}

float alphaAt(const std::array<float, kWidth * kHeight * 4>& pixels,
              int x, int y) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)) * 4;
    return pixels[index + 3];
}

} // namespace

TEST(KeyingVideoInputTest, RemovesGreenScreenAndPreservesForegroundAcrossDecodedFrames) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("keying-input.avi"));

    cv::VideoWriter writer(videoPath.toStdString(),
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           kFramesPerSecond, cv::Size(kWidth, kHeight), true);
    ASSERT_TRUE(writer.isOpened()) << "OpenCV could not create the temporary AVI fixture";
    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        writer.write(makeGreenScreenFrame(frameIndex));
    }
    writer.release();

    cv::VideoCapture capture(videoPath.toStdString());
    ASSERT_TRUE(capture.isOpened()) << "OpenCV could not reopen the temporary AVI fixture";

    ChromaKeyParams params;
    params.keyColor = float4{0.0f, 1.0f, 0.0f, 1.0f};
    params.hueTolerance = 0.22f;
    params.edgeSoftness = 0.10f;
    params.despillStrength = 0.8f;
    params.despillMode = 1;

    int decodedFrameCount = 0;
    cv::Mat frame;
    while (capture.read(frame)) {
        ASSERT_FALSE(frame.empty());
        ASSERT_EQ(frame.cols, kWidth);
        ASSERT_EQ(frame.rows, kHeight);

        std::array<float, kWidth * kHeight * 4> source{};
        std::array<float, kWidth * kHeight * 4> destination{};
        for (int y = 0; y < kHeight; ++y) {
            const auto* row = frame.ptr<cv::Vec3b>(y);
            for (int x = 0; x < kWidth; ++x) {
                const std::size_t index =
                    (static_cast<std::size_t>(y) * kWidth + x) * 4;
                source[index] = row[x][2] / 255.0f;
                source[index + 1] = row[x][1] / 255.0f;
                source[index + 2] = row[x][0] / 255.0f;
                source[index + 3] = 1.0f;
            }
        }

        const ChromaKeyBuffers buffers{source.data(), destination.data(), kWidth, kHeight};
        ASSERT_TRUE(processChromaKey(buffers, params));

        ImageF32x4_RGBA keyedForeground;
        keyedForeground.setFromRGBA32F(destination.data(), kWidth, kHeight);
        ImageF32x4_RGBA composite;
        composite.resize(kWidth, kHeight);
        composite.fill(FloatRGBA(0.0f, 0.0f, 1.0f, 1.0f));
        composite.alphaBlend(keyedForeground);

        const FloatRGBA backgroundPixel = composite.getPixel(8, 8);
        EXPECT_NEAR(backgroundPixel.r(), 0.0f, 0.05f);
        EXPECT_NEAR(backgroundPixel.g(), 0.0f, 0.05f);
        EXPECT_NEAR(backgroundPixel.b(), 1.0f, 0.05f)
            << "transparent screen should reveal the blue composite background";
        const FloatRGBA foregroundPixel =
            composite.getPixel(kSubjectX + decodedFrameCount, kSubjectY);
        EXPECT_GT(foregroundPixel.r(), 0.75f)
            << "opaque subject should remain in the composite";
        EXPECT_GT(foregroundPixel.b(), 0.0f)
            << "foreground should be alpha-composited over the blue background";

        EXPECT_LT(alphaAt(destination, 8, 8), 0.15f) << "screen should become transparent";
        EXPECT_GT(alphaAt(destination, kSubjectX + decodedFrameCount, kSubjectY), 0.90f)
            << "foreground should remain opaque";
        ++decodedFrameCount;
    }
    EXPECT_EQ(decodedFrameCount, kFrameCount);
}
