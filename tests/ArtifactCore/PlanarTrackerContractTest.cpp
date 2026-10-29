#include <gtest/gtest.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <utility>

import ArtifactCore.Tracking.PlanarTracker;

namespace {

cv::Mat makeTexturedFrame()
{
    cv::Mat image(192, 256, CV_8UC1);
    cv::RNG random(0x5a17c0de);
    random.fill(image, cv::RNG::UNIFORM, 0, 256);
    return image;
}

cv::Mat translated(const cv::Mat& image)
{
    const cv::Mat transform = (cv::Mat_<double>(2, 3) <<
        1.0, 0.0, 6.0,
        0.0, 1.0, -3.0);
    cv::Mat result;
    cv::warpAffine(image, result, transform, image.size(), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar(0));
    return result;
}

std::array<cv::Point2f, 4> referenceCorners()
{
    return {{{40.0f, 40.0f}, {216.0f, 40.0f},
             {216.0f, 152.0f}, {40.0f, 152.0f}}};
}

void expectTranslation(const ArtifactCore::PlanarTrackResult& result)
{
    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.homography.rows, 3);
    ASSERT_EQ(result.homography.cols, 3);
    EXPECT_EQ(result.homography.type(), CV_64F);
    EXPECT_GE(result.inlierCount, 4);
    EXPECT_GE(result.confidence, 0.15);
    EXPECT_LE(result.confidence, 1.0);
    const auto source = referenceCorners();
    for (std::size_t index = 0; index < source.size(); ++index) {
        SCOPED_TRACE(index);
        EXPECT_NEAR(result.corners[index].x, source[index].x + 6.0f, 0.75f);
        EXPECT_NEAR(result.corners[index].y, source[index].y - 3.0f, 0.75f);
    }
}

} // namespace

TEST(PlanarTrackerContractTest,
     EstimatesTranslationAndProjectsAllReferenceCorners)
{
    const cv::Mat previous = makeTexturedFrame();
    const cv::Mat current = translated(previous);
    const ArtifactCore::PlanarTracker tracker;

    const auto result = tracker.track(previous, current, referenceCorners());

    expectTranslation(result);
}

TEST(PlanarTrackerContractTest,
     AcceptsSupportedGrayBgrAndBgraFrameFormats)
{
    const cv::Mat previousGray = makeTexturedFrame();
    const cv::Mat currentGray = translated(previousGray);
    const std::array<std::pair<int, int>, 2> colorConversions{{
        {cv::COLOR_GRAY2BGR, 3}, {cv::COLOR_GRAY2BGRA, 4}}};
    const ArtifactCore::PlanarTracker tracker;

    expectTranslation(tracker.track(previousGray, currentGray, referenceCorners()));
    for (const auto& conversion : colorConversions) {
        cv::Mat previousColor;
        cv::Mat currentColor;
        cv::cvtColor(previousGray, previousColor, conversion.first);
        cv::cvtColor(currentGray, currentColor, conversion.first);
        ASSERT_EQ(previousColor.channels(), conversion.second);
        ASSERT_EQ(currentColor.channels(), conversion.second);
        expectTranslation(tracker.track(previousColor, currentColor,
                                        referenceCorners()));
    }
}

TEST(PlanarTrackerContractTest,
     InvalidFramesCornersAndThresholdsReturnAnInvalidEmptyResult)
{
    const cv::Mat validFrame = makeTexturedFrame();
    const cv::Mat current = translated(validFrame);
    const cv::Mat empty;
    const cv::Mat differentSize(128, 128, CV_8UC1, cv::Scalar(0));
    const cv::Mat unsupportedChannels(192, 256, CV_8UC2, cv::Scalar(0, 0));
    const ArtifactCore::PlanarTracker tracker;
    const auto corners = referenceCorners();
    auto nonFiniteCorners = corners;
    nonFiniteCorners[2].x = std::numeric_limits<float>::quiet_NaN();

    const std::array<ArtifactCore::PlanarTrackResult, 7> invalidResults{
        tracker.track(empty, current, corners),
        tracker.track(validFrame, differentSize, corners),
        tracker.track(unsupportedChannels, unsupportedChannels, corners),
        tracker.track(validFrame, current, nonFiniteCorners),
        tracker.track(validFrame, current, corners, 0.0),
        tracker.track(validFrame, current, corners, -1.0),
        tracker.track(validFrame, current, corners,
                      std::numeric_limits<double>::infinity()),
    };
    for (const auto& result : invalidResults) {
        EXPECT_FALSE(result.valid);
        EXPECT_TRUE(result.homography.empty());
        EXPECT_EQ(result.inlierCount, 0);
        EXPECT_DOUBLE_EQ(result.confidence, 0.0);
    }
}
