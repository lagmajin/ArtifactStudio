#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include <vector>

import Core.Mask.VolumeMask;

using namespace ArtifactCore;

TEST(VolumeMaskContractTest, DistanceFieldUsesPositiveInsideAndNegativeOutside)
{
    const std::vector<cv::Point> contour{
        cv::Point(2, 2), cv::Point(7, 2), cv::Point(7, 7), cv::Point(2, 7),
    };

    const cv::Mat distance = VolumeMaskGenerator::generateDistanceField(
        contour, 10, 10);

    ASSERT_EQ(distance.type(), CV_32FC1);
    ASSERT_EQ(distance.rows, 10);
    ASSERT_EQ(distance.cols, 10);
    EXPECT_GT(distance.at<float>(4, 4), 0.0f);
    EXPECT_LT(distance.at<float>(0, 0), 0.0f);
}

TEST(VolumeMaskContractTest, DistanceFieldIsInvariantToContourWinding)
{
    const std::vector<cv::Point> clockwise{
        cv::Point(2, 2), cv::Point(7, 2), cv::Point(7, 7), cv::Point(2, 7),
    };
    const std::vector<cv::Point> counterClockwise{
        cv::Point(2, 7), cv::Point(7, 7), cv::Point(7, 2), cv::Point(2, 2),
    };

    const cv::Mat clockwiseDistance = VolumeMaskGenerator::generateDistanceField(
        clockwise, 10, 10);
    const cv::Mat counterClockwiseDistance = VolumeMaskGenerator::generateDistanceField(
        counterClockwise, 10, 10);

    ASSERT_EQ(clockwiseDistance.size(), counterClockwiseDistance.size());
    EXPECT_EQ(cv::norm(clockwiseDistance, counterClockwiseDistance, cv::NORM_INF),
              0.0);
}

