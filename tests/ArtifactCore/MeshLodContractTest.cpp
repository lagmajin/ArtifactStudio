#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <QVector3D>
#include <QVector2D>
#include <QVector4D>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

import Memory.SharedPtr;
import Mesh;
import ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;

using namespace ArtifactCore;

namespace {

SharedPtr<Mesh> makeGridMesh(const int cellsPerSide) {
    auto mesh = makeShared<Mesh>();
    const int verticesPerSide = cellsPerSide + 1;
    mesh->setVertexCount(verticesPerSide * verticesPerSide);

    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    for (int y = 0; y < verticesPerSide; ++y) {
        for (int x = 0; x < verticesPerSide; ++x) {
            const int index = y * verticesPerSide + x;
            (*positions)[index] = QVector3D(
                static_cast<float>(x) / static_cast<float>(cellsPerSide),
                static_cast<float>(y) / static_cast<float>(cellsPerSide),
                0.0f);
        }
    }

    for (int y = 0; y < cellsPerSide; ++y) {
        for (int x = 0; x < cellsPerSide; ++x) {
            const int topLeft = y * verticesPerSide + x;
            const int topRight = topLeft + 1;
            const int bottomLeft = topLeft + verticesPerSide;
            const int bottomRight = bottomLeft + 1;
            mesh->addPolygon({topLeft, topRight, bottomRight});
            mesh->addPolygon({topLeft, bottomRight, bottomLeft});
        }
    }

    return mesh;
}

} // namespace

TEST(MeshLodContractTest, GeneratesValidMeshletRangesForEachLevel) {
    auto mesh = makeGridMesh(16);
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 24;
    config.maxLODLevels = 4;
    config.lodSwitchPixels = 96.0f;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), config.maxLODLevels);
    ASSERT_FALSE(data.renderData.positions.isEmpty());

    unsigned int expectedFirstIndex = 0;
    int expectedMeshletOffset = 0;
    for (int levelIndex = 0; levelIndex < data.levels.size(); ++levelIndex) {
        const Mesh::MeshletLODLevel& level = data.levels[levelIndex];
        ASSERT_EQ(level.level, levelIndex);
        EXPECT_EQ(level.firstIndex, expectedFirstIndex)
            << "LOD index ranges should be contiguous without gaps or overlap";
        EXPECT_EQ(level.meshletOffset, expectedMeshletOffset)
            << "LOD meshlet ranges should be contiguous without gaps or overlap";
        ASSERT_GT(level.meshletCount, 0);
        ASSERT_GT(level.indexCount, 0u);
        EXPECT_EQ(level.indexCount % 3u, 0u);
        EXPECT_LE(level.firstIndex + level.indexCount,
                  static_cast<unsigned int>(data.lodIndices.size()));
        EXPECT_LE(level.meshletOffset + level.meshletCount, data.meshlets.size());

        unsigned int meshletIndexCount = 0;
        unsigned int expectedMeshletFirstIndex = level.firstIndex;
        for (int meshletIndex = level.meshletOffset;
             meshletIndex < level.meshletOffset + level.meshletCount;
             ++meshletIndex) {
            const Mesh::Meshlet& meshlet = data.meshlets[meshletIndex];
            EXPECT_EQ(meshlet.firstIndex, expectedMeshletFirstIndex)
                << "meshlet index ranges within each LOD should be contiguous";
            EXPECT_GE(meshlet.firstIndex, level.firstIndex);
            EXPECT_LE(meshlet.firstIndex + meshlet.indexCount,
                      level.firstIndex + level.indexCount);
            EXPECT_LE(meshlet.sourceTriangleCount, config.maxTrianglesPerMeshlet);
            meshletIndexCount += meshlet.indexCount;
            expectedMeshletFirstIndex += meshlet.indexCount;

            for (unsigned int indexOffset = 0; indexOffset < meshlet.indexCount;
                 ++indexOffset) {
                EXPECT_LT(data.lodIndices[meshlet.firstIndex + indexOffset],
                          static_cast<unsigned int>(data.renderData.positions.size()));
            }
        }

        EXPECT_EQ(meshletIndexCount, level.indexCount);
        EXPECT_EQ(expectedMeshletFirstIndex, level.firstIndex + level.indexCount);
        expectedFirstIndex += level.indexCount;
        expectedMeshletOffset += level.meshletCount;
        if (levelIndex > 0) {
            EXPECT_LT(level.switchDistancePixels,
                      data.levels[levelIndex - 1].switchDistancePixels);
        }
    }
}

TEST(MeshLodContractTest, MeshletBoundsEncloseEveryVertexAtEveryLod) {
    auto mesh = makeGridMesh(17);
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 19;
    config.maxLODLevels = 5;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        for (int meshletIndex = level.meshletOffset;
             meshletIndex < level.meshletOffset + level.meshletCount;
             ++meshletIndex) {
            const Mesh::Meshlet& meshlet = data.meshlets[meshletIndex];
            ASSERT_GT(meshlet.indexCount, 0u);
            EXPECT_EQ(meshlet.indexCount % 3u, 0u);
            EXPECT_EQ(meshlet.sourceTriangleCount,
                      static_cast<int>(meshlet.indexCount / 3u) *
                          level.triangleStride);
            EXPECT_TRUE(std::isfinite(meshlet.boundsRadius));
            EXPECT_GE(meshlet.boundsRadius, 0.0f);
            EXPECT_LE(meshlet.boundsMin.x(), meshlet.boundsMax.x());
            EXPECT_LE(meshlet.boundsMin.y(), meshlet.boundsMax.y());
            EXPECT_LE(meshlet.boundsMin.z(), meshlet.boundsMax.z());

            const QVector3D expectedCenter =
                (meshlet.boundsMin + meshlet.boundsMax) * 0.5f;
            EXPECT_NEAR(meshlet.boundsCenter.x(), expectedCenter.x(), 1.0e-6f);
            EXPECT_NEAR(meshlet.boundsCenter.y(), expectedCenter.y(), 1.0e-6f);
            EXPECT_NEAR(meshlet.boundsCenter.z(), expectedCenter.z(), 1.0e-6f);

            const unsigned int end = meshlet.firstIndex + meshlet.indexCount;
            ASSERT_LE(end, static_cast<unsigned int>(data.lodIndices.size()));
            for (unsigned int indexOffset = meshlet.firstIndex;
                 indexOffset < end; ++indexOffset) {
                const unsigned int vertexIndex = data.lodIndices[indexOffset];
                ASSERT_LT(vertexIndex,
                          static_cast<unsigned int>(data.renderData.positions.size()));
                const QVector3D& position =
                    data.renderData.positions[static_cast<int>(vertexIndex)];
                EXPECT_GE(position.x(), meshlet.boundsMin.x() - 1.0e-6f);
                EXPECT_GE(position.y(), meshlet.boundsMin.y() - 1.0e-6f);
                EXPECT_GE(position.z(), meshlet.boundsMin.z() - 1.0e-6f);
                EXPECT_LE(position.x(), meshlet.boundsMax.x() + 1.0e-6f);
                EXPECT_LE(position.y(), meshlet.boundsMax.y() + 1.0e-6f);
                EXPECT_LE(position.z(), meshlet.boundsMax.z() + 1.0e-6f);
                EXPECT_LE((position - meshlet.boundsCenter).length(),
                          meshlet.boundsRadius + 1.0e-5f);
            }
        }
    }
}

TEST(MeshLodContractTest, SelectsCoarserLevelsAsProjectedRadiusShrinks) {
    auto mesh = makeGridMesh(16);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.lodSwitchPixels = 96.0f;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);
    ASSERT_GE(data.levels.size(), 2);

    EXPECT_EQ(data.levels.front().level,
              Mesh::chooseMeshletLODLevel(
                  data, std::numeric_limits<float>::max()));
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        EXPECT_EQ(level.level,
                  Mesh::chooseMeshletLODLevel(data, level.switchDistancePixels));
    }
    EXPECT_EQ(data.levels.back().level,
              Mesh::chooseMeshletLODLevel(data, 0.0f));
}

