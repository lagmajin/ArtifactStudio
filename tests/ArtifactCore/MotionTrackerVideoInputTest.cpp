#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <gtest/gtest.h>
#include <QTemporaryDir>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <vector>

import Tracking.MotionTracker;
import Image;
import FloatRGBA;

using namespace ArtifactCore;

namespace {

constexpr int kFrameWidth = 192;
constexpr int kFrameHeight = 128;
constexpr int kFrameCount = 12;
constexpr int kFramesPerSecond = 24;
constexpr int kMotionPerFrame = 2;
constexpr int kComplexFrameWidth = 640;
constexpr int kComplexFrameHeight = 360;
constexpr int kComplexFrameCount = 24;
constexpr int kComplexPlaneWidth = 360;
constexpr int kComplexPlaneHeight = 260;
constexpr int kRealFootageWidth = 360;
constexpr int kRealFootageHeight = 640;

cv::Mat makeTrackingFrame(int frameIndex) {
    cv::Mat frame(kFrameHeight, kFrameWidth, CV_8UC3, cv::Scalar(24, 24, 24));
    const cv::Rect textureBounds(30 + frameIndex * kMotionPerFrame, 30,
                                 72, 64);
    cv::Mat texture = frame(textureBounds);
    for (int y = 0; y < texture.rows; y += 8) {
        for (int x = 0; x < texture.cols; x += 8) {
            const auto value = static_cast<unsigned char>(
                ((x / 8 + y / 8) % 2 == 0) ? 230 : 54);
            cv::rectangle(texture, cv::Rect(x, y, 8, 8),
                          cv::Scalar(value, value, value), cv::FILLED);
        }
    }
    cv::line(texture, {4, 4}, {texture.cols - 5, texture.rows - 5},
             cv::Scalar(10, 180, 250), 2);
    cv::circle(texture, {texture.cols / 2, texture.rows / 2}, 9,
               cv::Scalar(240, 40, 20), cv::FILLED);
    return frame;
}

std::array<cv::Point2f, 4> perspectiveDestinationCorners(int frameIndex, bool steep = false) {
    const float t = static_cast<float>(frameIndex) /
                    static_cast<float>(kComplexFrameCount - 1);
    if (steep) {
        return {cv::Point2f(100.0f + 230.0f * t, 60.0f),
                cv::Point2f(500.0f - 80.0f * t, 75.0f + 70.0f * t),
                cv::Point2f(475.0f - 65.0f * t, 300.0f - 15.0f * t),
                cv::Point2f(85.0f + 215.0f * t, 280.0f + 40.0f * t)};
    }
    return {
        cv::Point2f(100.0f + 45.0f * t, 60.0f + 25.0f * t),
        cv::Point2f(500.0f + 55.0f * t, 75.0f + 45.0f * t),
        cv::Point2f(475.0f - 20.0f * t, 300.0f + 20.0f * t),
        cv::Point2f(85.0f - 10.0f * t, 280.0f - 35.0f * t)};
}

cv::Mat makePerspectiveTrackingFrame(int frameIndex, bool steep = false) {
    cv::Mat texture(kComplexPlaneHeight, kComplexPlaneWidth, CV_8UC3);
    cv::RNG random(0x5EED);
    random.fill(texture, cv::RNG::UNIFORM, cv::Scalar(26, 26, 26),
                cv::Scalar(225, 225, 225));
    for (int index = 0; index < 120; ++index) {
        const cv::Point center(random.uniform(4, kComplexPlaneWidth - 4),
                               random.uniform(4, kComplexPlaneHeight - 4));
        const cv::Scalar color(random.uniform(15, 245), random.uniform(15, 245),
                               random.uniform(15, 245));
        cv::circle(texture, center, random.uniform(3, 10), color, cv::FILLED,
                   cv::LINE_AA);
    }
    for (int x = 16; x < kComplexPlaneWidth; x += 32) {
        cv::line(texture, {x, 0}, {x - 12, kComplexPlaneHeight - 1},
                 cv::Scalar(250, 250, 250), 2, cv::LINE_AA);
    }
    for (int y = 24; y < kComplexPlaneHeight; y += 40) {
        cv::line(texture, {0, y}, {kComplexPlaneWidth - 1, y - 10},
                 cv::Scalar(30, 230, 245), 2, cv::LINE_AA);
    }

    const std::array<cv::Point2f, 4> sourceCorners = {
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(kComplexPlaneWidth - 1.0f, 0.0f),
        cv::Point2f(kComplexPlaneWidth - 1.0f, kComplexPlaneHeight - 1.0f),
        cv::Point2f(0.0f, kComplexPlaneHeight - 1.0f)};
    const auto destinationCorners = perspectiveDestinationCorners(frameIndex, steep);
    const cv::Mat transform = cv::getPerspectiveTransform(
        sourceCorners.data(), destinationCorners.data());
    cv::Mat frame(kComplexFrameHeight, kComplexFrameWidth, CV_8UC3,
                  cv::Scalar(18, 18, 18));
    cv::warpPerspective(texture, frame, transform, frame.size(), cv::INTER_LINEAR,
                        cv::BORDER_TRANSPARENT);
    return frame;
}

} // namespace