TEST(VolumeMaskContractTest, FalloffCurvesScaleInteriorDensityAndKeepOutsideTransparent)
{
    cv::Mat distance(1, 4, CV_32FC1);
    distance.at<float>(0, 0) = -1.0f;
    distance.at<float>(0, 1) = 0.0f;
    distance.at<float>(0, 2) = 2.5f;
    distance.at<float>(0, 3) = 5.0f;

    VolumeMaskSettings settings;
    settings.thickness = 5.0f;
    settings.density = 0.8f;
    settings.falloffCurve = VolumeFalloffCurve::Linear;
    const cv::Mat linear = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    ASSERT_EQ(linear.type(), CV_32FC1);
    EXPECT_FLOAT_EQ(linear.at<float>(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(linear.at<float>(0, 1), 0.0f);
    EXPECT_FLOAT_EQ(linear.at<float>(0, 2), 0.4f);
    EXPECT_FLOAT_EQ(linear.at<float>(0, 3), 0.8f);

    settings.falloffCurve = VolumeFalloffCurve::EaseIn;
    const cv::Mat easeIn = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    EXPECT_FLOAT_EQ(easeIn.at<float>(0, 2), 0.2f);

    settings.falloffCurve = VolumeFalloffCurve::EaseOut;
    const cv::Mat easeOut = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    EXPECT_FLOAT_EQ(easeOut.at<float>(0, 2), 0.6f);

    settings.falloffCurve = VolumeFalloffCurve::Step;
    const cv::Mat step = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    EXPECT_FLOAT_EQ(step.at<float>(0, 1), 0.8f);
    EXPECT_FLOAT_EQ(step.at<float>(0, 2), 0.8f);
}

TEST(VolumeMaskContractTest, DensityIsClampedToUnitInterval)
{
    cv::Mat distance(1, 2, CV_32FC1);
    distance.at<float>(0, 0) = 1.0f;
    distance.at<float>(0, 1) = 1.0f;
    VolumeMaskSettings settings;
    settings.thickness = 1.0f;
    settings.falloffCurve = VolumeFalloffCurve::Linear;

    settings.density = 1.5f;
    const cv::Mat aboveOne = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    EXPECT_FLOAT_EQ(aboveOne.at<float>(0, 0), 1.0f);

    settings.density = -0.5f;
    const cv::Mat belowZero = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    EXPECT_FLOAT_EQ(belowZero.at<float>(0, 1), 0.0f);
}

TEST(VolumeMaskContractTest, InvertComplementsTheComputedMaskAndRenderRejectsOpenContours)
{
    cv::Mat distance(1, 2, CV_32FC1);
    distance.at<float>(0, 0) = -1.0f;
    distance.at<float>(0, 1) = 2.0f;
    VolumeMaskSettings settings;
    settings.thickness = 4.0f;
    settings.density = 0.75f;
    settings.falloffCurve = VolumeFalloffCurve::Linear;

    const cv::Mat regular = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    settings.invert = true;
    const cv::Mat inverted = VolumeMaskGenerator::computeAlphaMask(distance, settings);
    EXPECT_FLOAT_EQ(inverted.at<float>(0, 0), 1.0f - regular.at<float>(0, 0));
    EXPECT_FLOAT_EQ(inverted.at<float>(0, 1), 1.0f - regular.at<float>(0, 1));

    cv::Mat output;
    VolumeMaskGenerator::renderToAlpha(
        {cv::Point(0, 0), cv::Point(3, 3)}, 4, 4, &output, settings);
    ASSERT_EQ(output.type(), CV_32FC1);
    ASSERT_EQ(output.rows, 4);
    ASSERT_EQ(output.cols, 4);
    EXPECT_EQ(cv::countNonZero(output), 0);
}

TEST(VolumeMaskContractTest, RenderReturnsTransparentMaskForContoursWithFewerThanThreePoints)
{
    const std::vector<std::vector<cv::Point>> contours{
        {},
        {cv::Point(1, 1)},
        {cv::Point(0, 0), cv::Point(3, 3)},
    };
    VolumeMaskSettings settings;
    settings.invert = true;

    for (const auto& contour : contours) {
        cv::Mat output;
        VolumeMaskGenerator::renderToAlpha(contour, 4, 3, &output, settings);

        ASSERT_EQ(output.type(), CV_32FC1);
        ASSERT_EQ(output.rows, 3);
        ASSERT_EQ(output.cols, 4);
        EXPECT_EQ(cv::countNonZero(output), 0);
    }
}

TEST(VolumeMaskContractTest, InvertMakesZeroDistanceBoundaryOpaque)
{
    cv::Mat distance(1, 1, CV_32FC1);
    distance.at<float>(0, 0) = 0.0f;

    VolumeMaskSettings settings;
    settings.thickness = 4.0f;
    settings.density = 0.35f;
    settings.falloffCurve = VolumeFalloffCurve::EaseOut;
    settings.invert = true;

    const cv::Mat alpha = VolumeMaskGenerator::computeAlphaMask(distance, settings);

    ASSERT_EQ(alpha.type(), CV_32FC1);
    EXPECT_FLOAT_EQ(alpha.at<float>(0, 0), 1.0f);
}

TEST(VolumeMaskContractTest, ClosedContourRenderComposesDistanceFalloffAndInversion)
{
    const std::vector<cv::Point> contour{
        cv::Point(2, 2), cv::Point(7, 2), cv::Point(7, 7), cv::Point(2, 7),
    };
    VolumeMaskSettings settings;
    settings.thickness = 3.0f;
    settings.density = 0.8f;
    settings.falloffCurve = VolumeFalloffCurve::Linear;
    settings.invert = true;

    cv::Mat output;
    VolumeMaskGenerator::renderToAlpha(contour, 10, 10, &output, settings);

    ASSERT_EQ(output.type(), CV_32FC1);
    ASSERT_EQ(output.rows, 10);
    ASSERT_EQ(output.cols, 10);
    EXPECT_FLOAT_EQ(output.at<float>(4, 4), 0.2f);
    EXPECT_NEAR(output.at<float>(4, 2), 1.0f - 0.8f / 3.0f, 1.0e-6f);
    EXPECT_FLOAT_EQ(output.at<float>(0, 0), 1.0f);
}