TEST(MeshLodContractTest, PerspectiveProjectedSizeSelectsExpectedDetailBands) {
    auto mesh = makeGridMesh(17);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.lodSwitchPixels = 96.0f;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);
    ASSERT_EQ(data.levels.size(), config.maxLODLevels);

    // Match ArtifactIRenderer's sphere projection: radius * projectionY *
    // viewportHeight / (2 * clipW), where clipW is camera-space distance.
    const auto projectedRadius = [](const float worldRadius,
                                    const float projectionScaleY,
                                    const float viewportHeight,
                                    const float clipW) {
        return worldRadius * std::abs(projectionScaleY) *
               std::max(1.0f, viewportHeight) /
               (2.0f * std::max(0.0001f, std::abs(clipW)));
    };

    struct ProjectionCase {
        float viewportHeight;
        float projectionScaleY;
        float clipW;
        int expectedLevel;
    };
    const std::array<ProjectionCase, 8> cases{{
        {1080.0f, 1.0f, 2.0f, 0},
        {1080.0f, 1.0f, 0.0f, 0},
        {1080.0f, 1.0f, 32.0f, 2},
        {2160.0f, 1.0f, 32.0f, 1},
        {1080.0f, 2.0f, 32.0f, 1},
        {1080.0f, 1.0f, 64.0f, 3},
        {0.0f, 1.0f, 32.0f, 3},
        {-1.0f, 1.0f, 32.0f, 3},
    }};
    for (const ProjectionCase& testCase : cases) {
        const float radiusPixels = projectedRadius(
            1.0f, testCase.projectionScaleY,
            testCase.viewportHeight, testCase.clipW);
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, radiusPixels),
                  testCase.expectedLevel)
            << "height=" << testCase.viewportHeight
            << " projectionY=" << testCase.projectionScaleY
            << " clipW=" << testCase.clipW
            << " projectedRadius=" << radiusPixels;
    }
}

TEST(MeshLodContractTest, CoarserLevelsReduceTriangleBudgetOnRepresentativeGrid) {
    auto mesh = makeGridMesh(32);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_EQ(data.levels.size(), config.maxLODLevels);
    ASSERT_GT(data.levels.front().indexCount, 0u);
    for (int levelIndex = 1; levelIndex < data.levels.size(); ++levelIndex) {
        EXPECT_LE(data.levels[levelIndex].indexCount,
                  data.levels[levelIndex - 1].indexCount);
        EXPECT_EQ(data.levels[levelIndex].indexCount % 3u, 0u);
    }
    EXPECT_LT(data.levels.back().indexCount,
              data.levels.front().indexCount)
        << "The coarsest LOD should submit fewer indices than the source level";
}

TEST(MeshLodContractTest, RespectsSmallRequestedMaximumLevelCount)
{
    auto mesh = makeGridMesh(32);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 2;
    config.lodSwitchPixels = 96.0f;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 2);
    EXPECT_EQ(data.levels[0].level, 0);
    EXPECT_EQ(data.levels[1].level, 1);
    EXPECT_EQ(data.levels[0].triangleStride, 1);
    EXPECT_EQ(data.levels[1].triangleStride, 2);
    EXPECT_FLOAT_EQ(data.levels[0].switchDistancePixels, 96.0f);
    EXPECT_FLOAT_EQ(data.levels[1].switchDistancePixels, 48.0f);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 96.0f), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 48.0f), 1);
    EXPECT_LE(data.levels[1].indexCount, data.levels[0].indexCount);
}

TEST(MeshLodContractTest, ExplicitSingleLevelKeepsTheCompleteFinestMesh)
{
    auto mesh = makeGridMesh(4);
    const Mesh::RenderData sourceRenderData = mesh->generateRenderData();
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 1;
    config.maxTrianglesPerMeshlet = 5;
    config.lodSwitchPixels = 72.0f;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 1);
    const Mesh::MeshletLODLevel& onlyLevel = data.levels.front();
    EXPECT_EQ(onlyLevel.level, 0);
    EXPECT_EQ(onlyLevel.triangleStride, 1);
    EXPECT_EQ(onlyLevel.indexCount,
              static_cast<unsigned int>(sourceRenderData.indices.size()));
    EXPECT_EQ(onlyLevel.switchDistancePixels, 72.0f);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 0.0f), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 10000.0f), 0);

    unsigned int meshletIndexCount = 0;
    for (int meshletIndex = onlyLevel.meshletOffset;
         meshletIndex < onlyLevel.meshletOffset + onlyLevel.meshletCount;
         ++meshletIndex) {
        const Mesh::Meshlet& meshlet = data.meshlets[meshletIndex];
        EXPECT_LE(meshlet.sourceTriangleCount, config.maxTrianglesPerMeshlet);
        meshletIndexCount += meshlet.indexCount;
    }
    EXPECT_EQ(meshletIndexCount, onlyLevel.indexCount);
}

TEST(MeshLodContractTest, EveryGeneratedLodKeepsNondegenerateConsistentWinding)
{
    auto mesh = makeGridMesh(32);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 5;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), config.maxLODLevels);
    const auto& positions = data.renderData.positions;
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        ASSERT_GT(level.indexCount, 0u);
        ASSERT_EQ(level.indexCount % 3u, 0u);
        ASSERT_LE(level.firstIndex + level.indexCount,
                  static_cast<unsigned int>(data.lodIndices.size()));

        for (unsigned int offset = level.firstIndex;
             offset < level.firstIndex + level.indexCount; offset += 3u) {
            const unsigned int indexA = data.lodIndices[offset];
            const unsigned int indexB = data.lodIndices[offset + 1u];
            const unsigned int indexC = data.lodIndices[offset + 2u];
            ASSERT_LT(indexA, static_cast<unsigned int>(positions.size()));
            ASSERT_LT(indexB, static_cast<unsigned int>(positions.size()));
            ASSERT_LT(indexC, static_cast<unsigned int>(positions.size()));
            EXPECT_NE(indexA, indexB);
            EXPECT_NE(indexB, indexC);
            EXPECT_NE(indexC, indexA);

            const QVector3D edgeA = positions[static_cast<qsizetype>(indexB)] -
                                    positions[static_cast<qsizetype>(indexA)];
            const QVector3D edgeB = positions[static_cast<qsizetype>(indexC)] -
                                    positions[static_cast<qsizetype>(indexA)];
            const QVector3D normal = QVector3D::crossProduct(edgeA, edgeB);
            EXPECT_GT(normal.lengthSquared(), 1.0e-12f)
                << "level=" << level.level << " triangleOffset=" << offset;
            EXPECT_GT(normal.z(), 0.0f)
                << "simplification must preserve source winding; level="
                << level.level << " triangleOffset=" << offset;
        }
    }
}

TEST(MeshLodContractTest,
     DegenerateSourceFacesKeepLODIndicesAndMeshletBoundsFinite)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(5);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    (*positions)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*positions)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*positions)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    (*positions)[3] = QVector3D(2.0f, 0.0f, 0.0f);
    (*positions)[4] = QVector3D(3.0f, 0.0f, 0.0f);
    mesh->addPolygon({0, 1, 2});
    mesh->addPolygon({1, 3, 4});

    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 1;
    config.maxLODLevels = 4;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.front().indexCount, 6u)
        << "the finest level should preserve both source faces, including "
           "the collinear face";
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        ASSERT_EQ(level.indexCount % 3u, 0u);
        ASSERT_LE(level.firstIndex + level.indexCount,
                  static_cast<unsigned int>(data.lodIndices.size()));
        for (unsigned int offset = level.firstIndex;
             offset < level.firstIndex + level.indexCount; ++offset) {
            EXPECT_LT(data.lodIndices[offset],
                      static_cast<unsigned int>(data.renderData.positions.size()))
                << "level=" << level.level;
        }
    }
    for (const Mesh::Meshlet& meshlet : data.meshlets) {
        EXPECT_TRUE(std::isfinite(meshlet.boundsMin.x()));
        EXPECT_TRUE(std::isfinite(meshlet.boundsMin.y()));
        EXPECT_TRUE(std::isfinite(meshlet.boundsMin.z()));
        EXPECT_TRUE(std::isfinite(meshlet.boundsMax.x()));
        EXPECT_TRUE(std::isfinite(meshlet.boundsMax.y()));
        EXPECT_TRUE(std::isfinite(meshlet.boundsMax.z()));
        EXPECT_TRUE(std::isfinite(meshlet.boundsRadius));
        EXPECT_GE(meshlet.boundsRadius, 0.0f);
        EXPECT_LE(meshlet.boundsMin.x(), meshlet.boundsMax.x());
        EXPECT_LE(meshlet.boundsMin.y(), meshlet.boundsMax.y());
        EXPECT_LE(meshlet.boundsMin.z(), meshlet.boundsMax.z());
    }
}

