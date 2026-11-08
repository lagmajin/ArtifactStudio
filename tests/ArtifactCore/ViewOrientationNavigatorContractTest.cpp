#include <gtest/gtest.h>

#include <QQuaternion>
#include <QVector3D>

#include <array>
#include <cmath>
#include <limits>
#include <utility>

import UI.View.Orientation.Navigator;

namespace {

using ArtifactCore::ViewOrientationHotspot;
using ArtifactCore::ViewOrientationNavigator;

bool sameRotation(const QQuaternion& a, const QQuaternion& b)
{
    return std::abs(QQuaternion::dotProduct(a.normalized(), b.normalized())) >
           0.9999f;
}

} // namespace

TEST(ViewOrientationNavigatorContractTest, ResolvesAxisFaceAndCornerDirections)
{
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({0, 0, 1}),
              ViewOrientationHotspot::Front);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({0, 0, -1}),
              ViewOrientationHotspot::Back);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({1, 0, 0}),
              ViewOrientationHotspot::Right);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({-1, 0, 0}),
              ViewOrientationHotspot::Left);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({0, 1, 0}),
              ViewOrientationHotspot::Top);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({0, -1, 0}),
              ViewOrientationHotspot::Bottom);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({1, 1, 1}),
              ViewOrientationHotspot::FrontTopRight);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({-1, 1, -1}),
              ViewOrientationHotspot::BackTopLeft);
    EXPECT_EQ(ViewOrientationNavigator::resolveHotspotFromDirection({0, 0, 0}),
              ViewOrientationHotspot::None);
}

TEST(ViewOrientationNavigatorContractTest,
     EveryHotspotOrientationRoundTripsThroughItsCameraPositionDirection)
{
    const std::array<std::pair<ViewOrientationHotspot, QVector3D>, 26> hotspots{{
        {ViewOrientationHotspot::Front, {0.0f, 0.0f, 1.0f}},
        {ViewOrientationHotspot::Back, {0.0f, 0.0f, -1.0f}},
        {ViewOrientationHotspot::Left, {-1.0f, 0.0f, 0.0f}},
        {ViewOrientationHotspot::Right, {1.0f, 0.0f, 0.0f}},
        {ViewOrientationHotspot::Top, {0.0f, 1.0f, 0.0f}},
        {ViewOrientationHotspot::Bottom, {0.0f, -1.0f, 0.0f}},
        {ViewOrientationHotspot::FrontTop, {0.0f, 1.0f, 1.0f}},
        {ViewOrientationHotspot::FrontBottom, {0.0f, -1.0f, 1.0f}},
        {ViewOrientationHotspot::FrontLeft, {-1.0f, 0.0f, 1.0f}},
        {ViewOrientationHotspot::FrontRight, {1.0f, 0.0f, 1.0f}},
        {ViewOrientationHotspot::BackTop, {0.0f, 1.0f, -1.0f}},
        {ViewOrientationHotspot::BackBottom, {0.0f, -1.0f, -1.0f}},
        {ViewOrientationHotspot::BackLeft, {-1.0f, 0.0f, -1.0f}},
        {ViewOrientationHotspot::BackRight, {1.0f, 0.0f, -1.0f}},
        {ViewOrientationHotspot::LeftTop, {-1.0f, 1.0f, 0.0f}},
        {ViewOrientationHotspot::LeftBottom, {-1.0f, -1.0f, 0.0f}},
        {ViewOrientationHotspot::RightTop, {1.0f, 1.0f, 0.0f}},
        {ViewOrientationHotspot::RightBottom, {1.0f, -1.0f, 0.0f}},
        {ViewOrientationHotspot::FrontTopLeft, {-1.0f, 1.0f, 1.0f}},
        {ViewOrientationHotspot::FrontTopRight, {1.0f, 1.0f, 1.0f}},
        {ViewOrientationHotspot::FrontBottomLeft, {-1.0f, -1.0f, 1.0f}},
        {ViewOrientationHotspot::FrontBottomRight, {1.0f, -1.0f, 1.0f}},
        {ViewOrientationHotspot::BackTopLeft, {-1.0f, 1.0f, -1.0f}},
        {ViewOrientationHotspot::BackTopRight, {1.0f, 1.0f, -1.0f}},
        {ViewOrientationHotspot::BackBottomLeft, {-1.0f, -1.0f, -1.0f}},
        {ViewOrientationHotspot::BackBottomRight, {1.0f, -1.0f, -1.0f}},
    }};

    for (const auto& [hotspot, expectedPositionDirection] : hotspots) {
        const QQuaternion orientation =
            ViewOrientationNavigator::orientationForHotspot(hotspot);
        const QVector3D actualDirection =
            orientation.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));
        SCOPED_TRACE(static_cast<int>(hotspot));
        EXPECT_NEAR(orientation.length(), 1.0f, 1.0e-5f);
        EXPECT_TRUE(sameRotation(
            orientation,
            ViewOrientationNavigator::orientationForHotspot(
                ViewOrientationNavigator::resolveHotspotFromDirection(actualDirection))));
        EXPECT_GT(QVector3D::dotProduct(actualDirection.normalized(),
                                       expectedPositionDirection.normalized()), 0.999f);
    }
}

