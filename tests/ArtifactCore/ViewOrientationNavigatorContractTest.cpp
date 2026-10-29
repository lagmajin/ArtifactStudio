#include <gtest/gtest.h>

#include <QQuaternion>
#include <QVector3D>

#include <cmath>

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
