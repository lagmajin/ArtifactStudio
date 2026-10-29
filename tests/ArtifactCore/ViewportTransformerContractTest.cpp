#include <gtest/gtest.h>

#include <DiligentCore/Common/interface/BasicMath.hpp>

#include <array>
#include <cmath>

import Core.Transform.Viewport;

namespace {

using ArtifactCore::ViewportTransformer;
using Diligent::float2;

void expectNear(float actual, float expected, float tolerance = 1.0e-3f)
{
    EXPECT_NEAR(actual, expected, tolerance);
}

} // namespace

TEST(ViewportTransformerContractTest, CanvasAndViewportCoordinatesAreInverseWithRotation)
{
    ViewportTransformer view;
    view.SetViewportSize(1280.0f, 720.0f);
    view.SetCanvasSize(640.0f, 360.0f);
    view.SetPan(-32.0f, 44.0f);
    view.SetZoom(1.75f);
    view.SetRotation(37.0f);

    const std::array<float2, 5> points = {
        float2{0.0f, 0.0f}, float2{640.0f, 0.0f},
        float2{0.0f, 360.0f}, float2{640.0f, 360.0f},
        float2{271.5f, 184.25f},
    };
    for (const auto& point : points) {
        const float2 roundTrip = view.ViewportToCanvas(view.CanvasToViewport(point));
        expectNear(roundTrip.x, point.x);
        expectNear(roundTrip.y, point.y);
    }
}

TEST(ViewportTransformerContractTest, ZoomAroundViewportPointKeepsCanvasPointAnchored)
{
    ViewportTransformer view;
    view.SetCanvasSize(1920.0f, 1080.0f);
    view.SetViewportSize(1280.0f, 720.0f);
    view.SetPan(-240.0f, 85.0f);
    view.SetZoom(0.6f);
    view.SetRotation(-23.0f);

    const float2 anchor{314.0f, 201.0f};
    const float2 canvasPoint = view.ViewportToCanvas(anchor);
    view.ZoomAroundViewportPoint(anchor, 2.3f);

    const float2 movedAnchor = view.CanvasToViewport(canvasPoint);
    expectNear(movedAnchor.x, anchor.x);
    expectNear(movedAnchor.y, anchor.y);
    expectNear(view.GetZoom(), 2.3f);
}

TEST(ViewportTransformerContractTest, FitAndFillUseExpectedUniformScaleAndCentering)
{
    ViewportTransformer view;
    view.SetCanvasSize(800.0f, 600.0f);
    view.SetViewportSize(1000.0f, 700.0f);

    view.FitCanvasToViewport(50.0f);
    expectNear(view.GetZoom(), 1.0f);
    expectNear(view.GetPan().x, 100.0f);
    expectNear(view.GetPan().y, 50.0f);

    view.FillCanvasToViewport();
    expectNear(view.GetZoom(), 1.25f);
    expectNear(view.GetPan().x, 0.0f);
    expectNear(view.GetPan().y, -25.0f);

    const auto constants = view.GetViewportCB();
    expectNear(constants.screenSize.x, 1000.0f);
    expectNear(constants.screenSize.y, 700.0f);
    expectNear(constants.zoom, 1.25f);
    expectNear(constants.rotation, 0.0f);
}

TEST(ViewportTransformerContractTest, ResetClampsAndNdcUseTopLeftOrigin)
{
    ViewportTransformer view;
    view.SetViewportSize(800.0f, 600.0f);
    view.SetCanvasSize(800.0f, 600.0f);
    view.SetZoom(-2.0f);
    expectNear(view.GetZoom(), 0.001f);
    view.SetRotation(240.0f);
    expectNear(view.GetRotation(), 180.0f);

    view.ResetView();
    expectNear(view.GetZoom(), 1.0f);
    expectNear(view.GetRotation(), 0.0f);
    expectNear(view.GetPan().x, 0.0f);
    expectNear(view.GetPan().y, 0.0f);

    const float2 topLeft = view.CanvasToNDC({0.0f, 0.0f});
    const float2 bottomRight = view.CanvasToNDC({800.0f, 600.0f});
    expectNear(topLeft.x, -1.0f);
    expectNear(topLeft.y, 1.0f);
    expectNear(bottomRight.x, 1.0f);
    expectNear(bottomRight.y, -1.0f);
}
