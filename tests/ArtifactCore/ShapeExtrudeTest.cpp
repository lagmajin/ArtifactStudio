#include <gtest/gtest.h>

#include <QPointF>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

import Geometry.ShapeExtrude;
import Mesh;

using namespace ArtifactCore;

namespace {

std::vector<QPointF> rectangleContour()
{
    return {{0.0, 0.0}, {40.0, 0.0}, {40.0, 20.0}, {0.0, 20.0}};
}

} // namespace

TEST(ShapeExtrudeTest, BuildsRenderableMeshForClosedRectangle)
{
    Mesh mesh;
    ShapeExtrudeParams params;
    params.depth = 20.0f;
    params.bevelWidth = 0.0f;

    ASSERT_TRUE(extrudeContourMesh({rectangleContour()}, params, mesh));
    EXPECT_GT(mesh.vertexCount(), 0);
    EXPECT_GT(mesh.polygonCount(), 0);

    const auto positions = mesh.vertexAttributes().get<QVector3D>("position");
    const auto normals = mesh.vertexAttributes().get<QVector3D>("normal");
    const auto uvs = mesh.vertexAttributes().get<QVector2D>("uv");
    ASSERT_TRUE(positions);
    ASSERT_TRUE(normals);
    ASSERT_TRUE(uvs);
    EXPECT_EQ(positions->size(), mesh.vertexCount());
    EXPECT_EQ(normals->size(), mesh.vertexCount());
    EXPECT_EQ(uvs->size(), mesh.vertexCount());

    float minZ = positions->data().front().z();
    float maxZ = minZ;
    for (const auto& position : positions->data()) {
        minZ = std::min(minZ, position.z());
        maxZ = std::max(maxZ, position.z());
    }
    EXPECT_FLOAT_EQ(minZ, -10.0f);
    EXPECT_FLOAT_EQ(maxZ, 10.0f);

    const Mesh::RenderData renderData = mesh.generateRenderData();
    EXPECT_FALSE(renderData.positions.isEmpty());
    EXPECT_EQ(renderData.positions.size(), renderData.normals.size());
    EXPECT_EQ(renderData.positions.size(), renderData.uvs.size());
    EXPECT_FALSE(renderData.indices.isEmpty());
}

TEST(ShapeExtrudeTest, BevelAddsGeometryWithoutChangingDepthExtent)
{
    Mesh straight;
    ShapeExtrudeParams straightParams;
    straightParams.depth = 24.0f;
    ASSERT_TRUE(extrudeContourMesh({rectangleContour()}, straightParams, straight));

    Mesh beveled;
    ShapeExtrudeParams bevelParams = straightParams;
    bevelParams.bevelWidth = 3.0f;
    bevelParams.bevelSegments = 2;
    ASSERT_TRUE(extrudeContourMesh({rectangleContour()}, bevelParams, beveled));

    EXPECT_GT(beveled.vertexCount(), straight.vertexCount());
    EXPECT_GT(beveled.polygonCount(), straight.polygonCount());

    const auto positions = beveled.vertexAttributes().get<QVector3D>("position");
    ASSERT_TRUE(positions);
    float minZ = positions->data().front().z();
    float maxZ = minZ;
    for (const auto& position : positions->data()) {
        minZ = std::min(minZ, position.z());
        maxZ = std::max(maxZ, position.z());
    }
    EXPECT_FLOAT_EQ(minZ, -12.0f);
    EXPECT_FLOAT_EQ(maxZ, 12.0f);
}

TEST(ShapeExtrudeTest, RejectsInvalidInputWithoutReplacingOutputMesh)
{
    Mesh mesh;
    ShapeExtrudeParams validParams;
    validParams.depth = 10.0f;
    ASSERT_TRUE(extrudeContourMesh({rectangleContour()}, validParams, mesh));
    const int vertexCount = mesh.vertexCount();
    const int polygonCount = mesh.polygonCount();

    ShapeExtrudeParams invalidParams = validParams;
    invalidParams.depth = 0.0f;
    EXPECT_FALSE(extrudeContourMesh({rectangleContour()}, invalidParams, mesh));
    EXPECT_EQ(mesh.vertexCount(), vertexCount);
    EXPECT_EQ(mesh.polygonCount(), polygonCount);
}

TEST(ShapeExtrudeTest, KeepsCapOpenAcrossTheEntireInnerContour)
{
    const std::vector<QPointF> innerContour = {
        {8.0, 8.0}, {12.0, 8.0}, {12.0, 12.0}, {8.0, 12.0}};
    Mesh mesh;
    ShapeExtrudeParams params;
    params.depth = 20.0f;
    params.bevelWidth = 0.0f;
    ASSERT_TRUE(extrudeContourMesh({rectangleContour(), innerContour}, params, mesh));

    const Mesh::RenderData renderData = mesh.generateRenderData();
    ASSERT_EQ(renderData.positions.size(), renderData.indices.size());

    // The center of the hole must not be covered by any triangle on the front cap.
    constexpr double testX = 10.0;
    constexpr double testY = 10.0;
    constexpr float frontZ = 10.0f;
    bool coveredByFrontCap = false;
    for (qsizetype i = 0; i + 2 < renderData.indices.size(); i += 3) {
        const QVector3D& a = renderData.positions[static_cast<qsizetype>(renderData.indices[i])];
        const QVector3D& b = renderData.positions[static_cast<qsizetype>(renderData.indices[i + 1])];
        const QVector3D& c = renderData.positions[static_cast<qsizetype>(renderData.indices[i + 2])];
        if (std::abs(a.z() - frontZ) > 1.0e-5f ||
            std::abs(b.z() - frontZ) > 1.0e-5f ||
            std::abs(c.z() - frontZ) > 1.0e-5f) {
            continue;
        }

        const double denominator = (b.y() - c.y()) * (a.x() - c.x()) +
                                   (c.x() - b.x()) * (a.y() - c.y());
        if (std::abs(denominator) < 1.0e-9) continue;
        const double u = ((b.y() - c.y()) * (testX - c.x()) +
                          (c.x() - b.x()) * (testY - c.y())) / denominator;
        const double v = ((c.y() - a.y()) * (testX - c.x()) +
                          (a.x() - c.x()) * (testY - c.y())) / denominator;
        const double w = 1.0 - u - v;
        if (u >= 0.0 && v >= 0.0 && w >= 0.0) {
            coveredByFrontCap = true;
            break;
        }
    }
    EXPECT_FALSE(coveredByFrontCap);
}