TEST(MotionTrackerVideoInputTest, TracksMotionFromFramesDecodedFromVideoFile) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("tracking-input.avi"));

    cv::VideoWriter writer(videoPath.toStdString(),
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           kFramesPerSecond,
                           cv::Size(kFrameWidth, kFrameHeight), true);
    ASSERT_TRUE(writer.isOpened()) << "OpenCV could not create the temporary AVI fixture";
    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        writer.write(makeTrackingFrame(frameIndex));
    }
    writer.release();

    cv::VideoCapture capture(videoPath.toStdString());
    ASSERT_TRUE(capture.isOpened()) << "OpenCV could not reopen the temporary AVI fixture";

    MotionTracker tracker;
    auto settings = TrackerSettings::normal();
    settings.method = TrackingMethod::OpticalFlow;
    settings.confidenceThreshold = 0.0;
    settings.maxPyramidLevel = 2;
    tracker.setSettings(settings);

    const QPointF initialPosition(66.0, 62.0);
    tracker.addTrackPoint(initialPosition);

    int decodedFrameCount = 0;
    cv::Mat frame;
    while (capture.read(frame)) {
        ASSERT_FALSE(frame.empty());
        tracker.setFrame(static_cast<double>(decodedFrameCount) / kFramesPerSecond,
                         frame);
        ++decodedFrameCount;
    }
    ASSERT_EQ(decodedFrameCount, kFrameCount);

    ASSERT_TRUE(tracker.trackRange(
        0.0, static_cast<double>(kFrameCount - 1) / kFramesPerSecond));
    const TrackResult result = tracker.result();
    ASSERT_EQ(result.frameCount(), static_cast<std::size_t>(kFrameCount));

    const auto pointId = tracker.firstTrackPointId();
    ASSERT_GE(pointId, 0);
    const QPointF trackedPosition = tracker.pointPositionAt(
        pointId, static_cast<double>(kFrameCount - 1) / kFramesPerSecond);
    const double expectedX = initialPosition.x() +
                             (kFrameCount - 1) * kMotionPerFrame;
    EXPECT_NEAR(trackedPosition.x(), expectedX, 4.0);
    EXPECT_NEAR(trackedPosition.y(), initialPosition.y(), 4.0);
    EXPECT_GT(tracker.averageConfidence(), 0.0);
}


TEST(MotionTrackerVideoInputTest, TracksPlanarMotionFromFramesDecodedFromVideoFile) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("planar-tracking-input.avi"));

    cv::VideoWriter writer(videoPath.toStdString(),
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           kFramesPerSecond,
                           cv::Size(kFrameWidth, kFrameHeight), true);
    ASSERT_TRUE(writer.isOpened()) << "OpenCV could not create the temporary AVI fixture";
    for (int frameIndex = 0; frameIndex < kFrameCount; ++frameIndex) {
        writer.write(makeTrackingFrame(frameIndex));
    }
    writer.release();

    cv::VideoCapture capture(videoPath.toStdString());
    ASSERT_TRUE(capture.isOpened()) << "OpenCV could not reopen the temporary AVI fixture";

    MotionTracker tracker;
    auto settings = TrackerSettings::normal();
    settings.method = TrackingMethod::OpticalFlow;
    settings.confidenceThreshold = 0.0;
    settings.maxPyramidLevel = 2;
    tracker.setSettings(settings);
    tracker.setTrackerType(TrackerType::Planar);

    const QPointF initialPosition(66.0, 62.0);
    tracker.addTrackPoint(initialPosition);
    tracker.addTrackRegion(QRectF(30.0, 30.0, 72.0, 64.0));

    int decodedFrameCount = 0;
    cv::Mat frame;
    while (capture.read(frame)) {
        ASSERT_FALSE(frame.empty());
        tracker.setFrame(static_cast<double>(decodedFrameCount) / kFramesPerSecond,
                         frame);
        ++decodedFrameCount;
    }
    ASSERT_EQ(decodedFrameCount, kFrameCount);

    ASSERT_TRUE(tracker.trackRange(
        0.0, static_cast<double>(kFrameCount - 1) / kFramesPerSecond));
    const TrackResult result = tracker.result();
    ASSERT_EQ(result.frameCount(), static_cast<std::size_t>(kFrameCount));

    const TrackFrame* finalFrame = tracker.frameAt(
        static_cast<double>(kFrameCount - 1) / kFramesPerSecond);
    ASSERT_NE(finalFrame, nullptr);
    ASSERT_TRUE(finalFrame->hasHomography);
    const auto pointId = tracker.firstTrackPointId();
    ASSERT_GE(pointId, 0);
    const double finalTime = static_cast<double>(kFrameCount - 1) / kFramesPerSecond;
    const double previousTime = static_cast<double>(kFrameCount - 2) / kFramesPerSecond;
    const QPointF previousPosition = tracker.pointPositionAt(pointId, previousTime);
    const QPointF trackedPosition = tracker.pointPositionAt(pointId, finalTime);
    QPointF projectedPosition;
    ASSERT_TRUE(finalFrame->projectPoint(previousPosition, projectedPosition));
    EXPECT_NEAR(projectedPosition.x(), trackedPosition.x(), 1.0);
    EXPECT_NEAR(projectedPosition.y(), trackedPosition.y(), 1.0);
    EXPECT_NEAR(trackedPosition.x(), initialPosition.x() +
                (kFrameCount - 1) * kMotionPerFrame, 4.0);
    EXPECT_NEAR(trackedPosition.y(), initialPosition.y(), 4.0);
    EXPECT_GT(tracker.averageConfidence(), 0.0);
}


