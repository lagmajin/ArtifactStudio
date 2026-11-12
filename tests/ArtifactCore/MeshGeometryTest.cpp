#include <gtest/gtest.h>

#include <cmath>
#include <limits>

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

TEST(MeshGeometryTest, TopologyQuadUsesOriginalSharedEdges) {
    auto mesh = makeQuad();
    const auto s = mesh->analyzeTopology();
    EXPECT_TRUE(s.positionAnalysisAvailable);
    EXPECT_EQ(s.boundaryEdgeCount, 4);
    EXPECT_EQ(s.edgeCount, 5);
    EXPECT_EQ(s.validPolygonCount, 2);
    EXPECT_EQ(s.nonManifoldEdgeCount, 0);
    EXPECT_EQ(s.inconsistentWindingEdgeCount, 0);
    EXPECT_EQ(s.connectedComponentCount, 1);
    EXPECT_EQ(s.eulerCharacteristic, 1);
    EXPECT_EQ(s.degenerateTriangleCount, 0);
}

TEST(MeshGeometryTest, TopologyReportsDefectsAndIsolatedVertices) {
    Mesh mesh;
    mesh.setVertexCount(7);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(0, 0, 0);
    (*p)[1] = QVector3D(1, 0, 0);
    (*p)[2] = QVector3D(0, 1, 0);
    (*p)[3] = QVector3D(0, -1, 0);
    (*p)[4] = QVector3D(2, 0, 0);
    mesh.addPolygon({0, 1, 2});
    mesh.addPolygon({0, 1, 3}); // same direction along 0-1
    mesh.addPolygon({0, 1, 4}); // third incident face, collinear
    mesh.addPolygon({2, 1, 0}); // reverse duplicate
    mesh.addPolygon({0, 0, 5}); // malformed
    mesh.addPolygon({0, 1, 99}); // out of range
    const auto s = mesh.analyzeTopology();
    EXPECT_EQ(s.invalidPolygonCount, 2);
    EXPECT_EQ(s.nonManifoldEdgeCount, 1);
    EXPECT_EQ(s.duplicateFaceCount, 1);
    EXPECT_EQ(s.degenerateTriangleCount, 2);
    EXPECT_EQ(s.isolatedVertexCount, 2);
    EXPECT_EQ(s.connectedComponentCount, 3);
}

TEST(MeshGeometryTest, TopologyDistinguishesWindingAndUnavailablePositions) {
    Mesh mesh;
    mesh.setVertexCount(4);
    mesh.addPolygon({0, 1, 2});
    mesh.addPolygon({0, 1, 3});
    auto s = mesh.analyzeTopology();
    EXPECT_FALSE(s.positionAnalysisAvailable);
    EXPECT_EQ(s.inconsistentWindingEdgeCount, 1);
    EXPECT_EQ(s.inconsistentWindingFaceCount, 2);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(std::numeric_limits<float>::quiet_NaN(), 0, 0);
    s = mesh.analyzeTopology();
    EXPECT_TRUE(s.positionAnalysisAvailable);
    EXPECT_EQ(s.nonFinitePositionTriangleCount, 2);
    EXPECT_EQ(s.degenerateTriangleCount, 0);
}

TEST(MeshGeometryTest, TopologyClosedTetrahedronAndFaceCanonicalization) {
    Mesh mesh;
    mesh.setVertexCount(4);
    mesh.addPolygon({0, 2, 1});
    mesh.addPolygon({0, 1, 3});
    mesh.addPolygon({1, 2, 3});
    mesh.addPolygon({2, 0, 3});
    auto s = mesh.analyzeTopology();
    EXPECT_EQ(s.boundaryEdgeCount, 0);
    EXPECT_EQ(s.inconsistentWindingEdgeCount, 0);
    EXPECT_EQ(s.inconsistentWindingFaceCount, 0);
    EXPECT_EQ(s.eulerCharacteristic, 2);
    mesh.addPolygon({2, 1, 0});
    s = mesh.analyzeTopology();
    EXPECT_EQ(s.duplicateFaceCount, 1);
}

