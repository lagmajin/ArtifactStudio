#include <gtest/gtest.h>

#include <cmath>

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

import Mesh;
import Memory.SharedPtr;

using namespace ArtifactCore;

namespace {

SharedPtr<Mesh> makeQuad() {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(4);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*posAttr)[2] = QVector3D(1.0f, 1.0f, 0.0f);
    (*posAttr)[3] = QVector3D(0.0f, 1.0f, 0.0f);
    mesh->addPolygon({0, 1, 2});
    mesh->addPolygon({0, 2, 3});
    return mesh;
}

} // namespace

TEST(MeshGeometryTest, QuadNormalsPointUp) {
    auto mesh = makeQuad();
    mesh->computeVertexNormals();
    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ((*normals)[i].x(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[i].y(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[i].z(), 1.0f);
    }
}

TEST(MeshGeometryTest, DegenerateTriangleFallsBackFinite) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(3);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*posAttr)[2] = QVector3D(2.0f, 0.0f, 0.0f);
    mesh->addPolygon({0, 1, 2});
    mesh->computeVertexNormals();
    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(std::isfinite((*normals)[i].x()));
        EXPECT_TRUE(std::isfinite((*normals)[i].y()));
        EXPECT_TRUE(std::isfinite((*normals)[i].z()));
        EXPECT_FLOAT_EQ((*normals)[i].z(), 1.0f);
    }
}

TEST(MeshGeometryTest, PointCloudNormalsAreNoOp) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(2);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 1.0f, 1.0f);
    // No polygons: must not crash and must not create normals.
    mesh->computeVertexNormals();
    EXPECT_EQ(mesh->vertexAttributes().get<QVector3D>("normal"), nullptr);
}

TEST(MeshGeometryTest, BoundingSphereCoversBox) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(2);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(2.0f, 4.0f, 6.0f);
    mesh->updateBounds();
    const QVector3D center = mesh->boundingSphereCenter();
    EXPECT_FLOAT_EQ(center.x(), 1.0f);
    EXPECT_FLOAT_EQ(center.y(), 2.0f);
    EXPECT_FLOAT_EQ(center.z(), 3.0f);
    EXPECT_NEAR(mesh->boundingSphereRadius(), std::sqrt(14.0), 1e-5);
}
