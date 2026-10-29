#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <cmath>

import Core.Camera;
import Math.Vec;

namespace {

using ArtifactCore::Camera;
using ArtifactCore::Coordinates::WorldPoint3;
using ArtifactCore::Units::Degrees;
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
