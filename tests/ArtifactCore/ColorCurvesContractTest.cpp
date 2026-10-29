#include <gtest/gtest.h>

#include <QColor>
#include <QImage>

import Color.Grading.ColorCurves;

namespace {

using ArtifactCore::ColorCurve;
using ArtifactCore::ColorCurves;
using ArtifactCore::CurvePoint;

} // namespace

TEST(ColorCurvesContractTest, SortsPointsAndEvaluatesPiecewiseLinearSegments)
{
    ColorCurve curve;
    curve.setPoints({
        CurvePoint{.input = 1.0f, .output = 1.0f},
        CurvePoint{.input = 0.5f, .output = 0.25f},
        CurvePoint{.input = 0.0f, .output = 0.0f},
    });

    ASSERT_EQ(curve.points().size(), 3u);
    EXPECT_FLOAT_EQ(curve.points()[0].input, 0.0f);
    EXPECT_FLOAT_EQ(curve.points()[1].input, 0.5f);
    EXPECT_FLOAT_EQ(curve.points()[2].input, 1.0f);
    EXPECT_FLOAT_EQ(curve.evaluate(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(curve.evaluate(0.25f), 0.125f);
    EXPECT_FLOAT_EQ(curve.evaluate(0.5f), 0.25f);
    EXPECT_FLOAT_EQ(curve.evaluate(0.75f), 0.625f);
    EXPECT_FLOAT_EQ(curve.evaluate(2.0f), 1.0f);
}

TEST(ColorCurvesContractTest, SmoothPointsUseCubicTangentsAndLutClampsOutputs)
{
    ColorCurve curve;
    curve.setPoints({
        CurvePoint{.input = 0.0f, .output = 0.0f, .smooth = true},
        CurvePoint{.input = 1.0f, .output = 1.0f, .smooth = true},
    });
    EXPECT_NEAR(curve.evaluate(0.25f), 0.15625f, 1.0e-6f);
    EXPECT_FLOAT_EQ(curve.evaluate(0.5f), 0.5f);
    EXPECT_NEAR(curve.evaluate(0.75f), 0.84375f, 1.0e-6f);

    curve.setPoints({
        CurvePoint{.input = 0.0f, .output = -1.0f},
        CurvePoint{.input = 1.0f, .output = 2.0f},
    });
    const auto lut = curve.generateLUT();
    EXPECT_EQ(lut.front(), 0);
    EXPECT_EQ(lut.back(), 255);
    EXPECT_NEAR(lut[128], 128, 1);
}

TEST(ColorCurvesContractTest, ProtectsMinimumPointCountAndResetRestoresIdentity)
{
    ColorCurve curve;
    curve.addPoint(0.5f, 0.8f);
    ASSERT_EQ(curve.points().size(), 3u);
    curve.removePoint(1);
    ASSERT_EQ(curve.points().size(), 2u);
    curve.removePoint(0);
    curve.removePoint(-1);
    EXPECT_EQ(curve.points().size(), 2u);

    curve.reset();
    EXPECT_EQ(curve.points().size(), 2u);
    EXPECT_FLOAT_EQ(curve.evaluate(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(curve.evaluate(0.37f), 0.37f);
    EXPECT_FLOAT_EQ(curve.evaluate(1.0f), 1.0f);
}

TEST(ColorCurvesContractTest, MasterOnlyAndPerChannelModesPreserveAlpha)
{
    ColorCurves curves;
    curves.master().setPoints({
        CurvePoint{.input = 0.0f, .output = 1.0f},
        CurvePoint{.input = 1.0f, .output = 0.0f},
    });

    QImage input(1, 1, QImage::Format_RGBA8888);
    input.setPixelColor(0, 0, QColor(10, 80, 200, 77));
    const QImage masterOnly = curves.apply(input);
    EXPECT_EQ(masterOnly.pixelColor(0, 0), QColor(245, 175, 55, 77));

    curves.red().setPoints({
        CurvePoint{.input = 0.0f, .output = 1.0f},
        CurvePoint{.input = 1.0f, .output = 0.0f},
    });
    curves.setMasterOnly(false);
    const QImage perChannel = curves.apply(input);
    EXPECT_EQ(perChannel.pixelColor(0, 0), QColor(10, 175, 55, 77));
}