TEST(MeshLodContractTest,
     CurvedSurfaceKeepsGeometricFacesAlignedWithVertexNormalsAtEveryLod)
{
    constexpr int cellsPerSide = 24;
    constexpr int verticesPerSide = cellsPerSide + 1;
    constexpr float curvature = 0.015f;
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(verticesPerSide * verticesPerSide);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    auto normals = mesh->vertexAttributes().add<QVector3D>("normal");
    for (int y = 0; y < verticesPerSide; ++y) {
        for (int x = 0; x < verticesPerSide; ++x) {
            const int index = y * verticesPerSide + x;
            (*positions)[index] = QVector3D(
                static_cast<float>(x), static_cast<float>(y),
                curvature * static_cast<float>(x * y));
            (*normals)[index] = QVector3D(
                -curvature * static_cast<float>(y),
                -curvature * static_cast<float>(x), 1.0f).normalized();
        }
    }
    for (int y = 0; y < cellsPerSide; ++y) {
        for (int x = 0; x < cellsPerSide; ++x) {
            const int topLeft = y * verticesPerSide + x;
            const int topRight = topLeft + 1;
            const int bottomLeft = topLeft + verticesPerSide;
            const int bottomRight = bottomLeft + 1;
            mesh->addPolygon({topLeft, topRight, bottomRight});
            mesh->addPolygon({topLeft, bottomRight, bottomLeft});
        }
    }

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 5;
    config.maxTrianglesPerMeshlet = 32;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_GE(data.levels.size(), 2);
    ASSERT_EQ(data.renderData.normals.size(), data.renderData.positions.size());
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        ASSERT_GT(level.indexCount, 0u);
        for (unsigned int offset = level.firstIndex;
             offset < level.firstIndex + level.indexCount; offset += 3u) {
            const unsigned int indexA = data.lodIndices[offset];
            const unsigned int indexB = data.lodIndices[offset + 1u];
            const unsigned int indexC = data.lodIndices[offset + 2u];
            ASSERT_LT(indexA,
                      static_cast<unsigned int>(data.renderData.positions.size()));
            ASSERT_LT(indexB,
                      static_cast<unsigned int>(data.renderData.positions.size()));
            ASSERT_LT(indexC,
                      static_cast<unsigned int>(data.renderData.positions.size()));

            const QVector3D edgeA =
                data.renderData.positions[static_cast<qsizetype>(indexB)] -
                data.renderData.positions[static_cast<qsizetype>(indexA)];
            const QVector3D edgeB =
                data.renderData.positions[static_cast<qsizetype>(indexC)] -
                data.renderData.positions[static_cast<qsizetype>(indexA)];
            const QVector3D geometricNormal =
                QVector3D::crossProduct(edgeA, edgeB);
            const QVector3D averageVertexNormal =
                data.renderData.normals[static_cast<qsizetype>(indexA)] +
                data.renderData.normals[static_cast<qsizetype>(indexB)] +
                data.renderData.normals[static_cast<qsizetype>(indexC)];
            EXPECT_GT(geometricNormal.lengthSquared(), 1.0e-12f)
                << "LOD=" << level.level << " indexOffset=" << offset;
            EXPECT_GT(QVector3D::dotProduct(geometricNormal,
                                            averageVertexNormal), 0.0f)
                << "simplified face should keep the curved surface's normal "
                   "orientation; LOD="
                << level.level << " indexOffset=" << offset;
        }
    }
}

TEST(MeshLodContractTest, FinestLevelRetainsEverySourceTriangleExactlyOnce) {
    constexpr int cellsPerSide = 13;
    auto mesh = makeGridMesh(cellsPerSide);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.maxTrianglesPerMeshlet = 11;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_FALSE(data.levels.isEmpty());
    const Mesh::MeshletLODLevel& finest = data.levels.front();
    EXPECT_EQ(finest.level, 0);
    EXPECT_EQ(finest.triangleStride, 1);
    EXPECT_EQ(finest.indexCount,
              static_cast<unsigned int>(mesh->polygonCount() * 3));

    unsigned int representedSourceTriangles = 0;
    for (int meshletIndex = finest.meshletOffset;
         meshletIndex < finest.meshletOffset + finest.meshletCount;
         ++meshletIndex) {
        const Mesh::Meshlet& meshlet = data.meshlets[meshletIndex];
        EXPECT_EQ(meshlet.indexCount % 3u, 0u);
        EXPECT_LE(meshlet.sourceTriangleCount, config.maxTrianglesPerMeshlet);
        representedSourceTriangles += meshlet.sourceTriangleCount;
    }
    EXPECT_EQ(representedSourceTriangles,
              static_cast<unsigned int>(mesh->polygonCount()));
}

TEST(MeshLodContractTest, GenerationIsDeterministicAcrossInterleavedConfigurations) {
    auto mesh = makeGridMesh(9);
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 17;
    config.maxLODLevels = 4;
    config.lodSwitchPixels = 112.0f;
    Mesh::MeshletLODConfig alternateConfig = config;
    alternateConfig.maxTrianglesPerMeshlet = 31;
    alternateConfig.lodSwitchPixels = 64.0f;

    const Mesh::MeshletLODData first = mesh->generateMeshletLODData(config);
    const Mesh::MeshletLODData alternate =
        mesh->generateMeshletLODData(alternateConfig);
    const Mesh::MeshletLODData repeated = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(alternate.isValid());
    ASSERT_TRUE(repeated.isValid());
    ASSERT_FALSE(first.levels.isEmpty());
    ASSERT_FALSE(alternate.levels.isEmpty());
    EXPECT_NE(first.levels.front().switchDistancePixels,
              alternate.levels.front().switchDistancePixels);
    ASSERT_EQ(first.levels.size(), repeated.levels.size());
    ASSERT_EQ(first.meshlets.size(), repeated.meshlets.size());
    EXPECT_EQ(first.lodIndices, repeated.lodIndices);
    EXPECT_EQ(first.renderData.positions, repeated.renderData.positions);

    for (int index = 0; index < first.levels.size(); ++index) {
        const auto& before = first.levels[index];
        const auto& after = repeated.levels[index];
        EXPECT_EQ(before.level, after.level);
        EXPECT_EQ(before.meshletOffset, after.meshletOffset);
        EXPECT_EQ(before.meshletCount, after.meshletCount);
        EXPECT_EQ(before.firstIndex, after.firstIndex);
        EXPECT_EQ(before.indexCount, after.indexCount);
        EXPECT_EQ(before.triangleStride, after.triangleStride);
        EXPECT_FLOAT_EQ(before.switchDistancePixels,
                        after.switchDistancePixels);
    }
    for (int index = 0; index < first.meshlets.size(); ++index) {
        const auto& before = first.meshlets[index];
        const auto& after = repeated.meshlets[index];
        EXPECT_EQ(before.firstIndex, after.firstIndex);
        EXPECT_EQ(before.indexCount, after.indexCount);
        EXPECT_EQ(before.vertexOffset, after.vertexOffset);
        EXPECT_EQ(before.vertexCount, after.vertexCount);
        EXPECT_EQ(before.sourceTriangleCount, after.sourceTriangleCount);
        EXPECT_FLOAT_EQ(before.boundsRadius, after.boundsRadius);
    }
}