TEST(ViewOrientationNavigatorContractTest, SnapDurationClampsAndAnimationCompletesAtTarget)
{
    ViewOrientationNavigator navigator;
    navigator.setSnapDurationSeconds(0.0f);
    EXPECT_FLOAT_EQ(navigator.snapDurationSeconds(), 0.01f);
    navigator.setSnapDurationSeconds(5.0f);
    EXPECT_FLOAT_EQ(navigator.snapDurationSeconds(), 2.0f);

    navigator.setSnapDurationSeconds(1.0f);
    navigator.snapTo(ViewOrientationHotspot::Right);
    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_EQ(navigator.activeHotspot(), ViewOrientationHotspot::Right);
    EXPECT_TRUE(navigator.update(0.5f));
    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_NEAR(navigator.currentOrientation().length(), 1.0f, 1.0e-5f);

    EXPECT_TRUE(navigator.update(0.5f));
    EXPECT_FALSE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(),
                             navigator.targetOrientation()));
    EXPECT_FALSE(navigator.update(0.1f));
}

TEST(ViewOrientationNavigatorContractTest,
     HalfwaySnapToOppositeHotspotStaysFiniteAndReachesTarget)
{
    ViewOrientationNavigator navigator;
    navigator.setSnapDurationSeconds(1.0f);
    const QVector3D initialDirection =
        navigator.currentOrientation().rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));
    navigator.snapTo(ViewOrientationHotspot::Back);
    const QVector3D targetDirection =
        navigator.targetOrientation().rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));

    ASSERT_TRUE(navigator.update(0.5f));
    const QQuaternion halfway = navigator.currentOrientation();
    const QVector3D halfwayDirection =
        halfway.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f));
    EXPECT_NEAR(halfway.length(), 1.0f, 1.0e-5f);
    EXPECT_NEAR(halfwayDirection.length(), 1.0f, 1.0e-5f);
    EXPECT_NEAR(QVector3D::dotProduct(halfwayDirection, initialDirection), 0.0f, 1.0e-4f);
    EXPECT_NEAR(QVector3D::dotProduct(halfwayDirection, targetDirection), 0.0f, 1.0e-4f);
    EXPECT_TRUE(navigator.isAnimating());

    ASSERT_TRUE(navigator.update(0.5f));
    EXPECT_FALSE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(),
                             navigator.targetOrientation()));
}

