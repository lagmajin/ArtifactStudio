#include <gtest/gtest.h>

#include <QVector3D>

#include <array>
#include <cmath>
#include <limits>
#include <cstddef>

import Mesh;

using namespace ArtifactCore;

namespace {

Mesh makeGridMesh(const int cellsPerSide)
{
    Mesh mesh;
    const int verticesPerSide = cellsPerSide + 1;
    mesh.setVertexCount(verticesPerSide * verticesPerSide);
    auto positions = mesh.vertexAttributes().add<QVector3D>("position");
    for (int y = 0; y < verticesPerSide; ++y) {
        for (int x = 0; x < verticesPerSide; ++x) {
            const int index = y * verticesPerSide + x;
            (*positions)[index] = QVector3D(static_cast<float>(x),
                                            static_cast<float>(y), 0.0f);
        }
    }
    for (int y = 0; y < cellsPerSide; ++y) {
        for (int x = 0; x < cellsPerSide; ++x) {
            const int topLeft = y * verticesPerSide + x;
            mesh.addPolygon({topLeft, topLeft + 1,
                             topLeft + verticesPerSide + 1,
                             topLeft + verticesPerSide});
        }
    }
    return mesh;
}

Mesh::MeshletLODData makeLodData()
{
    auto mesh = makeGridMesh(8);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.maxTrianglesPerMeshlet = 8;
    config.lodSwitchPixels = 96.0f;
    return mesh.generateMeshletLODData(config);
}

} // namespace

TEST(MeshShaderLodStandaloneTest, GeneratesOrderedValidMeshletLevels)
{
    const auto lodData = makeLodData();
    ASSERT_TRUE(lodData.isValid());
    ASSERT_EQ(lodData.levels.size(), 4);

    int previousOffset = 0;
    int previousCount = std::numeric_limits<int>::max();
    float previousSwitchPixels = std::numeric_limits<float>::infinity();
    for (const auto& level : lodData.levels) {
        EXPECT_GE(level.meshletOffset, previousOffset);
        EXPECT_GT(level.meshletCount, 0u);
        EXPECT_LE(level.meshletOffset + level.meshletCount,
                  lodData.meshlets.size());
        EXPECT_LT(level.meshletCount, previousCount);
        EXPECT_LT(level.switchDistancePixels, previousSwitchPixels);
        previousOffset = level.meshletOffset + level.meshletCount;
        previousCount = level.meshletCount;
        previousSwitchPixels = level.switchDistancePixels;
    }
    EXPECT_EQ(previousOffset, lodData.meshlets.size());
}

TEST(MeshShaderLodStandaloneTest, SelectsLevelsAtProjectedRadiusBoundaries)
{
    const auto lodData = makeLodData();
    ASSERT_TRUE(lodData.isValid());
    ASSERT_EQ(lodData.levels.size(), 4);

    const float firstSwitch = lodData.levels[0].switchDistancePixels;
    const float secondSwitch = lodData.levels[1].switchDistancePixels;
    const float thirdSwitch = lodData.levels[2].switchDistancePixels;
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(lodData, firstSwitch), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  lodData, std::nextafter(firstSwitch, 0.0f)), 1);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(lodData, secondSwitch), 1);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  lodData, std::nextafter(secondSwitch, 0.0f)), 2);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(lodData, thirdSwitch), 2);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  lodData, std::nextafter(thirdSwitch, 0.0f)), 3);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  lodData, std::numeric_limits<float>::infinity()), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  lodData, std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(lodData, 0.0f), 3);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(lodData, -1.0f), 3);
}

TEST(MeshShaderLodStandaloneTest, InvalidAndEmptyMeshesDoNotProduceSelectableLevels)
{
    Mesh emptyMesh;
    const auto emptyLodData = emptyMesh.generateMeshletLODData();
    EXPECT_FALSE(emptyLodData.isValid());
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(emptyLodData, 32.0f), 0);

    auto mesh = makeGridMesh(2);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 1;
    const auto singleLevelData = mesh.generateMeshletLODData(config);
    ASSERT_TRUE(singleLevelData.isValid());
    ASSERT_EQ(singleLevelData.levels.size(), 1);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(singleLevelData, 32.0f), 0);
}
