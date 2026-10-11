#include <gtest/gtest.h>

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QTransform>

#include <cmath>
#include <initializer_list>

import Geometry.ResolutionRemap;

using namespace ArtifactCore;

namespace {
const QSize kOldSize{200, 100};
const QSize kLandscapeToPortrait{100, 200};
}

TEST(ResolutionRemapStandaloneTest, TopLeftLockedScalesPositionOnEachAxis)
{
    const QPointF result = ResolutionRemap::remapPosition(
        {150.0, 75.0}, kOldSize, QSize(400, 300), RemapPolicy::TopLeftLocked);
    EXPECT_DOUBLE_EQ(result.x(), 300.0);
    EXPECT_DOUBLE_EQ(result.y(), 225.0);
}

TEST(ResolutionRemapStandaloneTest, StretchToFitUsesIndependentAxisScales)
{
    const QPointF result = ResolutionRemap::remapPosition(
        {30.0, 20.0}, kOldSize, QSize(400, 300), RemapPolicy::StretchToFit);
    EXPECT_DOUBLE_EQ(result.x(), 60.0);
    EXPECT_DOUBLE_EQ(result.y(), 60.0);
}

TEST(ResolutionRemapStandaloneTest, CenterLockedPreservesOffsetFromCanvasCenter)
{
    const QPointF result = ResolutionRemap::remapPosition(
        {150.0, 75.0}, kOldSize, QSize(400, 300), RemapPolicy::CenterLocked);
    EXPECT_DOUBLE_EQ(result.x(), 300.0);
    EXPECT_DOUBLE_EQ(result.y(), 225.0);
}

TEST(ResolutionRemapStandaloneTest, FitWithPaddingCentersTheScaledContent)
{
    const QPointF result = ResolutionRemap::remapPosition(
        {20.0, 10.0}, kOldSize, kLandscapeToPortrait, RemapPolicy::FitWithPadding);
    EXPECT_DOUBLE_EQ(result.x(), 10.0);
    EXPECT_DOUBLE_EQ(result.y(), 80.0);
}

TEST(ResolutionRemapStandaloneTest, FitWithCropOffsetsTheOversizedContent)
{
    const QPointF result = ResolutionRemap::remapPosition(
        {20.0, 10.0}, kOldSize, kLandscapeToPortrait, RemapPolicy::FitWithCrop);
    EXPECT_DOUBLE_EQ(result.x(), -110.0);
    EXPECT_DOUBLE_EQ(result.y(), 20.0);
}

TEST(ResolutionRemapStandaloneTest, IdenticalAndEmptySizesLeavePositionUnchanged)
{
    const QPointF original(37.5, -12.0);
    EXPECT_EQ(ResolutionRemap::remapPosition(
        original, kOldSize, kOldSize, RemapPolicy::FitWithCrop), original);
    EXPECT_EQ(ResolutionRemap::remapPosition(
        original, QSize(), QSize(400, 200), RemapPolicy::StretchToFit), original);
    EXPECT_EQ(ResolutionRemap::remapPosition(
        original, kOldSize, QSize(0, 200), RemapPolicy::CenterLocked), original);
}

TEST(ResolutionRemapStandaloneTest, RemapBoundsTransformsBothCorners)
{
    const QRectF result = ResolutionRemap::remapBounds(
        QRectF(20.0, 10.0, 30.0, 20.0), kOldSize,
        QSize(400, 300), RemapPolicy::StretchToFit);
    EXPECT_DOUBLE_EQ(result.x(), 40.0);
    EXPECT_DOUBLE_EQ(result.y(), 30.0);
    EXPECT_DOUBLE_EQ(result.width(), 60.0);
    EXPECT_DOUBLE_EQ(result.height(), 60.0);
}

TEST(ResolutionRemapStandaloneTest, RemapBoundsNormalizesNegativeWidthAndHeight)
{
    const QRectF result = ResolutionRemap::remapBounds(
        QRectF(80.0, 40.0, -20.0, -10.0), kOldSize,
        QSize(400, 200), RemapPolicy::TopLeftLocked);
    EXPECT_DOUBLE_EQ(result.x(), 120.0);
    EXPECT_DOUBLE_EQ(result.y(), 60.0);
    EXPECT_DOUBLE_EQ(result.width(), 40.0);
    EXPECT_DOUBLE_EQ(result.height(), 20.0);
}

