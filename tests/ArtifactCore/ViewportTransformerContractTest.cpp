#include <gtest/gtest.h>

#include <DiligentCore/Common/interface/BasicMath.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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

TEST(ViewportTransformerContractTest,
     NonFiniteRotationDoesNotPropagateIntoCoordinateTransforms)
{
    ViewportTransformer view;
    view.SetViewportSize(1280.0f, 720.0f);
    view.SetCanvasSize(1920.0f, 1080.0f);
    view.SetPan(23.0f, -17.0f);
    view.SetRotation(31.0f);
    view.SetRotation(std::numeric_limits<float>::quiet_NaN());

    const float2 viewportPoint = view.CanvasToViewport({321.0f, 456.0f});
    const float2 canvasPoint = view.ViewportToCanvas({321.0f, 456.0f});
    const float2 ndcPoint = view.CanvasToNDC({321.0f, 456.0f});

    EXPECT_TRUE(std::isfinite(view.GetRotation()));
    EXPECT_TRUE(std::isfinite(viewportPoint.x));
    EXPECT_TRUE(std::isfinite(viewportPoint.y));
    EXPECT_TRUE(std::isfinite(canvasPoint.x));
    EXPECT_TRUE(std::isfinite(canvasPoint.y));
    EXPECT_TRUE(std::isfinite(ndcPoint.x));
    EXPECT_TRUE(std::isfinite(ndcPoint.y));
}

TEST(ViewportTransformerContractTest,
     NonFiniteZoomDoesNotCorruptScalePanOrViewportCoordinates)
{
    const std::array<float, 3> invalidZooms = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
    const float2 anchor{314.0f, 201.0f};

    for (const float invalidZoom : invalidZooms) {
        SCOPED_TRACE(invalidZoom);
        ViewportTransformer view;
        view.SetViewportSize(1280.0f, 720.0f);
        view.SetCanvasSize(1920.0f, 1080.0f);
        view.SetPan(-24.0f, 18.0f);
        view.SetZoom(invalidZoom);
        EXPECT_TRUE(std::isfinite(view.GetZoom()));
        EXPECT_TRUE(std::isfinite(view.GetPan().x));
        EXPECT_TRUE(std::isfinite(view.GetPan().y));

        view.SetZoom(0.75f);
        const float2 canvasAnchor = view.ViewportToCanvas(anchor);
        view.ZoomAroundViewportPoint(anchor, invalidZoom);
        const float2 projectedAnchor = view.CanvasToViewport(canvasAnchor);
        EXPECT_TRUE(std::isfinite(view.GetZoom()));
        EXPECT_TRUE(std::isfinite(view.GetPan().x));
        EXPECT_TRUE(std::isfinite(view.GetPan().y));
        EXPECT_TRUE(std::isfinite(projectedAnchor.x));
        EXPECT_TRUE(std::isfinite(projectedAnchor.y));
        expectNear(projectedAnchor.x, anchor.x, 2.0e-2f);
        expectNear(projectedAnchor.y, anchor.y, 2.0e-2f);
    }
}