TEST(MeshLodContractTest, GenerationDoesNotMutateSourceMeshGeometry)
{
    auto mesh = makeGridMesh(7);
    const auto sourcePositions =
        mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(sourcePositions, nullptr);
    const QVector<QVector3D> positionsBefore = sourcePositions->data();
    const int vertexCountBefore = mesh->vertexCount();
    const int polygonCountBefore = mesh->polygonCount();

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 6;
    config.maxTrianglesPerMeshlet = 13;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    EXPECT_EQ(mesh->vertexCount(), vertexCountBefore);
    EXPECT_EQ(mesh->polygonCount(), polygonCountBefore);
    ASSERT_EQ(sourcePositions->size(), positionsBefore.size());
    for (int index = 0; index < positionsBefore.size(); ++index) {
        EXPECT_EQ((*sourcePositions)[index], positionsBefore[index]);
    }
}

TEST(MeshLodContractTest,
     RegenerationAfterVertexEditUsesUpdatedGeometryAndStaysDeterministic)
{
    auto mesh = makeGridMesh(8);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.maxTrianglesPerMeshlet = 12;

    const Mesh::MeshletLODData beforeEdit = mesh->generateMeshletLODData(config);
    ASSERT_TRUE(beforeEdit.isValid());
    ASSERT_FALSE(beforeEdit.renderData.positions.isEmpty());
    EXPECT_FLOAT_EQ(beforeEdit.renderData.positions.front().x(), 0.0f);

    auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    (*positions)[0].setX(7.0f);

    const Mesh::MeshletLODData afterEdit = mesh->generateMeshletLODData(config);
    const Mesh::MeshletLODData repeated = mesh->generateMeshletLODData(config);
    ASSERT_TRUE(afterEdit.isValid());
    ASSERT_TRUE(repeated.isValid());
    ASSERT_FALSE(afterEdit.renderData.positions.isEmpty());
    EXPECT_FLOAT_EQ(afterEdit.renderData.positions.front().x(), 7.0f)
        << "regenerated LOD data should use vertex attributes from the new "
           "mesh revision";
    EXPECT_EQ(afterEdit.renderData.positions, repeated.renderData.positions);
    EXPECT_EQ(afterEdit.lodIndices, repeated.lodIndices);
    ASSERT_EQ(afterEdit.levels.size(), repeated.levels.size());
    for (int index = 0; index < afterEdit.levels.size(); ++index) {
        EXPECT_EQ(afterEdit.levels[index].indexCount,
                  repeated.levels[index].indexCount);
        EXPECT_EQ(afterEdit.levels[index].triangleStride,
                  repeated.levels[index].triangleStride);
    }
    EXPECT_FLOAT_EQ(beforeEdit.renderData.positions.front().x(), 0.0f)
        << "previously returned LOD snapshots should remain independent of "
           "subsequent source edits";
}

TEST(MeshLodContractTest, MissingOptionalAttributesStayAbsentAndUseDefaultColor)
{
    auto mesh = makeGridMesh(3);
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData();

    ASSERT_TRUE(data.isValid());
    const auto& renderData = data.renderData;
    ASSERT_FALSE(renderData.positions.isEmpty());
    EXPECT_TRUE(renderData.normals.isEmpty());
    EXPECT_TRUE(renderData.tangents.isEmpty());
    EXPECT_TRUE(renderData.uvs.isEmpty());
    EXPECT_TRUE(renderData.uvs1.isEmpty());
    ASSERT_EQ(renderData.colors.size(), renderData.positions.size());
    for (const QVector4D& color : renderData.colors) {
        EXPECT_FLOAT_EQ(color.x(), 1.0f);
        EXPECT_FLOAT_EQ(color.y(), 1.0f);
        EXPECT_FLOAT_EQ(color.z(), 1.0f);
        EXPECT_FLOAT_EQ(color.w(), 1.0f);
    }
}

TEST(MeshLodContractTest, PreservesVertexAttributesAcrossOptimizedLodRemapping)
{
    auto mesh = makeGridMesh(6);
    auto positions = mesh->vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    auto normals = mesh->vertexAttributes().add<QVector3D>("normal");
    auto tangents = mesh->vertexAttributes().add<QVector4D>("tangent");
    auto uvs = mesh->vertexAttributes().add<QVector2D>("uv");
    auto uvs1 = mesh->vertexAttributes().add<QVector2D>("uv1");
    auto colors = mesh->vertexAttributes().add<QVector4D>("color");
    ASSERT_NE(normals, nullptr);
    ASSERT_NE(tangents, nullptr);
    ASSERT_NE(uvs, nullptr);
    ASSERT_NE(uvs1, nullptr);
    ASSERT_NE(colors, nullptr);
    for (int index = 0; index < mesh->vertexCount(); ++index) {
        const QVector3D position = (*positions)[index];
        (*normals)[index] = QVector3D(position.x(), position.y(), 1.0f);
        (*tangents)[index] = QVector4D(position.x(), position.y(), 0.25f, 1.0f);
        (*uvs)[index] = QVector2D(position.x(), position.y());
        (*uvs1)[index] = QVector2D(position.y(), position.x());
        (*colors)[index] = QVector4D(position.x(), position.y(),
                                     position.x() + position.y(), 1.0f);
    }

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.maxTrianglesPerMeshlet = 8;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_GE(data.levels.size(), 2);
    const auto& renderData = data.renderData;
    ASSERT_EQ(renderData.normals.size(), renderData.positions.size());
    ASSERT_EQ(renderData.tangents.size(), renderData.positions.size());
    ASSERT_EQ(renderData.uvs.size(), renderData.positions.size());
    ASSERT_EQ(renderData.uvs1.size(), renderData.positions.size());
    ASSERT_EQ(renderData.colors.size(), renderData.positions.size());
    for (qsizetype index = 0; index < renderData.positions.size(); ++index) {
        const QVector3D position = renderData.positions[index];
        EXPECT_EQ(renderData.normals[index], QVector3D(position.x(), position.y(), 1.0f));
        EXPECT_EQ(renderData.tangents[index], QVector4D(position.x(), position.y(), 0.25f, 1.0f));
        EXPECT_EQ(renderData.uvs[index], QVector2D(position.x(), position.y()));
        EXPECT_EQ(renderData.uvs1[index], QVector2D(position.y(), position.x()));
        EXPECT_EQ(renderData.colors[index],
                  QVector4D(position.x(), position.y(), position.x() + position.y(), 1.0f));
    }
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        for (unsigned int offset = level.firstIndex;
             offset < level.firstIndex + level.indexCount; ++offset) {
            EXPECT_LT(data.lodIndices[offset],
                      static_cast<unsigned int>(renderData.positions.size()));
        }
    }
}

TEST(MeshLodContractTest, SwitchBoundariesAreInclusiveAndMonotonic) {
    auto mesh = makeGridMesh(16);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    config.lodSwitchPixels = 96.0f;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);
    ASSERT_GE(data.levels.size(), 2);

    for (int levelIndex = 1; levelIndex < data.levels.size(); ++levelIndex) {
        const float boundary = data.levels[levelIndex].switchDistancePixels;
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, boundary), levelIndex);
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                      data, std::nextafter(boundary,
                                           std::numeric_limits<float>::infinity())),
                  levelIndex - 1);
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                      data, std::nextafter(boundary,
                                           -std::numeric_limits<float>::infinity())),
                  levelIndex);
        EXPECT_LE(data.levels[levelIndex].indexCount,
                  data.levels[levelIndex - 1].indexCount);
    }
}

TEST(MeshLodContractTest, NonFiniteAndNegativeProjectedRadiiHaveStableFallbacks) {
    auto mesh = makeGridMesh(16);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);
    ASSERT_FALSE(data.levels.isEmpty());

    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  data, std::numeric_limits<float>::infinity()),
              data.levels.front().level);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  data, std::numeric_limits<float>::quiet_NaN()),
              data.levels.front().level);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, -1.0f),
              data.levels.back().level);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  data, -std::numeric_limits<float>::infinity()),
              data.levels.back().level);
}

TEST(MeshLodContractTest, EmptyMeshHasNoLevelsAndUsesLevelZeroFallback) {
    auto mesh = makeShared<Mesh>();

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData();

    EXPECT_FALSE(data.isValid());
    EXPECT_TRUE(data.levels.isEmpty());
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 0.0f), 0);
}