TEST(MotionTrackerVideoInputTest, TracksPerspectiveAndScaleChangesFromVideoFile) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString videoPath = temporaryDirectory.filePath(QStringLiteral("complex-planar-tracking-input.avi"));

    cv::VideoWriter writer(videoPath.toStdString(),
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           kFramesPerSecond,
                           cv::Size(kComplexFrameWidth, kComplexFrameHeight), true);
    ASSERT_TRUE(writer.isOpened()) << "OpenCV could not create the temporary AVI fixture";
    for (int frameIndex = 0; frameIndex < kComplexFrameCount; ++frameIndex) {
        writer.write(makePerspectiveTrackingFrame(frameIndex));
    }
    writer.release();

    cv::VideoCapture capture(videoPath.toStdString());
    ASSERT_TRUE(capture.isOpened()) << "OpenCV could not reopen the temporary AVI fixture";

    MotionTracker tracker;
    auto settings = TrackerSettings::normal();
    settings.method = TrackingMethod::OpticalFlow;
    settings.confidenceThreshold = 0.0;
    settings.maxPyramidLevel = 3;
    tracker.setSettings(settings);
    tracker.setTrackerType(TrackerType::Planar);

    const std::array<cv::Point2f, 4> sourcePlaneCorners = {
        cv::Point2f(0.0f, 0.0f), cv::Point2f(kComplexPlaneWidth - 1.0f, 0.0f),
        cv::Point2f(kComplexPlaneWidth - 1.0f, kComplexPlaneHeight - 1.0f),
        cv::Point2f(0.0f, kComplexPlaneHeight - 1.0f)};
    const cv::Mat firstPlaneTransform = cv::getPerspectiveTransform(
        sourcePlaneCorners.data(), perspectiveDestinationCorners(0).data());
    std::array<cv::Point2f, 4> graphicCornersInPlane = {
        cv::Point2f(140.0f, 90.0f), cv::Point2f(220.0f, 90.0f),
        cv::Point2f(220.0f, 146.0f), cv::Point2f(140.0f, 146.0f)};
    cv::Mat graphicCornersInPlaneMat(1, 4, CV_32FC2, graphicCornersInPlane.data());
    cv::Mat initialGraphicCornersMat;
    cv::perspectiveTransform(graphicCornersInPlaneMat, initialGraphicCornersMat,
                             firstPlaneTransform);
    std::array<QPointF, 4> initialGraphicCorners{};
    for (std::size_t corner = 0; corner < initialGraphicCorners.size(); ++corner) {
        const cv::Point2f point = initialGraphicCornersMat.at<cv::Point2f>(0, static_cast<int>(corner));
        initialGraphicCorners[corner] = QPointF(point.x, point.y);
    }
    cv::Point2f initialPlanePoint(180.0f, 118.0f);
    cv::Mat initialPlanePointMat(1, 1, CV_32FC2, &initialPlanePoint);
    cv::Mat initialTrackedPointMat;
    cv::perspectiveTransform(initialPlanePointMat, initialTrackedPointMat,
                             firstPlaneTransform);
    const cv::Point2f initialPlanePointInFrame = initialTrackedPointMat.at<cv::Point2f>(0, 0);
    const QPointF initialPosition(initialPlanePointInFrame.x, initialPlanePointInFrame.y);
    tracker.addTrackPoint(initialPosition);
    tracker.addTrackRegion(QRectF(75.0, 45.0, 485.0, 280.0));

    int decodedFrameCount = 0;
    std::array<cv::Mat, kComplexFrameCount> decodedFrames;
    cv::Mat frame;
    while (capture.read(frame)) {
        ASSERT_FALSE(frame.empty());
        ASSERT_LT(decodedFrameCount, kComplexFrameCount);
        decodedFrames[static_cast<std::size_t>(decodedFrameCount)] = frame.clone();
        tracker.setFrame(static_cast<double>(decodedFrameCount) / kFramesPerSecond,
                         frame);
        ++decodedFrameCount;
    }
    ASSERT_EQ(decodedFrameCount, kComplexFrameCount);

    ASSERT_TRUE(tracker.trackRange(
        0.0, static_cast<double>(kComplexFrameCount - 1) / kFramesPerSecond));
    const TrackResult result = tracker.result();
    ASSERT_EQ(result.frameCount(), static_cast<std::size_t>(kComplexFrameCount));
    const auto pointId = tracker.firstTrackPointId();
    ASSERT_GE(pointId, 0);
    const double finalTime = static_cast<double>(kComplexFrameCount - 1) / kFramesPerSecond;
    const double previousTime = static_cast<double>(kComplexFrameCount - 2) / kFramesPerSecond;
    const TrackFrame* finalFrame = tracker.frameAt(finalTime);
    ASSERT_NE(finalFrame, nullptr);
    ASSERT_TRUE(finalFrame->hasHomography);

    const QPointF previousPosition = tracker.pointPositionAt(pointId, previousTime);
    const QPointF trackedPosition = tracker.pointPositionAt(pointId, finalTime);
    QPointF projectedPosition;
    ASSERT_TRUE(finalFrame->projectPoint(previousPosition, projectedPosition));
    EXPECT_NEAR(projectedPosition.x(), trackedPosition.x(), 1.5);
    EXPECT_NEAR(projectedPosition.y(), trackedPosition.y(), 1.5);

    // Position should remain within two pixels of the known generated motion.
    const cv::Mat firstPlaneTransformForPoint = firstPlaneTransform;
    const cv::Mat finalPlaneTransformForPoint = cv::getPerspectiveTransform(
        std::array<cv::Point2f, 4>{
            cv::Point2f(0.0f, 0.0f),
            cv::Point2f(kComplexPlaneWidth - 1.0f, 0.0f),
            cv::Point2f(kComplexPlaneWidth - 1.0f, kComplexPlaneHeight - 1.0f),
            cv::Point2f(0.0f, kComplexPlaneHeight - 1.0f)}.data(),
        perspectiveDestinationCorners(kComplexFrameCount - 1).data());
    const cv::Mat expectedOverallMotion =
        finalPlaneTransformForPoint * firstPlaneTransformForPoint.inv();
    cv::Point2f initialTrackedPointForGroundTruth(static_cast<float>(initialPosition.x()),
                                                  static_cast<float>(initialPosition.y()));
    cv::Mat initialPointMat(1, 1, CV_32FC2, &initialTrackedPointForGroundTruth);
    cv::Mat expectedPointMat;
    cv::perspectiveTransform(initialPointMat, expectedPointMat, expectedOverallMotion);
    const cv::Point2f expectedFinalPoint = expectedPointMat.at<cv::Point2f>(0, 0);
    EXPECT_NEAR(trackedPosition.x(), expectedFinalPoint.x, 2.0);
    EXPECT_NEAR(trackedPosition.y(), expectedFinalPoint.y, 2.0);
    EXPECT_GT(tracker.averageConfidence(), 0.0);

    // Follow a large graphic through each measured frame-to-frame homography,
    // then composite it over a background at the resulting planar location.
    std::array<QPointF, 4> baseGraphicCorners = initialGraphicCorners;
    std::array<std::array<QPointF, 4>, kComplexFrameCount> graphicCornersByFrame{};
    graphicCornersByFrame[0] = baseGraphicCorners;
    for (int frameIndex = 1; frameIndex < kComplexFrameCount; ++frameIndex) {
        const double time = static_cast<double>(frameIndex) / kFramesPerSecond;
        const TrackFrame* trackedFrame = tracker.frameAt(time);
        ASSERT_NE(trackedFrame, nullptr);
        ASSERT_TRUE(trackedFrame->hasHomography);
        for (QPointF& corner : baseGraphicCorners) {
            QPointF projectedCorner;
            ASSERT_TRUE(trackedFrame->projectPoint(corner, projectedCorner));
            corner = projectedCorner;
        }
        graphicCornersByFrame[static_cast<std::size_t>(frameIndex)] = baseGraphicCorners;
    }

    const cv::Mat finalPlaneTransform = cv::getPerspectiveTransform(
        sourcePlaneCorners.data(), perspectiveDestinationCorners(kComplexFrameCount - 1).data());
    const cv::Mat expectedMotion = finalPlaneTransform * firstPlaneTransform.inv();
    std::array<cv::Point2f, 4> baseGraphicForComparison{};
    for (std::size_t corner = 0; corner < baseGraphicForComparison.size(); ++corner) {
        baseGraphicForComparison[corner] = cv::Point2f(
            static_cast<float>(graphicCornersByFrame[0][corner].x()),
            static_cast<float>(graphicCornersByFrame[0][corner].y()));
    }
    cv::Mat baseGraphicMat(1, static_cast<int>(baseGraphicForComparison.size()),
                           CV_32FC2, baseGraphicForComparison.data());
    const cv::Mat inverseFirstPlaneTransform = firstPlaneTransform.inv();
    for (int frameIndex = 1; frameIndex < kComplexFrameCount; ++frameIndex) {
        const cv::Mat currentPlaneTransform = cv::getPerspectiveTransform(
            sourcePlaneCorners.data(), perspectiveDestinationCorners(frameIndex).data());
        const cv::Mat expectedMotionAtFrame =
            currentPlaneTransform * inverseFirstPlaneTransform;
        cv::Mat expectedCornersAtFrame;
        cv::perspectiveTransform(baseGraphicMat, expectedCornersAtFrame,
                                 expectedMotionAtFrame);
        for (int corner = 0; corner < 4; ++corner) {
            const auto& actual = graphicCornersByFrame[static_cast<std::size_t>(frameIndex)]
                                                       [static_cast<std::size_t>(corner)];
            const cv::Point2f expected = expectedCornersAtFrame.at<cv::Point2f>(0, corner);
            EXPECT_NEAR(actual.x(), expected.x, 2.0)
                << "frame " << frameIndex << " tracked graphic corner " << corner << " X error";
            EXPECT_NEAR(actual.y(), expected.y, 2.0)
                << "frame " << frameIndex << " tracked graphic corner " << corner << " Y error";
        }
    }

    constexpr int graphicWidth = 80;
    constexpr int graphicHeight = 56;
    std::array<float, graphicWidth * graphicHeight * 4> graphicPixels{};
    for (int y = 0; y < graphicHeight; ++y) {
        for (int x = 0; x < graphicWidth; ++x) {
            const std::size_t pixel =
                (static_cast<std::size_t>(y) * graphicWidth + x) * 4;
            const bool border = x < 3 || y < 3 || x >= graphicWidth - 3 ||
                                y >= graphicHeight - 3;
            graphicPixels[pixel] = 1.0f;
            graphicPixels[pixel + 1] = border ? 1.0f : 0.0f;
            graphicPixels[pixel + 2] = border ? 1.0f : 0.0f;
            graphicPixels[pixel + 3] = 1.0f;
        }
    }
    const std::array<cv::Point2f, 4> graphicSourceCorners = {
        cv::Point2f(0.0f, 0.0f), cv::Point2f(graphicWidth - 1.0f, 0.0f),
        cv::Point2f(graphicWidth - 1.0f, graphicHeight - 1.0f),
        cv::Point2f(0.0f, graphicHeight - 1.0f)};
    cv::Mat graphicSource(graphicHeight, graphicWidth, CV_32FC4,
                          graphicPixels.data());

    const char* outputPathFromEnvironment = std::getenv("ARTIFACT_TRACKING_COMPOSITE_OUTPUT");
    const std::filesystem::path outputPath = outputPathFromEnvironment
        ? std::filesystem::path(outputPathFromEnvironment)
        : std::filesystem::current_path() / "tracking-composite-preview.avi";
    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    cv::VideoWriter outputWriter(outputPath.string(),
                                 cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                                 kFramesPerSecond,
                                 cv::Size(kComplexFrameWidth, kComplexFrameHeight), true);
    ASSERT_TRUE(outputWriter.isOpened()) << "Could not create composite video: " << outputPath.string();

    for (int frameIndex = 0; frameIndex < kComplexFrameCount; ++frameIndex) {
        std::array<cv::Point2f, 4> graphicDestinationCorners{};
        const auto& corners = graphicCornersByFrame[static_cast<std::size_t>(frameIndex)];
        for (std::size_t corner = 0; corner < graphicDestinationCorners.size(); ++corner) {
            graphicDestinationCorners[corner] = cv::Point2f(
                static_cast<float>(corners[corner].x()),
                static_cast<float>(corners[corner].y()));
        }
        const cv::Mat graphicTransform = cv::getPerspectiveTransform(
            graphicSourceCorners.data(), graphicDestinationCorners.data());
        cv::Mat warpedGraphic(kComplexFrameHeight, kComplexFrameWidth, CV_32FC4,
                              cv::Scalar(0.0f, 0.0f, 0.0f, 0.0f));
        cv::warpPerspective(graphicSource, warpedGraphic, graphicTransform,
                            warpedGraphic.size(), cv::INTER_LINEAR,
                            cv::BORDER_CONSTANT, cv::Scalar(0.0f, 0.0f, 0.0f, 0.0f));

        cv::Mat backgroundRgba8;
        cv::cvtColor(decodedFrames[static_cast<std::size_t>(frameIndex)],
                     backgroundRgba8, cv::COLOR_BGR2RGBA);
        cv::Mat backgroundRgbaFloat;
        backgroundRgba8.convertTo(backgroundRgbaFloat, CV_32FC4, 1.0 / 255.0);
        ImageF32x4_RGBA foreground;
        foreground.setFromRGBA32F(
            reinterpret_cast<const float*>(warpedGraphic.data), kComplexFrameWidth, kComplexFrameHeight);
        ImageF32x4_RGBA composite;
        composite.setFromRGBA32F(
            reinterpret_cast<const float*>(backgroundRgbaFloat.data), kComplexFrameWidth, kComplexFrameHeight);
        composite.alphaBlend(foreground);

        if (frameIndex == kComplexFrameCount - 1) {
            const int compositeX = std::clamp(static_cast<int>(std::lround(trackedPosition.x())),
                                              0, kComplexFrameWidth - 1);
            const int compositeY = std::clamp(static_cast<int>(std::lround(trackedPosition.y())),
                                              0, kComplexFrameHeight - 1);
            const FloatRGBA compositedPixel = composite.getPixel(compositeX, compositeY);
            EXPECT_GT(compositedPixel.r(), 0.9f);
            EXPECT_LT(compositedPixel.g(), 0.1f);
            EXPECT_LT(compositedPixel.b(), 0.1f);
        }

        cv::Mat compositeRgbaFloat = composite.toCanonicalRGBA32FC4();
        cv::Mat compositeRgba8;
        compositeRgbaFloat.convertTo(compositeRgba8, CV_8UC4, 255.0);
        cv::Mat compositeBgr8;
        cv::cvtColor(compositeRgba8, compositeBgr8, cv::COLOR_RGBA2BGR);
        outputWriter.write(compositeBgr8);
    }
    outputWriter.release();

    cv::VideoCapture compositeCapture(outputPath.string());
    ASSERT_TRUE(compositeCapture.isOpened()) << "Could not reopen composite video output";
    int compositeFrameCount = 0;
    cv::Mat compositeFrame;
    while (compositeCapture.read(compositeFrame)) {
        ASSERT_FALSE(compositeFrame.empty());
        ++compositeFrameCount;
    }
    EXPECT_EQ(compositeFrameCount, kComplexFrameCount);
}

