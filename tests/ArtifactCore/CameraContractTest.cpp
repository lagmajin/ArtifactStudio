#include <gtest/gtest.h>

#include <QMatrix4x4>
#include <QVector3D>
#include <glm/glm.hpp>

#include <cmath>
#include <initializer_list>
#include <limits>

import Core.Camera;
import Math.Vec;

namespace {

using ArtifactCore::Camera;
using ArtifactCore::Coordinates::WorldPoint3;
using ArtifactCore::StereoCamera;
using ArtifactCore::Units::Degrees;
using ArtifactCore::Units::Meters;
using ArtifactCore::Units::WorldLength;

void expectNear(float actual, float expected, float tolerance = 1.0e-4f)
{
    EXPECT_NEAR(actual, expected, tolerance);
}

} // namespace

TEST(CameraContractTest, DefaultAndResetStateProduceForwardFacingCamera)
{
    Camera camera;
    const auto position = camera.position();
    const auto forward = camera.forward();
    EXPECT_FLOAT_EQ(position.x, 0.0f);
    EXPECT_FLOAT_EQ(position.y, 0.0f);
    EXPECT_FLOAT_EQ(position.z, 5.0f);
    EXPECT_FLOAT_EQ(forward.x, 0.0f);
    EXPECT_FLOAT_EQ(forward.y, 0.0f);
    EXPECT_FLOAT_EQ(forward.z, -1.0f);

    camera.setYaw(Degrees{37.0f});
    camera.setPitch(Degrees{22.0f});
    camera.setDistance(WorldLength{17.0f});
    camera.setPerspective(Degrees{70.0f}, 2.0f,
                          ArtifactCore::Units::Pixels{0.5f},
                          ArtifactCore::Units::Pixels{500.0f});
    camera.reset();

    expectNear(camera.yaw().value, 0.0f);
    expectNear(camera.pitch().value, 0.0f);
    expectNear(camera.distance().value, 5.0f);
    expectNear(camera.fovY().value, 45.0f);
    expectNear(camera.aspect(), 16.0f / 9.0f);
    expectNear(camera.nearZ().value, 0.1f);
    expectNear(camera.farZ().value, 1000.0f);
}

TEST(CameraContractTest, OrbitAndDollyClampPitchAndMinimumDistance)
{
    Camera camera;
    camera.setPitch(Degrees{120.0f});
    expectNear(camera.pitch().value, 89.9f);
    camera.orbit(Degrees{15.0f}, Degrees{-300.0f});
    expectNear(camera.yaw().value, 15.0f);
    expectNear(camera.pitch().value, -89.9f);

    camera.setDistance(WorldLength{-5.0f});
    expectNear(camera.distance().value, 0.01f);
    camera.dolly(WorldLength{100.0f});
    expectNear(camera.distance().value, 0.01f);
}

TEST(CameraContractTest, PanMovesEyeAndTargetTogetherWithoutChangingOrbitDistance)
{
    Camera camera;
    camera.pan(WorldLength{2.0f}, WorldLength{-3.0f});

    expectNear(camera.target().x, 2.0f);
    expectNear(camera.target().y, -3.0f);
    expectNear(camera.target().z, 0.0f);
    expectNear(camera.position().x, 2.0f);
    expectNear(camera.position().y, -3.0f);
    expectNear(camera.position().z, 5.0f);
    expectNear(camera.distance().value, 5.0f);
    expectNear(camera.forward().x, 0.0f);
    expectNear(camera.forward().y, 0.0f);
    expectNear(camera.forward().z, -1.0f);
}

TEST(CameraContractTest, OrthographicViewsPointTowardTheirTargets)
{
    Camera camera;
    camera.setViewFront();
    expectNear(camera.forward().x, 0.0f);
    expectNear(camera.forward().y, 0.0f);
    expectNear(camera.forward().z, -1.0f);

    camera.setViewBack();
    expectNear(camera.forward().z, 1.0f);
    camera.setViewLeft();
    expectNear(camera.forward().x, 1.0f);
    camera.setViewRight();
    expectNear(camera.forward().x, -1.0f);

    camera.setViewTop();
    expectNear(camera.pitch().value, 89.9f);
    camera.setViewBottom();
    expectNear(camera.pitch().value, -89.9f);
}