TEST(MeshLodContractTest, EmptyLevelTableFallsBackForAnyProjectedRadius) {
    Mesh::MeshletLODData data;
    constexpr std::array<float, 5> projectedRadii{
        -std::numeric_limits<float>::infinity(), -1.0f, 0.0f,
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()};

    for (const float projectedRadius : projectedRadii) {
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, projectedRadius), 0);
    }
}

TEST(MeshLodContractTest, ChooserReturnsLevelIdentifiersFromSparseLevelTables) {
    Mesh::MeshletLODData data;
    data.levels.push_back({.level = 2, .switchDistancePixels = 64.0f});
    data.levels.push_back({.level = 5, .switchDistancePixels = 16.0f});
    data.levels.push_back({.level = 9, .switchDistancePixels = 4.0f});

    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 128.0f), 2);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 64.0f), 2);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 32.0f), 5);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 16.0f), 5);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 8.0f), 9);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 0.0f), 9);
}

TEST(MeshLodContractTest, MeshWithoutRenderablePolygonsHasNoLodLevels) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(2);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    (*positions)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*positions)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    mesh->addPolygon({0, 1});

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData();

    EXPECT_FALSE(data.isValid());
    EXPECT_TRUE(data.renderData.indices.isEmpty());
    EXPECT_TRUE(data.lodIndices.isEmpty());
    EXPECT_TRUE(data.meshlets.isEmpty());
    EXPECT_TRUE(data.levels.isEmpty());
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 0.0f), 0);
}

TEST(MeshLodContractTest, QuadSourcePolygonIsTriangulatedIntoFinestLod)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(4);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    (*positions)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*positions)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*positions)[2] = QVector3D(1.0f, 1.0f, 0.0f);
    (*positions)[3] = QVector3D(0.0f, 1.0f, 0.0f);
    mesh->addPolygon({0, 1, 2, 3});

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 1;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 1);
    EXPECT_EQ(data.renderData.indices.size(), 6);
    EXPECT_EQ(data.levels.front().indexCount, 6u);
    EXPECT_EQ(data.levels.front().triangleStride, 1);
    EXPECT_EQ(mesh->polygonCount(), 1);
    for (const unsigned int index : data.lodIndices) {
        EXPECT_LT(index, static_cast<unsigned int>(data.renderData.positions.size()));
    }
}

TEST(MeshLodContractTest, ConvexNgonSourcePolygonKeepsFanTriangulationInFinestLod)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(5);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    (*positions)[0] = QVector3D(0.0f, 1.0f, 0.0f);
    (*positions)[1] = QVector3D(-0.95f, 0.31f, 0.0f);
    (*positions)[2] = QVector3D(-0.59f, -0.81f, 0.0f);
    (*positions)[3] = QVector3D(0.59f, -0.81f, 0.0f);
    (*positions)[4] = QVector3D(0.95f, 0.31f, 0.0f);
    mesh->addPolygon({0, 1, 2, 3, 4});

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 1;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 1);
    const Mesh::MeshletLODLevel& finest = data.levels.front();
    EXPECT_EQ(data.renderData.indices.size(), 9);
    EXPECT_EQ(finest.indexCount, 9u);
    EXPECT_EQ(finest.triangleStride, 1);
    for (unsigned int offset = finest.firstIndex;
         offset < finest.firstIndex + finest.indexCount; offset += 3u) {
        const unsigned int indexA = data.lodIndices[offset];
        const unsigned int indexB = data.lodIndices[offset + 1u];
        const unsigned int indexC = data.lodIndices[offset + 2u];
        ASSERT_LT(indexA, static_cast<unsigned int>(data.renderData.positions.size()));
        ASSERT_LT(indexB, static_cast<unsigned int>(data.renderData.positions.size()));
        ASSERT_LT(indexC, static_cast<unsigned int>(data.renderData.positions.size()));
        const QVector3D edgeA = data.renderData.positions[static_cast<qsizetype>(indexB)] -
                                data.renderData.positions[static_cast<qsizetype>(indexA)];
        const QVector3D edgeB = data.renderData.positions[static_cast<qsizetype>(indexC)] -
                                data.renderData.positions[static_cast<qsizetype>(indexA)];
        EXPECT_GT(QVector3D::crossProduct(edgeA, edgeB).z(), 0.0f);
    }
}

TEST(MeshLodContractTest, InvalidGenerationLimitsClampToOneValidLevel) {
    auto mesh = makeGridMesh(8);
    constexpr std::array<int, 3> invalidLimits{0, -1, -64};
    for (const int invalidLimit : invalidLimits) {
        Mesh::MeshletLODConfig config;
        config.maxTrianglesPerMeshlet = invalidLimit;
        config.maxLODLevels = invalidLimit;

        const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

        ASSERT_TRUE(data.isValid()) << "invalid limit=" << invalidLimit;
        ASSERT_EQ(data.levels.size(), 1) << "invalid limit=" << invalidLimit;
        EXPECT_EQ(data.levels.front().level, 0);
        EXPECT_EQ(data.levels.front().triangleStride, 1);
        EXPECT_EQ(data.levels.front().indexCount,
                  static_cast<unsigned int>(data.renderData.indices.size()));
        for (const Mesh::Meshlet& meshlet : data.meshlets) {
            EXPECT_LE(meshlet.sourceTriangleCount, 1)
                << "invalid limit=" << invalidLimit;
        }
    }
}

TEST(MeshLodContractTest, OneTriangleMeshletLimitSplitsEveryLevelExactly)
{
    auto mesh = makeGridMesh(4);
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 1;
    config.maxLODLevels = 3;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), config.maxLODLevels);
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        ASSERT_EQ(level.indexCount % 3u, 0u);
        unsigned int representedIndexCount = 0;
        for (int meshletIndex = level.meshletOffset;
             meshletIndex < level.meshletOffset + level.meshletCount;
             ++meshletIndex) {
            const Mesh::Meshlet& meshlet = data.meshlets[meshletIndex];
            EXPECT_EQ(meshlet.indexCount, 3u);
            EXPECT_EQ(meshlet.indexCount % 3u, 0u);
            representedIndexCount += meshlet.indexCount;
        }
        EXPECT_EQ(representedIndexCount, level.indexCount);
        EXPECT_EQ(level.meshletCount, level.indexCount / 3u);
    }
}

TEST(MeshLodContractTest, InvalidSwitchPixelScalesFallBackToDefault) {
    auto mesh = makeGridMesh(8);
    const Mesh::MeshletLODData defaults = mesh->generateMeshletLODData();
    ASSERT_FALSE(defaults.levels.isEmpty());

    const std::array<float, 5> invalidScales{
        0.0f,
        -1.0f,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    for (const float invalidScale : invalidScales) {
        Mesh::MeshletLODConfig config;
        config.lodSwitchPixels = invalidScale;
        const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

        ASSERT_EQ(data.levels.size(), defaults.levels.size());
        for (int levelIndex = 0; levelIndex < data.levels.size(); ++levelIndex) {
            EXPECT_TRUE(std::isfinite(data.levels[levelIndex].switchDistancePixels));
            EXPECT_GT(data.levels[levelIndex].switchDistancePixels, 0.0f);
            EXPECT_FLOAT_EQ(data.levels[levelIndex].switchDistancePixels,
                            defaults.levels[levelIndex].switchDistancePixels);
        }
    }
}

TEST(MeshLodContractTest, LargestFiniteSwitchScaleKeepsEveryBoundaryFinite) {
    auto mesh = makeGridMesh(8);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 5;
    config.lodSwitchPixels = std::numeric_limits<float>::max();

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_GT(data.levels.size(), 1);
    float previousBoundary = std::numeric_limits<float>::infinity();
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        EXPECT_TRUE(std::isfinite(level.switchDistancePixels));
        EXPECT_GT(level.switchDistancePixels, 0.0f);
        EXPECT_LT(level.switchDistancePixels, previousBoundary);
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                      data, level.switchDistancePixels),
                  level.level);
        previousBoundary = level.switchDistancePixels;
    }
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                  data, std::numeric_limits<float>::max()),
              data.levels.front().level);
}