TEST(MeshGeometryTest, TopologyCanWeldSplitPositionsWithoutChangingMesh) {
    Mesh mesh;
    mesh.setVertexCount(6);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(0, 0, 0); (*p)[1] = QVector3D(1, 0, 0); (*p)[2] = QVector3D(0, 1, 0);
    (*p)[3] = QVector3D(1, 0, 0); (*p)[4] = QVector3D(1, 1, 0); (*p)[5] = QVector3D(0, 1, 0);
    mesh.addPolygon({0, 1, 2});
    mesh.addPolygon({3, 4, 5});
    const auto original = mesh.analyzeTopology();
    const auto welded = mesh.analyzeTopology(0.0, true);
    EXPECT_EQ(original.topologyVertexCount, 6);
    EXPECT_EQ(original.boundaryEdgeCount, 6);
    EXPECT_EQ(original.connectedComponentCount, 2);
    EXPECT_TRUE(welded.topologyAnalysisAvailable);
    EXPECT_EQ(welded.topologyVertexCount, 4);
    EXPECT_EQ(welded.boundaryEdgeCount, 4);
    EXPECT_EQ(welded.connectedComponentCount, 1);
    EXPECT_EQ(welded.eulerCharacteristic, 1);
    EXPECT_EQ(mesh.vertexCount(), 6);
}

TEST(MeshGeometryTest, TopologyWeldingRequiresFinitePositions) {
    Mesh mesh;
    mesh.setVertexCount(3);
    mesh.addPolygon({0, 1, 2});
    EXPECT_FALSE(mesh.analyzeTopology(0.0, true).topologyAnalysisAvailable);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[1] = QVector3D(std::numeric_limits<float>::infinity(), 0, 0);
    EXPECT_FALSE(mesh.analyzeTopology(0.0, true).topologyAnalysisAvailable);
    EXPECT_TRUE(mesh.analyzeTopology().topologyAnalysisAvailable);
}

TEST(MeshGeometryTest, TopologyUsesFanAreaAndCallerTolerance) {
    Mesh mesh;
    mesh.setVertexCount(4);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(0, 0, 0); (*p)[1] = QVector3D(2, 0, 0);
    (*p)[2] = QVector3D(2, 0.01f, 0); (*p)[3] = QVector3D(0, 1, 0);
    mesh.addPolygon({0, 1, 2, 3});
    const auto exact = mesh.analyzeTopology();
    const auto tolerant = mesh.analyzeTopology(0.02);
    EXPECT_EQ(exact.degenerateTriangleCount, 0);
    EXPECT_EQ(tolerant.degenerateTriangleCount, 1);
    EXPECT_EQ(tolerant.boundaryEdgeCount, 4);
    EXPECT_EQ(tolerant.eulerCharacteristic, 1);
}

TEST(MeshGeometryTest, SelfIntersectionFindsCrossingAndCoplanarOverlap) {
    Mesh mesh;
    mesh.setVertexCount(9);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(0, 0, 0); (*p)[1] = QVector3D(2, 0, 0); (*p)[2] = QVector3D(0, 2, 0);
    (*p)[3] = QVector3D(0.5f, 0.5f, -1); (*p)[4] = QVector3D(0.5f, 0.5f, 1); (*p)[5] = QVector3D(1, 0.5f, 0);
    (*p)[6] = QVector3D(0.25f, 0.25f, 0); (*p)[7] = QVector3D(0.5f, 0.25f, 0); (*p)[8] = QVector3D(0.25f, 0.5f, 0);
    mesh.addPolygon({0, 1, 2});
    mesh.addPolygon({3, 4, 5});
    mesh.addPolygon({6, 7, 8});
    const auto s = mesh.analyzeSelfIntersections();
    EXPECT_TRUE(s.positionAnalysisAvailable);
    EXPECT_EQ(s.skippedTriangleCount, 0);
    EXPECT_EQ(s.intersectingPairCount, 2);
}

TEST(MeshGeometryTest, SelfIntersectionExcludesAdjacentAndSeparatedPairs) {
    Mesh mesh;
    mesh.setVertexCount(7);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(0, 0, 0); (*p)[1] = QVector3D(1, 0, 0); (*p)[2] = QVector3D(0, 1, 0);
    (*p)[3] = QVector3D(1, 1, 0); (*p)[4] = QVector3D(3, 0, 0); (*p)[5] = QVector3D(4, 0, 0); (*p)[6] = QVector3D(3, 1, 0);
    mesh.addPolygon({0, 1, 2});
    mesh.addPolygon({1, 3, 2});
    mesh.addPolygon({4, 5, 6});
    const auto s = mesh.analyzeSelfIntersections();
    EXPECT_EQ(s.adjacentPairCount, 1);
    EXPECT_EQ(s.intersectingPairCount, 0);
}