TEST(ViewOrientationNavigatorContractTest, NegativeDeltaDoesNotAdvanceAndImmediateSnapStopsAnimation)
{
    ViewOrientationNavigator navigator;
    navigator.setSnapDurationSeconds(0.5f);
    const QQuaternion initial = navigator.currentOrientation();
    navigator.snapTo(ViewOrientationHotspot::BackTop);

    EXPECT_TRUE(navigator.update(-1.0f));
    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), initial));

    navigator.snapTo(ViewOrientationHotspot::Top, true);
    EXPECT_FALSE(navigator.isAnimating());
    EXPECT_EQ(navigator.activeHotspot(), ViewOrientationHotspot::Top);
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(),
                             navigator.targetOrientation()));
    EXPECT_FALSE(navigator.update(0.25f));
}

TEST(ViewOrientationNavigatorContractTest,
     NonFiniteDeltaCannotCorruptAnimationAndPositiveInfinityCompletesIt)
{
    ViewOrientationNavigator navigator;
    navigator.setSnapDurationSeconds(1.0f);
    navigator.snapTo(ViewOrientationHotspot::Right);
    ASSERT_TRUE(navigator.isAnimating());
    const QQuaternion initial = navigator.currentOrientation();

    EXPECT_TRUE(navigator.update(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), initial));

    EXPECT_TRUE(navigator.update(-std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), initial));

    EXPECT_TRUE(navigator.update(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(),
                             navigator.targetOrientation()));
}

TEST(ViewOrientationNavigatorContractTest,
     RetargetingAnActiveSnapContinuesFromTheCurrentOrientation)
{
    ViewOrientationNavigator navigator;
    navigator.setSnapDurationSeconds(1.0f);
    navigator.snapTo(ViewOrientationHotspot::Right);
    ASSERT_TRUE(navigator.update(0.3f));
    const QQuaternion orientationAtRetarget = navigator.currentOrientation();
    ASSERT_FALSE(sameRotation(orientationAtRetarget,
                              navigator.targetOrientation()));

    navigator.snapTo(ViewOrientationHotspot::Top);

    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_EQ(navigator.activeHotspot(), ViewOrientationHotspot::Top);
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), orientationAtRetarget));
    EXPECT_TRUE(sameRotation(navigator.targetOrientation(),
                             ViewOrientationNavigator::orientationForHotspot(
                                 ViewOrientationHotspot::Top)));
    ASSERT_TRUE(navigator.update(0.0f));
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), orientationAtRetarget));

    ASSERT_TRUE(navigator.update(1.0f));
    EXPECT_FALSE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(),
                             navigator.targetOrientation()));
}

TEST(ViewOrientationNavigatorContractTest,
     ExternalOrientationIsNormalizedAndClassifiedBeforeTheNextSnap)
{
    ViewOrientationNavigator navigator;
    const QQuaternion scaledIdentity(2.0f, 0.0f, 0.0f, 0.0f);

    navigator.setCurrentOrientation(scaledIdentity);

    EXPECT_NEAR(navigator.currentOrientation().length(), 1.0f, 1.0e-6f);
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), QQuaternion()));
    EXPECT_EQ(navigator.activeHotspot(), ViewOrientationHotspot::Front);
    EXPECT_TRUE(sameRotation(navigator.targetOrientation(),
                             navigator.currentOrientation()));

    navigator.setSnapDurationSeconds(0.8f);
    navigator.snapTo(ViewOrientationHotspot::Right);
    EXPECT_TRUE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(), scaledIdentity));
    EXPECT_TRUE(navigator.update(0.4f));
    EXPECT_NEAR(navigator.currentOrientation().length(), 1.0f, 1.0e-5f);
    EXPECT_FALSE(sameRotation(navigator.currentOrientation(),
                              navigator.targetOrientation()));

    EXPECT_TRUE(navigator.update(0.4f));
    EXPECT_FALSE(navigator.isAnimating());
    EXPECT_TRUE(sameRotation(navigator.currentOrientation(),
                             ViewOrientationNavigator::orientationForHotspot(
                                 ViewOrientationHotspot::Right)));
}