TEST(MeshLodContractTest,
     SmallestPositiveSwitchScaleKeepsEveryGeneratedBoundaryUsable) {
    auto mesh = makeGridMesh(8);
    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 5;
    config.lodSwitchPixels = std::numeric_limits<float>::denorm_min();

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_GT(data.levels.size(), 1);
    float previousBoundary = std::numeric_limits<float>::infinity();
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        EXPECT_TRUE(std::isfinite(level.switchDistancePixels));
        EXPECT_GT(level.switchDistancePixels, 0.0f);
        EXPECT_LT(level.switchDistancePixels, previousBoundary);
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                      data, level.switchDistancePixels),
                  level.level);
        previousBoundary = level.switchDistancePixels;
    }
}

TEST(MeshLodContractTest, StopsGeneratingLevelsWhenTriangleStridesAreExhausted) {
    auto mesh = makeGridMesh(1);
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 8;
    config.maxLODLevels = 64;
    config.lodSwitchPixels = 96.0f;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 2);
    EXPECT_EQ(data.levels[0].triangleStride, 1);
    EXPECT_EQ(data.levels[1].triangleStride, 2);
    EXPECT_FLOAT_EQ(data.levels[0].switchDistancePixels, 96.0f);
    EXPECT_FLOAT_EQ(data.levels[1].switchDistancePixels, 48.0f);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 96.0f), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 48.0f), 1);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 0.0f), 1);
}

TEST(MeshLodContractTest,
     NonPowerOfTwoTriangleCountGeneratesFinalOversizedStrideLevel) {
    constexpr int triangleCount = 7;
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(triangleCount * 3);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    for (int triangle = 0; triangle < triangleCount; ++triangle) {
        const int firstVertex = triangle * 3;
        const float x = static_cast<float>(triangle) * 2.0f;
        (*positions)[firstVertex] = QVector3D(x, 0.0f, 0.0f);
        (*positions)[firstVertex + 1] = QVector3D(x + 1.0f, 0.0f, 0.0f);
        (*positions)[firstVertex + 2] = QVector3D(x, 1.0f, 0.0f);
        mesh->addPolygon({firstVertex, firstVertex + 1, firstVertex + 2});
    }

    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 2;
    config.maxLODLevels = 16;
    config.lodSwitchPixels = 96.0f;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 4);
    constexpr std::array<int, 4> expectedStrides{1, 2, 4, 8};
    for (int levelIndex = 0; levelIndex < data.levels.size(); ++levelIndex) {
        const Mesh::MeshletLODLevel& level = data.levels[levelIndex];
        EXPECT_EQ(level.level, levelIndex);
        EXPECT_EQ(level.triangleStride, expectedStrides[levelIndex]);
        EXPECT_GT(level.indexCount, 0u);
        EXPECT_GT(level.meshletCount, 0);
        EXPECT_FLOAT_EQ(level.switchDistancePixels,
                        96.0f / static_cast<float>(expectedStrides[levelIndex]));
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(data,
                                               level.switchDistancePixels),
                  levelIndex);
    }
    EXPECT_EQ(data.levels.back().triangleStride, 8)
        << "the terminal stride can exceed a non-power-of-two source count";
}

TEST(MeshLodContractTest, SingleTriangleSourceProducesOneValidLevelForLargeLimit) {
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(3);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    (*positions)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*positions)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*positions)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    mesh->addPolygon({0, 1, 2});
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 0;
    config.maxLODLevels = 4096;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 1);
    EXPECT_EQ(data.levels.front().triangleStride, 1);
    EXPECT_EQ(data.levels.front().indexCount, 3u);
    EXPECT_EQ(data.levels.front().meshletCount, 1);
    EXPECT_EQ(data.meshlets.front().sourceTriangleCount, 1);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 0.0f), 0);
    EXPECT_EQ(Mesh::chooseMeshletLODLevel(data, 10000.0f), 0);
}

TEST(MeshLodContractTest, PartialFinalStrideKeepsEveryLodIndexInBounds) {
    auto mesh = makeGridMesh(2);
    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 3;
    config.maxLODLevels = 16;
    config.lodSwitchPixels = 80.0f;

    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_GE(data.levels.size(), 2);
    EXPECT_LT(data.levels.size(), config.maxLODLevels);
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        ASSERT_GT(level.indexCount, 0u);
        EXPECT_EQ(level.indexCount % 3u, 0u);
        EXPECT_LE(level.firstIndex + level.indexCount,
                  static_cast<unsigned int>(data.lodIndices.size()));
        for (unsigned int offset = level.firstIndex;
             offset < level.firstIndex + level.indexCount; ++offset) {
            EXPECT_LT(data.lodIndices[offset],
                      static_cast<unsigned int>(data.renderData.positions.size()));
        }
    }
}

TEST(MeshLodContractTest, SwitchPixelScaleAppliesUniformlyToEveryLevel) {
    auto mesh = makeGridMesh(8);
    Mesh::MeshletLODConfig baseConfig;
    baseConfig.maxLODLevels = 4;
    baseConfig.lodSwitchPixels = 80.0f;
    Mesh::MeshletLODConfig doubledConfig = baseConfig;
    doubledConfig.lodSwitchPixels = 160.0f;

    const Mesh::MeshletLODData baseData = mesh->generateMeshletLODData(baseConfig);
    const Mesh::MeshletLODData doubledData =
        mesh->generateMeshletLODData(doubledConfig);

    ASSERT_EQ(baseData.levels.size(), doubledData.levels.size());
    ASSERT_GT(baseData.levels.size(), 1);
    for (int levelIndex = 0; levelIndex < baseData.levels.size(); ++levelIndex) {
        EXPECT_FLOAT_EQ(doubledData.levels[levelIndex].switchDistancePixels,
                        baseData.levels[levelIndex].switchDistancePixels * 2.0f);
        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                      baseData, baseData.levels[levelIndex].switchDistancePixels),
                  Mesh::chooseMeshletLODLevel(
                      doubledData,
                      doubledData.levels[levelIndex].switchDistancePixels));
    }
}

TEST(MeshLodContractTest,
     ConfigurationMatrixKeepsEveryGeneratedLevelAndSwitchBoundaryValid) {
    constexpr std::array<int, 5> gridSizes{1, 2, 3, 7, 11};
    constexpr std::array<int, 4> meshletTriangleLimits{1, 3, 8, 31};
    constexpr std::array<int, 4> requestedLevelCounts{1, 2, 5, 12};
    constexpr std::array<float, 3> switchPixelScales{0.25f, 96.0f, 4096.0f};

    for (const int gridSize : gridSizes) {
        const auto mesh = makeGridMesh(gridSize);
        for (const int maxTriangles : meshletTriangleLimits) {
            for (const int requestedLevels : requestedLevelCounts) {
                for (const float switchPixels : switchPixelScales) {
                    SCOPED_TRACE(::testing::Message()
                        << "grid=" << gridSize
                        << " maxTriangles=" << maxTriangles
                        << " requestedLevels=" << requestedLevels
                        << " switchPixels=" << switchPixels);

                    Mesh::MeshletLODConfig config;
                    config.maxTrianglesPerMeshlet = maxTriangles;
                    config.maxLODLevels = requestedLevels;
                    config.lodSwitchPixels = switchPixels;
                    const Mesh::MeshletLODData data =
                        mesh->generateMeshletLODData(config);

                    ASSERT_TRUE(data.isValid());
                    ASSERT_GE(data.levels.size(), 1);
                    ASSERT_LE(data.levels.size(), requestedLevels);
                    unsigned int expectedFirstIndex = 0;
                    int expectedMeshletOffset = 0;
                    for (int levelIndex = 0;
                         levelIndex < data.levels.size(); ++levelIndex) {
                        const Mesh::MeshletLODLevel& level =
                            data.levels[levelIndex];
                        ASSERT_GT(level.indexCount, 0u);
                        ASSERT_EQ(level.indexCount % 3u, 0u);
                        EXPECT_EQ(level.level, levelIndex);
                        EXPECT_EQ(level.firstIndex, expectedFirstIndex);
                        EXPECT_EQ(level.meshletOffset, expectedMeshletOffset);
                        EXPECT_LE(level.firstIndex + level.indexCount,
                                  static_cast<unsigned int>(data.lodIndices.size()));
                        EXPECT_LE(level.meshletOffset + level.meshletCount,
                                  data.meshlets.size());
                        EXPECT_TRUE(std::isfinite(level.switchDistancePixels));
                        EXPECT_GT(level.switchDistancePixels, 0.0f);
                        EXPECT_EQ(Mesh::chooseMeshletLODLevel(
                                      data, level.switchDistancePixels),
                                  levelIndex);

                        unsigned int levelIndexCount = 0;
                        for (int meshletIndex = level.meshletOffset;
                             meshletIndex <
                                 level.meshletOffset + level.meshletCount;
                             ++meshletIndex) {
                            const Mesh::Meshlet& meshlet =
                                data.meshlets[meshletIndex];
                            ASSERT_GT(meshlet.indexCount, 0u);
                            EXPECT_EQ(meshlet.indexCount % 3u, 0u);
                            EXPECT_LE(meshlet.indexCount / 3u,
                                      static_cast<unsigned int>(maxTriangles));
                            EXPECT_EQ(meshlet.firstIndex,
                                      level.firstIndex + levelIndexCount);
                            levelIndexCount += meshlet.indexCount;
                        }
                        EXPECT_EQ(levelIndexCount, level.indexCount);
                        expectedFirstIndex += level.indexCount;
                        expectedMeshletOffset += level.meshletCount;
                        if (levelIndex > 0) {
                            EXPECT_LT(level.switchDistancePixels,
                                      data.levels[levelIndex - 1]
                                          .switchDistancePixels);
                        }
                    }
                    EXPECT_EQ(expectedFirstIndex,
                              static_cast<unsigned int>(data.lodIndices.size()));
                    EXPECT_EQ(expectedMeshletOffset, data.meshlets.size());
                }
            }
        }
    }
}

