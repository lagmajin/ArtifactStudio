#include <gtest/gtest.h>

#include <QMatrix4x4>
#include <QVector3D>

import Core.Mask.DepthMask;

using namespace ArtifactCore;

namespace {

float alphaAtCameraDepth(float depth, const DepthMaskSettings& settings,
                         const QMatrix4x4& view = QMatrix4x4())
{
    return DepthMaskCalculator::computeAlpha(
        QVector3D(0.0f, 0.0f, -depth), view, QMatrix4x4(), settings);
}

} // namespace

TEST(DepthMaskContractTest, LinearFalloffClampsAtNearAndFarAndInterpolatesInside)
{
    DepthMaskSettings settings;
    settings.nearDistance = 2.0f;
    settings.farDistance = 10.0f;
    settings.falloff = DepthFalloffCurve::Linear;

    EXPECT_FLOAT_EQ(alphaAtCameraDepth(0.0f, settings), 0.0f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(2.0f, settings), 0.0f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(6.0f, settings), 0.5f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(10.0f, settings), 1.0f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(20.0f, settings), 1.0f);
}

TEST(DepthMaskContractTest, FalloffCurvesHaveDistinctMidpointSemantics)
{
    DepthMaskSettings settings;
    settings.nearDistance = 0.0f;
    settings.farDistance = 10.0f;

    settings.falloff = DepthFalloffCurve::Hard;
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(4.9f, settings), 0.0f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(5.0f, settings), 1.0f);

    settings.falloff = DepthFalloffCurve::Smooth;
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(2.5f, settings), 0.15625f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(5.0f, settings), 0.5f);
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(7.5f, settings), 0.84375f);
}

TEST(DepthMaskContractTest, ViewTransformControlsCameraDepthAndInvertComplementsAlpha)
{
    DepthMaskSettings settings;
    settings.nearDistance = 0.0f;
    settings.farDistance = 10.0f;
    settings.falloff = DepthFalloffCurve::Linear;
    QMatrix4x4 view;
    view.setToIdentity();
    view.translate(0.0f, 0.0f, -2.0f);

    const float regular = alphaAtCameraDepth(4.0f, settings, view);
    EXPECT_FLOAT_EQ(regular, 0.6f);
    settings.invert = true;
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(4.0f, settings, view), 1.0f - regular);
}

TEST(DepthMaskContractTest, NonIncreasingDistanceRangeReturnsInvertDependentFallback)
{
    DepthMaskSettings settings;
    settings.nearDistance = 5.0f;
    settings.farDistance = 5.0f;
    settings.falloff = DepthFalloffCurve::Linear;

    EXPECT_FLOAT_EQ(alphaAtCameraDepth(5.0f, settings), 0.0f);
    settings.farDistance = 4.0f;
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(100.0f, settings), 0.0f);
    settings.invert = true;
    EXPECT_FLOAT_EQ(alphaAtCameraDepth(-100.0f, settings), 1.0f);
}