TEST(MotionTrackerVideoInputTest, KeepsInsertCornersWithinTwoPixelsAtSteepAngles) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const auto inputPath = temporaryDirectory.filePath(QStringLiteral("steep-plane.avi"));
    cv::VideoWriter inputWriter(inputPath.toStdString(),
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), kFramesPerSecond,
        cv::Size(kComplexFrameWidth, kComplexFrameHeight));
    ASSERT_TRUE(inputWriter.isOpened());
    for (int index = 0; index < kComplexFrameCount; ++index) {
        inputWriter.write(makePerspectiveTrackingFrame(index, true));
    }
    inputWriter.release();
    cv::VideoCapture input(inputPath.toStdString());
    ASSERT_TRUE(input.isOpened());
    std::array<cv::Mat, kComplexFrameCount> frames;
    MotionTracker tracker;
    auto settings = TrackerSettings::normal();
    settings.confidenceThreshold = 0.5;
    tracker.setSettings(settings);
    tracker.setTrackerType(TrackerType::Planar);
    const std::array<cv::Point2f, 4> plane = {
        cv::Point2f(0, 0), cv::Point2f(kComplexPlaneWidth - 1, 0),
        cv::Point2f(kComplexPlaneWidth - 1, kComplexPlaneHeight - 1),
        cv::Point2f(0, kComplexPlaneHeight - 1)};
    std::array<cv::Point2f, 4> localInsert = {
        cv::Point2f(140, 90), cv::Point2f(220, 90),
        cv::Point2f(220, 146), cv::Point2f(140, 146)};
    cv::Mat localMat(1, 4, CV_32FC2, localInsert.data());
    cv::Mat initial;
    cv::perspectiveTransform(localMat, initial,
        cv::getPerspectiveTransform(plane.data(), perspectiveDestinationCorners(0, true).data()));
    std::array<QPointF, 4> measured;
    for (int corner = 0; corner < 4; ++corner) {
        const auto point = initial.at<cv::Point2f>(0, corner);
        measured[corner] = QPointF(point.x, point.y);
    }
    tracker.addTrackPoint(measured[0]);
    tracker.addTrackRegion(QRectF(75, 45, 485, 280));
    for (int index = 0; index < kComplexFrameCount; ++index) {
        ASSERT_TRUE(input.read(frames[index]));
        tracker.setFrame(static_cast<double>(index) / kFramesPerSecond, frames[index]);
    }
    const bool completed = tracker.trackRange(0.0,
        static_cast<double>(kComplexFrameCount - 1) / kFramesPerSecond);
    const char* configured = std::getenv("ARTIFACT_TRACKING_COMPOSITE_OUTPUT");
    const auto outputPath = (configured ? std::filesystem::path(configured).parent_path()
                                       : std::filesystem::current_path()) /
                            "tracking-steep-angle-diagnostic.avi";
    cv::VideoWriter output(outputPath.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        kFramesPerSecond, cv::Size(kComplexFrameWidth, kComplexFrameHeight));
    ASSERT_TRUE(output.isOpened());
    double maximumError = 0.0;
    int maximumErrorFrame = 0;
    bool hasMeasurement = true;
    for (int index = 0; index < kComplexFrameCount; ++index) {
        if (index > 0) {
            const auto* sample = tracker.frameAt(static_cast<double>(index) / kFramesPerSecond);
            hasMeasurement = hasMeasurement && sample && sample->hasHomography;
            if (hasMeasurement) {
                for (auto& point : measured) {
                    QPointF projected;
                    hasMeasurement = sample->projectPoint(point, projected) && hasMeasurement;
                    point = projected;
                }
            }
        }
        cv::Mat expected;
        cv::perspectiveTransform(localMat, expected,
            cv::getPerspectiveTransform(plane.data(), perspectiveDestinationCorners(index, true).data()));
        std::array<cv::Point, 4> correctOutline;
        std::array<cv::Point, 4> measuredOutline;
        for (int corner = 0; corner < 4; ++corner) {
            const auto correct = expected.at<cv::Point2f>(0, corner);
            correctOutline[corner] = cv::Point(cvRound(correct.x), cvRound(correct.y));
            measuredOutline[corner] = cv::Point(cvRound(measured[corner].x()), cvRound(measured[corner].y()));
            if (hasMeasurement) {
                const double error = std::hypot(measured[corner].x() - correct.x,
                                                measured[corner].y() - correct.y);
                if (error > maximumError) { maximumError = error; maximumErrorFrame = index; }
            }
        }
        cv::Mat display = frames[index].clone();
        for (int corner = 0; corner < 4; ++corner) {
            cv::line(display, correctOutline[corner], correctOutline[(corner + 1) % 4],
                     cv::Scalar(0, 255, 0), 3);
            if (hasMeasurement) cv::line(display, measuredOutline[corner], measuredOutline[(corner + 1) % 4],
                                         cv::Scalar(0, 0, 255), 1);
        }
        cv::putText(display, hasMeasurement ? "GREEN: truth  RED: tracked" : "TRACK LOST",
                    cv::Point(12, 25), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1);
        output.write(display);
    }
    output.release();
    EXPECT_TRUE(completed) << "Steep-angle tracking stopped before the final frame";
    EXPECT_TRUE(hasMeasurement);
    EXPECT_LE(maximumError, 2.0) << "Maximum insert-corner error at frame " << maximumErrorFrame;
}