TEST(MeshLodContractTest, VertexAttributesStayAttachedToRemappedPositions) {
    constexpr int cellsPerSide = 12;
    auto mesh = makeGridMesh(cellsPerSide);
    const int verticesPerSide = cellsPerSide + 1;

    auto normals = mesh->vertexAttributes().add<QVector3D>("normal");
    auto tangents = mesh->vertexAttributes().add<QVector4D>("tangent");
    auto uvs = mesh->vertexAttributes().add<QVector2D>("uv");
    auto secondaryUvs = mesh->vertexAttributes().add<QVector2D>("uv1");
    auto colors = mesh->vertexAttributes().add<QVector4D>("color");
    for (int y = 0; y < verticesPerSide; ++y) {
        for (int x = 0; x < verticesPerSide; ++x) {
            const int index = y * verticesPerSide + x;
            const float u = static_cast<float>(x) / cellsPerSide;
            const float v = static_cast<float>(y) / cellsPerSide;
            (*normals)[index] = QVector3D(0.0f, 0.0f, 1.0f);
            (*tangents)[index] = QVector4D(1.0f, 0.0f, 0.0f, 1.0f);
            (*uvs)[index] = QVector2D(u, v);
            (*secondaryUvs)[index] = QVector2D(1.0f - u, 1.0f - v);
            (*colors)[index] = QVector4D(u, v, 0.25f, 1.0f);
        }
    }

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 4;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_FALSE(data.levels.isEmpty());
    const auto& renderData = data.renderData;
    ASSERT_EQ(renderData.positions.size(), renderData.normals.size());
    ASSERT_EQ(renderData.positions.size(), renderData.tangents.size());
    ASSERT_EQ(renderData.positions.size(), renderData.uvs.size());
    ASSERT_EQ(renderData.positions.size(), renderData.uvs1.size());
    ASSERT_EQ(renderData.positions.size(), renderData.colors.size());

    for (int vertexIndex = 0; vertexIndex < renderData.positions.size(); ++vertexIndex) {
        const QVector3D position = renderData.positions[vertexIndex];
        const QVector3D normal = renderData.normals[vertexIndex];
        const QVector4D tangent = renderData.tangents[vertexIndex];
        const QVector2D uv = renderData.uvs[vertexIndex];
        const QVector2D uv1 = renderData.uvs1[vertexIndex];
        const QVector4D color = renderData.colors[vertexIndex];

        EXPECT_FLOAT_EQ(position.z(), 0.0f);
        EXPECT_FLOAT_EQ(normal.x(), 0.0f);
        EXPECT_FLOAT_EQ(normal.y(), 0.0f);
        EXPECT_FLOAT_EQ(normal.z(), 1.0f);
        EXPECT_FLOAT_EQ(tangent.x(), 1.0f);
        EXPECT_FLOAT_EQ(tangent.y(), 0.0f);
        EXPECT_FLOAT_EQ(tangent.z(), 0.0f);
        EXPECT_FLOAT_EQ(tangent.w(), 1.0f);
        EXPECT_NEAR(uv.x(), position.x(), 1.0e-6f);
        EXPECT_NEAR(uv.y(), position.y(), 1.0e-6f);
        EXPECT_NEAR(uv1.x(), 1.0f - position.x(), 1.0e-6f);
        EXPECT_NEAR(uv1.y(), 1.0f - position.y(), 1.0e-6f);
        EXPECT_NEAR(color.x(), position.x(), 1.0e-6f);
        EXPECT_NEAR(color.y(), position.y(), 1.0e-6f);
        EXPECT_FLOAT_EQ(color.z(), 0.25f);
        EXPECT_FLOAT_EQ(color.w(), 1.0f);
    }

    for (const unsigned int index : data.lodIndices) {
        EXPECT_LT(index, static_cast<unsigned int>(renderData.positions.size()));
    }
}

TEST(MeshLodContractTest, VertexRemapPreservesUVSeamAtCoincidentPositions)
{
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(5);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");
    auto uvs = mesh->vertexAttributes().add<QVector2D>("uv");

    (*positions)[0] = QVector3D(0.0f, 0.0f, 0.0f);
    (*positions)[1] = QVector3D(1.0f, 0.0f, 0.0f);
    (*positions)[2] = QVector3D(0.0f, 1.0f, 0.0f);
    (*positions)[3] = QVector3D(1.0f, 0.0f, 0.0f);
    (*positions)[4] = QVector3D(1.0f, 1.0f, 0.0f);
    (*uvs)[0] = QVector2D(0.0f, 0.0f);
    (*uvs)[1] = QVector2D(1.0f, 0.0f);
    (*uvs)[2] = QVector2D(0.0f, 1.0f);
    (*uvs)[3] = QVector2D(0.0f, 0.0f);
    (*uvs)[4] = QVector2D(1.0f, 1.0f);
    mesh->addPolygon({0, 1, 2});
    mesh->addPolygon({3, 4, 2});

    Mesh::MeshletLODConfig config;
    config.maxLODLevels = 1;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), 1);
    const auto& renderData = data.renderData;
    ASSERT_EQ(renderData.uvs.size(), renderData.positions.size());
    int firstSeamVertex = -1;
    int secondSeamVertex = -1;
    for (int vertexIndex = 0; vertexIndex < renderData.positions.size(); ++vertexIndex) {
        const QVector3D& position = renderData.positions[vertexIndex];
        const QVector2D& uv = renderData.uvs[vertexIndex];
        if (position == QVector3D(1.0f, 0.0f, 0.0f) &&
            uv == QVector2D(1.0f, 0.0f)) {
            firstSeamVertex = vertexIndex;
        } else if (position == QVector3D(1.0f, 0.0f, 0.0f) &&
                   uv == QVector2D(0.0f, 0.0f)) {
            secondSeamVertex = vertexIndex;
        }
    }
    ASSERT_GE(firstSeamVertex, 0);
    ASSERT_GE(secondSeamVertex, 0);

    const Mesh::MeshletLODLevel& finest = data.levels.front();
    bool firstSeamReferenced = false;
    bool secondSeamReferenced = false;
    for (unsigned int offset = finest.firstIndex;
         offset < finest.firstIndex + finest.indexCount; ++offset) {
        const unsigned int index = data.lodIndices[offset];
        firstSeamReferenced |= index == static_cast<unsigned int>(firstSeamVertex);
        secondSeamReferenced |= index == static_cast<unsigned int>(secondSeamVertex);
    }
    EXPECT_TRUE(firstSeamReferenced);
    EXPECT_TRUE(secondSeamReferenced);
}