TEST(ViewportTransformerContractTest, ZoomAnchorIsStableAcrossRotationAndZoomBoundaryGrid)
{
    const std::array<float, 7> rotations = {
        -180.0f, -135.0f, -90.0f, -1.0f, 0.0f, 90.0f, 180.0f,
    };
    const std::array<float, 3> initialZooms = {0.1f, 0.75f, 3.0f};
    const std::array<float, 4> requestedZooms = {0.0005f, 0.2f, 1.7f, 12.0f};
    const std::array<float2, 5> anchors = {
        float2{0.0f, 0.0f}, float2{1280.0f, 0.0f},
        float2{0.0f, 720.0f}, float2{1280.0f, 720.0f},
        float2{423.5f, 318.25f},
    };

    for (const float rotation : rotations) {
        for (const float initialZoom : initialZooms) {
            for (const float requestedZoom : requestedZooms) {
                for (const float2 anchor : anchors) {
                    ViewportTransformer view;
                    view.SetViewportSize(1280.0f, 720.0f);
                    view.SetCanvasSize(1920.0f, 1080.0f);
                    view.SetPan(-241.0f, 87.0f);
                    view.SetZoom(initialZoom);
                    view.SetRotation(rotation);

                    const float2 canvasPoint = view.ViewportToCanvas(anchor);
                    view.ZoomAroundViewportPoint(anchor, requestedZoom);

                    const float2 reprojectedAnchor = view.CanvasToViewport(canvasPoint);
                    expectNear(reprojectedAnchor.x, anchor.x, 2.0e-2f);
                    expectNear(reprojectedAnchor.y, anchor.y, 2.0e-2f);
                    expectNear(view.GetZoom(), std::max(0.001f, requestedZoom));
                }
            }
        }
    }
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

TEST(ViewportTransformerContractTest, FillHonorsMarginsAndCentersWithinTheViewport)
{
    ViewportTransformer view;
    view.SetCanvasSize(800.0f, 600.0f);
    view.SetViewportSize(1000.0f, 700.0f);

    view.FillCanvasToViewport(50.0f);

    expectNear(view.GetZoom(), 1.125f);
    expectNear(view.GetPan().x, 50.0f);
    expectNear(view.GetPan().y, 12.5f);
}

TEST(ViewportTransformerContractTest, FitAndFillUseRotatedCanvasBounds)
{
    constexpr float canvasWidth = 800.0f;
    constexpr float canvasHeight = 600.0f;
    constexpr float viewportWidth = 1000.0f;
    constexpr float viewportHeight = 700.0f;
    constexpr float margin = 25.0f;
    const std::array<float, 4> rotations = {-90.0f, -37.0f, 45.0f, 90.0f};

    for (const float rotation : rotations) {
        const float radians = rotation * 0.01745329251994329577f;
        const float cosine = std::abs(std::cos(radians));
        const float sine = std::abs(std::sin(radians));
        const float rotatedWidth = canvasWidth * cosine + canvasHeight * sine;
        const float rotatedHeight = canvasWidth * sine + canvasHeight * cosine;
        const float availableWidth = viewportWidth - margin * 2.0f;
        const float availableHeight = viewportHeight - margin * 2.0f;
        const float expectedFit = std::min(
            availableWidth / rotatedWidth, availableHeight / rotatedHeight);
        const float expectedFill = std::max(
            availableWidth / rotatedWidth, availableHeight / rotatedHeight);

        ViewportTransformer view;
        view.SetCanvasSize(canvasWidth, canvasHeight);
        view.SetViewportSize(viewportWidth, viewportHeight);
        view.SetRotation(rotation);

        view.FitCanvasToViewport(margin);
        expectNear(view.GetZoom(), expectedFit, 2.0e-3f);
        expectNear(view.GetPan().x,
                   (viewportWidth - canvasWidth * expectedFit) * 0.5f, 2.0e-3f);
        expectNear(view.GetPan().y,
                   (viewportHeight - canvasHeight * expectedFit) * 0.5f, 2.0e-3f);

        view.FillCanvasToViewport(margin);
        expectNear(view.GetZoom(), expectedFill, 2.0e-3f);
        expectNear(view.GetPan().x,
                   (viewportWidth - canvasWidth * expectedFill) * 0.5f, 2.0e-3f);
        expectNear(view.GetPan().y,
                   (viewportHeight - canvasHeight * expectedFill) * 0.5f, 2.0e-3f);
        EXPECT_GE(rotatedWidth * view.GetZoom(), availableWidth - 2.0e-3f)
            << "rotation=" << rotation;
        EXPECT_GE(rotatedHeight * view.GetZoom(), availableHeight - 2.0e-3f)
            << "rotation=" << rotation;
    }
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

TEST(ViewportTransformerContractTest, NdcProjectionMatchesIndependentRotatedPixelMapping)
{
    const std::array<float, 5> rotations = {
        -180.0f, -73.0f, 0.0f, 121.0f, 180.0f,
    };
    const std::array<float, 4> zooms = {0.001f, 0.25f, 1.0f, 7.0f};
    const std::array<float2, 5> points = {
        float2{0.0f, 0.0f}, float2{1920.0f, 0.0f},
        float2{0.0f, 1080.0f}, float2{1920.0f, 1080.0f},
        float2{1137.25f, 422.5f},
    };

    for (const float rotation : rotations) {
        for (const float zoom : zooms) {
            ViewportTransformer view;
            view.SetCanvasSize(1920.0f, 1080.0f);
            view.SetViewportSize(1472.5f, 821.25f);
            view.SetPan(-311.0f, 146.5f);
            view.SetZoom(zoom);
            view.SetRotation(rotation);

            const float radians = rotation * 0.01745329251994329577f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            for (const float2 point : points) {
                const float relativeX = point.x - 960.0f;
                const float relativeY = point.y - 540.0f;
                const float pixelX =
                    (relativeX * cosine - relativeY * sine + 960.0f) * zoom - 311.0f;
                const float pixelY =
                    (relativeX * sine + relativeY * cosine + 540.0f) * zoom + 146.5f;
                const float expectedNdcX = pixelX / 1472.5f * 2.0f - 1.0f;
                const float expectedNdcY = 1.0f - pixelY / 821.25f * 2.0f;

                const float2 actual = view.CanvasToNDC(point);
                expectNear(actual.x, expectedNdcX, 2.0e-3f);
                expectNear(actual.y, expectedNdcY, 2.0e-3f);
            }
        }
    }
}

TEST(ViewportTransformerContractTest, FitLeavesViewUnchangedWhenNoUsableViewportAreaExists)
{
    ViewportTransformer view;
    view.SetCanvasSize(800.0f, 600.0f);
    view.SetViewportSize(1000.0f, 700.0f);
    view.SetPan(32.0f, -18.0f);
    view.SetZoom(1.4f);

    const float2 originalPan = view.GetPan();
    const float originalZoom = view.GetZoom();
    view.FitCanvasToViewport(400.0f);
    expectNear(view.GetZoom(), originalZoom);
    expectNear(view.GetPan().x, originalPan.x);
    expectNear(view.GetPan().y, originalPan.y);

    view.SetViewportSize(0.0f, 0.0f);
    view.FitCanvasToViewport();
    expectNear(view.GetZoom(), originalZoom);
    expectNear(view.GetPan().x, originalPan.x);
    expectNear(view.GetPan().y, originalPan.y);
}

TEST(ViewportTransformerContractTest, FillLeavesViewUnchangedWhenNoUsableViewportAreaExists)
{
    ViewportTransformer view;
    view.SetCanvasSize(800.0f, 600.0f);
    view.SetViewportSize(1000.0f, 700.0f);
    view.SetPan(-24.0f, 35.0f);
    view.SetZoom(1.6f);

    const float2 originalPan = view.GetPan();
    const float originalZoom = view.GetZoom();
    view.FillCanvasToViewport(400.0f);
    expectNear(view.GetZoom(), originalZoom);
    expectNear(view.GetPan().x, originalPan.x);
    expectNear(view.GetPan().y, originalPan.y);

    view.SetViewportSize(0.0f, 0.0f);
    view.FillCanvasToViewport();
    expectNear(view.GetZoom(), originalZoom);
    expectNear(view.GetPan().x, originalPan.x);
    expectNear(view.GetPan().y, originalPan.y);

    view.SetViewportSize(1000.0f, 700.0f);
    view.SetCanvasSize(0.0f, 600.0f);
    view.FillCanvasToViewport();
    expectNear(view.GetZoom(), originalZoom);
    expectNear(view.GetPan().x, originalPan.x);
    expectNear(view.GetPan().y, originalPan.y);
}