TEST(MeshGeometryTest, SelfIntersectionReportsSplitVertexAndFanExclusions) {
    Mesh mesh;
    mesh.setVertexCount(7);
    auto p = mesh.vertexAttributes().add<QVector3D>("position");
    (*p)[0] = QVector3D(0, 0, 0); (*p)[1] = QVector3D(1, 0, 0);
    (*p)[2] = QVector3D(1, 1, 0); (*p)[3] = QVector3D(0, 1, 0);
    (*p)[4] = QVector3D(1, 0, 0); (*p)[5] = QVector3D(2, 0, 0);
    (*p)[6] = QVector3D(1, 1, 0);
    mesh.addPolygon({0, 1, 2, 3});
    mesh.addPolygon({4, 5, 6});
    const auto original = mesh.analyzeSelfIntersections();
    const auto welded = mesh.analyzeSelfIntersections(0.0, true);
    EXPECT_EQ(original.samePolygonPairCount, 1);
    EXPECT_EQ(original.intersectingPairCount, 2);
    EXPECT_EQ(welded.samePolygonPairCount, 1);
    EXPECT_EQ(welded.adjacentPairCount, 2);
    EXPECT_EQ(welded.intersectingPairCount, 0);
}

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

TEST(MeshGeometryTest, RenderDataSnapshotMutationDoesNotChangeCachedData) {
    auto mesh = makeQuad();
    auto uvs = mesh->vertexAttributes().add<QVector2D>("uv");
    ASSERT_NE(uvs, nullptr);
    for (int index = 0; index < mesh->vertexCount(); ++index) {
        (*uvs)[index] = QVector2D(static_cast<float>(index),
                                  static_cast<float>(index + 10));
    }

    Mesh::RenderData returned = mesh->generateRenderData();
    ASSERT_EQ(returned.positions.size(), 6);
    ASSERT_EQ(returned.uvs.size(), 6);
    returned.positions[0] = QVector3D(99.0f, 98.0f, 97.0f);
    returned.uvs[0] = QVector2D(-5.0f, -6.0f);
    returned.indices[0] = 5;

    const Mesh::RenderData cachedAgain = mesh->generateRenderData();

    EXPECT_EQ(cachedAgain.positions[0], QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(cachedAgain.uvs[0], QVector2D(0.0f, 10.0f));
    EXPECT_EQ(cachedAgain.indices[0], 0u);
}

TEST(MeshGeometryTest, SinglePolygonQuadNormalsUseFanTriangulation) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(4);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*posAttr)[2] = QVector3D(1.0f, 1.0f, 0.0f);
    (*posAttr)[3] = QVector3D(0.0f, 1.0f, 0.0f);
    mesh->addPolygon({0, 1, 2, 3});

    mesh->computeVertexNormals();

    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    ASSERT_EQ(normals->size(), 4);
    for (int index = 0; index < normals->size(); ++index) {
        EXPECT_FLOAT_EQ((*normals)[index].x(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[index].y(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[index].z(), 1.0f);
    }
}

TEST(MeshGeometryTest, SharedVertexNormalsUseAreaWeightedFaceContributions) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(5);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*posAttr)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    (*posAttr)[3] = QVector3D(0.0f, 2.0f, 0.0f);
    (*posAttr)[4] = QVector3D(0.0f, 0.0f, 1.0f);
    mesh->addPolygon({0, 1, 2});
    mesh->addPolygon({0, 3, 4});

    mesh->computeVertexNormals();

    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    const float inverseLength = 1.0f / std::sqrt(5.0f);
    EXPECT_NEAR((*normals)[0].x(), 2.0f * inverseLength, 1e-6f);
    EXPECT_FLOAT_EQ((*normals)[0].y(), 0.0f);
    EXPECT_NEAR((*normals)[0].z(), inverseLength, 1e-6f);
    EXPECT_FLOAT_EQ((*normals)[1].z(), 1.0f);
    EXPECT_FLOAT_EQ((*normals)[2].z(), 1.0f);
    EXPECT_FLOAT_EQ((*normals)[3].x(), 1.0f);
    EXPECT_FLOAT_EQ((*normals)[4].x(), 1.0f);
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

TEST(MeshGeometryTest, OpposingFaceContributionsCancelToFallbackNormals) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(3);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*posAttr)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    mesh->addPolygon({0, 1, 2});
    mesh->addPolygon({0, 2, 1});

    mesh->computeVertexNormals();

    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    ASSERT_EQ(normals->size(), 3);
    for (int index = 0; index < normals->size(); ++index) {
        EXPECT_EQ((*normals)[index], QVector3D(0.0f, 0.0f, 1.0f));
    }
}