TEST(CameraContractTest, LookAtDerivesOrbitStateAndZeroOrbitReconstructsEye)
{
    Camera camera;
    const WorldPoint3 eye{3.0f, 2.0f, 5.0f};
    const WorldPoint3 target{1.0f, -1.0f, 0.0f};
    camera.lookAt(eye, target);

    expectNear(camera.position().x, eye.x);
    expectNear(camera.position().y, eye.y);
    expectNear(camera.position().z, eye.z);
    expectNear(camera.target().x, target.x);
    expectNear(camera.target().y, target.y);
    expectNear(camera.target().z, target.z);
    expectNear(camera.distance().value, std::sqrt(38.0f));
    expectNear(camera.yaw().value,
               std::atan2(-2.0f, -5.0f) * 180.0f / 3.14159265358979323846f);
    expectNear(camera.pitch().value,
               std::atan2(-3.0f, std::sqrt(29.0f)) *
                   180.0f / 3.14159265358979323846f);

    camera.orbit(Degrees{0.0f}, Degrees{0.0f});
    expectNear(camera.position().x, eye.x);
    expectNear(camera.position().y, eye.y);
    expectNear(camera.position().z, eye.z);
}

TEST(CameraContractTest, FitToSphereCentersTargetAndComputesPerspectiveDistance)
{
    Camera camera;
    camera.setViewFront();
    const WorldPoint3 center{3.0f, -2.0f, 5.0f};
    const float radius = 2.5f;
    const float margin = 1.3f;
    camera.fitToSphere(center, WorldLength{radius}, margin);

    expectNear(camera.target().x, center.x);
    expectNear(camera.target().y, center.y);
    expectNear(camera.target().z, center.z);
    const float expectedDistance =
        radius * margin / std::sin(camera.fovY().value * 0.5f * 0.01745329251994329577f);
    expectNear(camera.distance().value, expectedDistance);
    expectNear(camera.position().z, center.z + expectedDistance);
}

TEST(CameraContractTest, FitToSphereClampsZeroAndNegativeRadiusToMinimumDistance)
{
    Camera camera;
    const WorldPoint3 center{2.0f, -3.0f, 4.0f};

    for (const float radius : {0.0f, -2.0f}) {
        camera.fitToSphere(center, WorldLength{radius});

        expectNear(camera.target().x, center.x);
        expectNear(camera.target().y, center.y);
        expectNear(camera.target().z, center.z);
        expectNear(camera.distance().value, 0.01f);
        EXPECT_TRUE(std::isfinite(camera.position().x));
        EXPECT_TRUE(std::isfinite(camera.position().y));
        EXPECT_TRUE(std::isfinite(camera.position().z));
    }
}

TEST(CameraContractTest, ProjectionInputsClampToFiniteSupportedRanges)
{
    Camera camera;
    camera.setFovY(Degrees{-10.0f});
    expectNear(camera.fovY().value, 1.0f);
    camera.setFovY(Degrees{200.0f});
    expectNear(camera.fovY().value, 179.0f);
    camera.setAspect(-1.0f);
    expectNear(camera.aspect(), 0.001f);

    const auto projection = camera.projectionMatrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_TRUE(std::isfinite(projection[column][row]));
        }
    }
}

TEST(CameraContractTest, NonFiniteProjectionInputsDoNotPoisonCameraState)
{
    Camera camera;
    camera.setFovY(Degrees{std::numeric_limits<float>::quiet_NaN()});
    camera.setAspect(std::numeric_limits<float>::infinity());

    EXPECT_TRUE(std::isfinite(camera.fovY().value));
    EXPECT_TRUE(std::isfinite(camera.aspect()));
    const auto projection = camera.projectionMatrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_TRUE(std::isfinite(projection[column][row]));
        }
    }
}

TEST(CameraContractTest, StereoEyesUseSymmetricHalfIpdOffsets)
{
    const float ipd = 0.072f;
    const StereoCamera stereo = StereoCamera::fromHmd(
        QMatrix4x4(), Meters{ipd}, ArtifactCore::Units::Pixels{0.2f},
        ArtifactCore::Units::Pixels{250.0f});

    const QVector3D origin(0.0f, 0.0f, 0.0f);
    const QVector3D leftOrigin = stereo.leftEyeView.map(origin);
    const QVector3D rightOrigin = stereo.rightEyeView.map(origin);
    expectNear(leftOrigin.x(), -ipd * 0.5f);
    expectNear(rightOrigin.x(), ipd * 0.5f);
    expectNear(leftOrigin.y(), 0.0f);
    expectNear(rightOrigin.y(), 0.0f);
    expectNear(stereo.ipd.value, ipd);
    expectNear(stereo.nearPlane.value, 0.2f);
    expectNear(stereo.farPlane.value, 250.0f);
}