TEST(MotionTrackerVideoInputTest, TracksCC0DroneFootageAndWritesTrackedComposite) {
    const std::filesystem::path fixturePath =
        std::filesystem::path(__FILE__).parent_path() / "fixtures" / "walkway-orbit-cc0.mp4";
    ASSERT_TRUE(std::filesystem::exists(fixturePath)) << fixturePath.string();

    cv::VideoCapture capture(fixturePath.string());
    ASSERT_TRUE(capture.isOpened()) << "Could not open CC0 tracking footage: "
                                    << fixturePath.string();

    MotionTracker tracker;
    auto settings = TrackerSettings::normal();
    settings.method = TrackingMethod::OpticalFlow;
    settings.confidenceThreshold = 0.5;
    settings.maxPyramidLevel = 3;
    tracker.setSettings(settings);
    tracker.setTrackerType(TrackerType::Planar);
    const QPointF insertAnchor(180.0, 330.0);
    tracker.addTrackPoint(insertAnchor);
    // Keep the correlation mask close to the insert and on the bridge truss;
    // the earlier wide ROI mixed the bridge, river, and distant shoreline.
    tracker.addTrackRegion(QRectF(130.0, 295.0, 100.0, 70.0));

    std::vector<cv::Mat> decodedFrames;
    cv::Mat frame;
    while (capture.read(frame)) {
        ASSERT_FALSE(frame.empty());
        ASSERT_EQ(frame.cols, kRealFootageWidth);
        ASSERT_EQ(frame.rows, kRealFootageHeight);
        const double time = static_cast<double>(decodedFrames.size()) / 15.0;
        tracker.setFrame(time, frame);
        decodedFrames.push_back(frame.clone());
    }
    ASSERT_GE(decodedFrames.size(), 40u);

    const double finalTime = static_cast<double>(decodedFrames.size() - 1) / 15.0;
    ASSERT_TRUE(tracker.trackRange(0.0, finalTime));
    const TrackResult result = tracker.result();
    ASSERT_EQ(result.frameCount(), decodedFrames.size());
    ASSERT_GT(tracker.averageConfidence(), 0.0);

    std::array<QPointF, 4> trackedGraphicCorners = {
        QPointF(130.0, 295.0), QPointF(230.0, 295.0),
        QPointF(230.0, 365.0), QPointF(130.0, 365.0)};
    std::vector<std::array<QPointF, 4>> graphicCornersByFrame(decodedFrames.size());
    graphicCornersByFrame[0] = {trackedGraphicCorners[0], trackedGraphicCorners[1],
                                 trackedGraphicCorners[2], trackedGraphicCorners[3]};
    std::size_t homographyFrameCount = 0;
    double minimumHomographyConfidence = 1.0;
    for (std::size_t frameIndex = 1; frameIndex < decodedFrames.size(); ++frameIndex) {
        const double time = static_cast<double>(frameIndex) / 15.0;
        const TrackFrame* trackedFrame = tracker.frameAt(time);
        ASSERT_NE(trackedFrame, nullptr);
        if (!trackedFrame->hasHomography) {
            graphicCornersByFrame[frameIndex] = graphicCornersByFrame[frameIndex - 1];
            continue;
        }
        ++homographyFrameCount;
        minimumHomographyConfidence = std::min(minimumHomographyConfidence,
                                                trackedFrame->overallConfidence);
        for (QPointF& corner : trackedGraphicCorners) {
            QPointF projectedCorner;
            ASSERT_TRUE(trackedFrame->projectPoint(corner, projectedCorner));
            corner = projectedCorner;
        }
        graphicCornersByFrame[frameIndex] = {trackedGraphicCorners[0], trackedGraphicCorners[1],
                                             trackedGraphicCorners[2], trackedGraphicCorners[3]};
    }
    EXPECT_GE(homographyFrameCount, decodedFrames.size() * 9 / 10);
    EXPECT_GE(minimumHomographyConfidence, settings.confidenceThreshold);

    constexpr int graphicWidth = 100;
    constexpr int graphicHeight = 70;
    cv::Mat graphic(graphicHeight, graphicWidth, CV_32FC4, cv::Scalar(0.95f, 0.12f, 0.03f, 1.0f));
    cv::rectangle(graphic, cv::Rect(0, 0, graphicWidth, graphicHeight),
                  cv::Scalar(1.0f, 1.0f, 1.0f, 1.0f), 4);
    const std::array<cv::Point2f, 4> graphicSourceCorners = {
        cv::Point2f(0.0f, 0.0f), cv::Point2f(graphicWidth - 1.0f, 0.0f),
        cv::Point2f(graphicWidth - 1.0f, graphicHeight - 1.0f),
        cv::Point2f(0.0f, graphicHeight - 1.0f)};
    const char* configuredOutputPath = std::getenv("ARTIFACT_TRACKING_COMPOSITE_OUTPUT");
    const std::filesystem::path outputPath = configuredOutputPath
        ? std::filesystem::path(configuredOutputPath).parent_path() / "tracking-composite-real-footage.avi"
        : std::filesystem::current_path() / "tracking-composite-real-footage.avi";
    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    cv::VideoWriter outputWriter(outputPath.string(),
                                 cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15.0,
                                 cv::Size(kRealFootageWidth, kRealFootageHeight), true);
    ASSERT_TRUE(outputWriter.isOpened()) << "Could not create real-footage composite: "
                                        << outputPath.string();

    for (std::size_t frameIndex = 0; frameIndex < decodedFrames.size(); ++frameIndex) {
        std::array<cv::Point2f, 4> destinationCorners{};
        for (std::size_t corner = 0; corner < destinationCorners.size(); ++corner) {
            const QPointF point = graphicCornersByFrame[frameIndex][corner];
            destinationCorners[corner] = cv::Point2f(static_cast<float>(point.x()),
                                                      static_cast<float>(point.y()));
        }
        const cv::Mat transform = cv::getPerspectiveTransform(graphicSourceCorners.data(),
                                                               destinationCorners.data());
        cv::Mat warped(kRealFootageHeight, kRealFootageWidth, CV_32FC4,
                       cv::Scalar(0.0f, 0.0f, 0.0f, 0.0f));
        cv::warpPerspective(graphic, warped, transform, warped.size(), cv::INTER_LINEAR,
                            cv::BORDER_CONSTANT, cv::Scalar(0.0f, 0.0f, 0.0f, 0.0f));

        cv::Mat backgroundRgba8;
        cv::cvtColor(decodedFrames[frameIndex], backgroundRgba8, cv::COLOR_BGR2RGBA);
        cv::Mat backgroundRgbaFloat;
        backgroundRgba8.convertTo(backgroundRgbaFloat, CV_32FC4, 1.0 / 255.0);
        ImageF32x4_RGBA foreground;
        foreground.setFromRGBA32F(reinterpret_cast<const float*>(warped.data),
                                  kRealFootageWidth, kRealFootageHeight);
        ImageF32x4_RGBA composite;
        composite.setFromRGBA32F(reinterpret_cast<const float*>(backgroundRgbaFloat.data),
                                 kRealFootageWidth, kRealFootageHeight);
        composite.alphaBlend(foreground);
        cv::Mat rgbaFloat = composite.toCanonicalRGBA32FC4();
        cv::Mat rgba8;
        rgbaFloat.convertTo(rgba8, CV_8UC4, 255.0);
        cv::Mat bgr8;
        cv::cvtColor(rgba8, bgr8, cv::COLOR_RGBA2BGR);
        outputWriter.write(bgr8);
    }
    outputWriter.release();

    ASSERT_TRUE(std::filesystem::exists(outputPath));
    cv::VideoCapture compositeCapture(outputPath.string());
    ASSERT_TRUE(compositeCapture.isOpened());
    EXPECT_EQ(static_cast<int>(compositeCapture.get(cv::CAP_PROP_FRAME_COUNT)),
              static_cast<int>(decodedFrames.size()));
}