TEST(MeshGeometryTest, NonFiniteTrianglePositionsProduceFiniteFallbackNormals) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(3);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(std::numeric_limits<float>::quiet_NaN(), 1.0f, 0.0f);
    (*posAttr)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    mesh->addPolygon({0, 1, 2});

    mesh->computeVertexNormals();

    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    ASSERT_EQ(normals->size(), 3);
    for (int index = 0; index < normals->size(); ++index) {
        EXPECT_TRUE(std::isfinite((*normals)[index].x()));
        EXPECT_TRUE(std::isfinite((*normals)[index].y()));
        EXPECT_TRUE(std::isfinite((*normals)[index].z()));
        EXPECT_EQ((*normals)[index], QVector3D(0.0f, 0.0f, 1.0f));
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

TEST(MeshGeometryTest, BoundsRecomputeAfterVertexPositionsChange) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(2);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(2.0f, 4.0f, 6.0f);
    mesh->updateBounds();
    ASSERT_EQ(mesh->boundingBoxMax(), QVector3D(2.0f, 4.0f, 6.0f));

    (*posAttr)[0] = QVector3D(-2.0f, 1.0f, 0.0f);
    (*posAttr)[1] = QVector3D(4.0f, 3.0f, 2.0f);
    mesh->updateBounds();

    EXPECT_EQ(mesh->boundingBoxMin(), QVector3D(-2.0f, 1.0f, 0.0f));
    EXPECT_EQ(mesh->boundingBoxMax(), QVector3D(4.0f, 3.0f, 2.0f));
    EXPECT_EQ(mesh->boundingSphereCenter(), QVector3D(1.0f, 2.0f, 1.0f));
    EXPECT_NEAR(mesh->boundingSphereRadius(), std::sqrt(11.0), 1.0e-5);
}

TEST(MeshGeometryTest, BoundsIgnoreNonFinitePositions)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(3);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(-2.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(std::numeric_limits<float>::quiet_NaN(), 50.0f, 0.0f);
    (*posAttr)[2] = QVector3D(4.0f, 0.0f, 0.0f);

    mesh->updateBounds();

    EXPECT_FLOAT_EQ(mesh->boundingBoxMin().x(), -2.0f);
    EXPECT_FLOAT_EQ(mesh->boundingBoxMax().x(), 4.0f);
    EXPECT_FLOAT_EQ(mesh->boundingBoxMin().y(), 0.0f);
    EXPECT_FLOAT_EQ(mesh->boundingBoxMax().y(), 0.0f);
    EXPECT_FLOAT_EQ(mesh->boundingSphereCenter().x(), 1.0f);
    EXPECT_FLOAT_EQ(mesh->boundingSphereRadius(), 3.0f);
}

TEST(MeshGeometryTest, BoundsRemainFiniteWhenEveryPositionIsNonFinite)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(3);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(std::numeric_limits<float>::quiet_NaN(), 1.0f, 2.0f);
    (*posAttr)[1] = QVector3D(std::numeric_limits<float>::infinity(), 3.0f, 4.0f);
    (*posAttr)[2] = QVector3D(5.0f, 6.0f,
                              -std::numeric_limits<float>::infinity());

    mesh->updateBounds();

    EXPECT_EQ(mesh->boundingBoxMin(), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(mesh->boundingBoxMax(), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(mesh->boundingSphereCenter(), QVector3D(0.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(mesh->boundingSphereRadius(), 0.0f);
}

TEST(MeshGeometryTest, NormalGenerationSkipsInvalidPolygonIndices)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(4);
    auto posAttr = mesh->vertexAttributes().add<QVector3D>("position");
    (*posAttr)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*posAttr)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*posAttr)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    (*posAttr)[3] = QVector3D(2.0f, 2.0f, 0.0f);
    mesh->addPolygon({0, 1, 9});
    mesh->addPolygon({0, 2, 1});

    mesh->computeVertexNormals();

    const auto normals = mesh->vertexAttributes().get<QVector3D>("normal");
    ASSERT_NE(normals, nullptr);
    for (int index = 0; index < 3; ++index) {
        EXPECT_FLOAT_EQ((*normals)[index].x(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[index].y(), 0.0f);
        EXPECT_FLOAT_EQ((*normals)[index].z(), -1.0f);
    }
    EXPECT_FLOAT_EQ((*normals)[3].z(), 1.0f);
}