TEST(ResolutionRemapStandaloneTest, OrdinaryScaleUsesGeometricMeanOfAxisScale)
{
    EXPECT_NEAR(ResolutionRemap::remapScale(
        4.0, kOldSize, QSize(400, 300), RemapPolicy::StretchToFit),
        4.0 * std::sqrt(6.0), 1e-12);
}

TEST(ResolutionRemapStandaloneTest, FitPoliciesUseTheirUniformFitScale)
{
    EXPECT_DOUBLE_EQ(ResolutionRemap::remapScale(
        4.0, kOldSize, kLandscapeToPortrait, RemapPolicy::FitWithPadding), 2.0);
    EXPECT_DOUBLE_EQ(ResolutionRemap::remapScale(
        4.0, kOldSize, kLandscapeToPortrait, RemapPolicy::FitWithCrop), 8.0);
}

TEST(ResolutionRemapStandaloneTest, ScaleRemainsUnchangedForIdenticalResolution)
{
    EXPECT_DOUBLE_EQ(ResolutionRemap::remapScale(
        2.75, kOldSize, kOldSize, RemapPolicy::CenterLocked), 2.75);
}

TEST(ResolutionRemapStandaloneTest, ImpactWithoutContentFlagsIsFalse)
{
    const auto impact = ResolutionRemap::calculateImpact(
        QSize(1920, 1080), QSize(1280, 720), false, false, false);
    EXPECT_FALSE(impact.hasImpact());
    EXPECT_FALSE(impact.hasMaskPaths);
    EXPECT_FALSE(impact.hasAnchorPoints);
    EXPECT_EQ(impact.maskVertexCount, 0);
    EXPECT_EQ(impact.keyframeCount, 0);
}

TEST(ResolutionRemapStandaloneTest, ImpactFlagsReportMaskKeyframeAndAnchorPresence)
{
    const auto impact = ResolutionRemap::calculateImpact(
        QSize(1920, 1080), QSize(1080, 1920), true, true, true);
    EXPECT_TRUE(impact.hasImpact());
    EXPECT_TRUE(impact.hasMaskPaths);
    EXPECT_TRUE(impact.hasAnchorPoints);
    EXPECT_EQ(impact.maskVertexCount, 1);
    EXPECT_EQ(impact.keyframeCount, 1);
    EXPECT_DOUBLE_EQ(impact.oldAspectRatio, 16.0 / 9.0);
    EXPECT_DOUBLE_EQ(impact.newAspectRatio, 9.0 / 16.0);
}

TEST(ResolutionRemapStandaloneTest, ImpactUsesSafeAspectForZeroHeight)
{
    const auto impact = ResolutionRemap::calculateImpact(
        QSize(1920, 0), QSize(0, 1080), false, false, false);
    EXPECT_DOUBLE_EQ(impact.oldAspectRatio, 1.0);
    EXPECT_DOUBLE_EQ(impact.newAspectRatio, 0.0);
}

TEST(ResolutionRemapStandaloneTest, TransformMatchesPositionMappingForEachPolicy)
{
    const QPointF point(30.0, 20.0);
    for (const auto policy : {RemapPolicy::CenterLocked, RemapPolicy::TopLeftLocked,
                              RemapPolicy::StretchToFit, RemapPolicy::FitWithPadding,
                              RemapPolicy::FitWithCrop}) {
        const auto expected = ResolutionRemap::remapPosition(
            point, kOldSize, kLandscapeToPortrait, policy);
        const auto transformed = ResolutionRemap::remapTransform(
            kOldSize, kLandscapeToPortrait, policy).map(point);
        EXPECT_NEAR(transformed.x(), expected.x(), 1e-9);
        EXPECT_NEAR(transformed.y(), expected.y(), 1e-9);
    }
}

TEST(ResolutionRemapStandaloneTest, EmptySizeProducesIdentityTransform)
{
    const QTransform transform = ResolutionRemap::remapTransform(
        kOldSize, QSize(), RemapPolicy::FitWithPadding);
    EXPECT_EQ(transform.map(QPointF(12.0, 34.0)), QPointF(12.0, 34.0));
}