TEST(MeshLodContractTest,
     CoarserLevelsKeepEveryDisconnectedMeshIslandRepresented) {
    constexpr int islandCount = 3;
    constexpr int cellsPerSide = 4;
    constexpr int verticesPerSide = cellsPerSide + 1;
    auto mesh = makeShared<Mesh>();
    mesh->setVertexCount(islandCount * verticesPerSide * verticesPerSide);
    auto positions = mesh->vertexAttributes().add<QVector3D>("position");

    for (int island = 0; island < islandCount; ++island) {
        const int vertexOffset = island * verticesPerSide * verticesPerSide;
        const float islandOffsetX = static_cast<float>(island * 10);
        for (int y = 0; y < verticesPerSide; ++y) {
            for (int x = 0; x < verticesPerSide; ++x) {
                const int index = vertexOffset + y * verticesPerSide + x;
                (*positions)[index] = QVector3D(
                    islandOffsetX + static_cast<float>(x),
                    static_cast<float>(y), 0.0f);
            }
        }
        for (int y = 0; y < cellsPerSide; ++y) {
            for (int x = 0; x < cellsPerSide; ++x) {
                const int topLeft = vertexOffset + y * verticesPerSide + x;
                const int topRight = topLeft + 1;
                const int bottomLeft = topLeft + verticesPerSide;
                const int bottomRight = bottomLeft + 1;
                mesh->addPolygon({topLeft, topRight, bottomRight});
                mesh->addPolygon({topLeft, bottomRight, bottomLeft});
            }
        }
    }

    Mesh::MeshletLODConfig config;
    config.maxTrianglesPerMeshlet = 16;
    config.maxLODLevels = 4;
    const Mesh::MeshletLODData data = mesh->generateMeshletLODData(config);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.levels.size(), config.maxLODLevels);
    for (const Mesh::MeshletLODLevel& level : data.levels) {
        std::array<int, islandCount> trianglesPerIsland{};
        for (unsigned int offset = level.firstIndex;
             offset + 2u < level.firstIndex + level.indexCount;
             offset += 3u) {
            const QVector3D& a = data.renderData.positions[
                static_cast<qsizetype>(data.lodIndices[offset])];
            const QVector3D& b = data.renderData.positions[
                static_cast<qsizetype>(data.lodIndices[offset + 1u])];
            const QVector3D& c = data.renderData.positions[
                static_cast<qsizetype>(data.lodIndices[offset + 2u])];
            const float centerX = (a.x() + b.x() + c.x()) / 3.0f;
            const int island = static_cast<int>(centerX / 10.0f);
            ASSERT_GE(island, 0);
            ASSERT_LT(island, islandCount);
            ++trianglesPerIsland[static_cast<std::size_t>(island)];
        }
        for (int island = 0; island < islandCount; ++island) {
            EXPECT_GT(trianglesPerIsland[static_cast<std::size_t>(island)], 0)
                << "LOD level " << level.level
                << " should retain disconnected island " << island;
        }
    }
}

TEST(MeshLodContractTest,
     PuppetMeshDetailLevelsIncreaseTriangulationAndKeepAttributesAligned)
{
    cv::Mat source = cv::Mat::zeros(256, 256, CV_8UC4);
    cv::ellipse(source, cv::Point(128, 128), cv::Size(104, 88), 0.0,
                0.0, 360.0, cv::Scalar(220, 150, 80, 255), -1, cv::LINE_8);
    const cv::Mat original = source.clone();
    const std::array<int, 6> detailLevels{2, 4, 8, 16, 32, 64};
    std::size_t lowestTriangleCount = 0;
    std::size_t highestTriangleCount = 0;
    std::size_t previousTriangleCount = 0;
    PuppetMesh lowestDetailMesh;
    PuppetMesh highestDetailMesh;
    OpenCVPuppetEngine engine;

    for (const int detailLevel : detailLevels) {
        engine.bindImage(source, detailLevel);
        const PuppetMesh mesh = engine.getDeformedMesh();
        ASSERT_FALSE(mesh.vertices.empty()) << "detailLevel=" << detailLevel;
        ASSERT_FALSE(mesh.indices.empty()) << "detailLevel=" << detailLevel;
        EXPECT_EQ(mesh.vertices.size(), mesh.texCoords.size())
            << "detailLevel=" << detailLevel;
        EXPECT_EQ(mesh.vertices.size(), mesh.zDepth.size())
            << "detailLevel=" << detailLevel;
        ASSERT_EQ(mesh.indices.size() % 3, 0u)
            << "detailLevel=" << detailLevel;

        const std::size_t triangleCount = mesh.indices.size() / 3;
        EXPECT_GE(triangleCount, previousTriangleCount)
            << "increasing puppet detail should not reduce triangulation; detailLevel="
            << detailLevel;
        previousTriangleCount = triangleCount;
        if (detailLevel == detailLevels.front()) {
            lowestTriangleCount = triangleCount;
            lowestDetailMesh = mesh;
        }
        if (detailLevel == detailLevels.back()) {
            highestTriangleCount = triangleCount;
            highestDetailMesh = mesh;
        }

        for (const int index : mesh.indices) {
            EXPECT_GE(index, 0) << "detailLevel=" << detailLevel;
            EXPECT_LT(static_cast<std::size_t>(index), mesh.vertices.size())
                << "detailLevel=" << detailLevel;
        }
    }

    EXPECT_GT(highestTriangleCount, lowestTriangleCount)
        << "the highest puppet detail should produce a denser triangulation "
           "than the lowest detail";

    engine.bindImage(source, detailLevels.front());
    const PuppetMesh restoredLowestDetail = engine.getDeformedMesh();
    EXPECT_EQ(restoredLowestDetail.vertices.size(),
              lowestDetailMesh.vertices.size());
    EXPECT_EQ(restoredLowestDetail.indices, lowestDetailMesh.indices)
        << "switching back to Low should rebuild its original mesh topology";

    engine.bindImage(source, detailLevels.back());
    const PuppetMesh restoredHighestDetail = engine.getDeformedMesh();
    EXPECT_EQ(restoredHighestDetail.vertices.size(),
              highestDetailMesh.vertices.size());
    EXPECT_EQ(restoredHighestDetail.indices, highestDetailMesh.indices)
        << "switching back to High should rebuild its original mesh topology";

    EXPECT_EQ(cv::countNonZero(source.reshape(1) != original.reshape(1)), 0)
        << "binding detail levels must not modify the caller's source image";
}

TEST(MeshLodContractTest,
     PuppetMeshNonPositiveDetailUsesTheSameValidFallback)
{
    cv::Mat source = cv::Mat::zeros(128, 128, CV_8UC4);
    cv::ellipse(source, cv::Point(64, 64), cv::Size(52, 44), 0.0,
                0.0, 360.0, cv::Scalar(220, 150, 80, 255), -1, cv::LINE_8);

    OpenCVPuppetEngine zeroDetailEngine;
    zeroDetailEngine.bindImage(source, 0);
    const PuppetMesh zeroDetail = zeroDetailEngine.getDeformedMesh();
    ASSERT_FALSE(zeroDetail.vertices.empty());
    ASSERT_FALSE(zeroDetail.indices.empty());
    EXPECT_EQ(zeroDetail.indices.size() % 3, 0u);

    OpenCVPuppetEngine negativeDetailEngine;
    negativeDetailEngine.bindImage(source, -4);
    const PuppetMesh negativeDetail = negativeDetailEngine.getDeformedMesh();
    EXPECT_EQ(negativeDetail.vertices.size(), zeroDetail.vertices.size());
    EXPECT_EQ(negativeDetail.texCoords.size(), zeroDetail.texCoords.size());
    EXPECT_EQ(negativeDetail.zDepth.size(), zeroDetail.zDepth.size());
    EXPECT_EQ(negativeDetail.indices, zeroDetail.indices)
        << "nonpositive detail values should use the same fallback density";
}
