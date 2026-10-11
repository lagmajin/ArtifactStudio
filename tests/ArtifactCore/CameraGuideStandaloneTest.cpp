#include <gtest/gtest.h>

#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <vector>

import Geometry.CameraGuide;

using namespace ArtifactCore;

TEST(CameraGuideStandaloneTest, DefaultGuideHasExpectedPrimitiveCounts)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    EXPECT_EQ(guide.vertices.size(), 16u);
    EXPECT_EQ(guide.lineIndices.size(), 54u);
    EXPECT_EQ(guide.triangleIndices.size(), 36u);
}

TEST(CameraGuideStandaloneTest, DefaultBodyIsCenteredAndHasExpectedDepth)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    EXPECT_FLOAT_EQ(guide.vertices[0].x(), -0.275f);
    EXPECT_FLOAT_EQ(guide.vertices[1].x(), 0.275f);
    EXPECT_FLOAT_EQ(guide.vertices[0].y(), -0.18f);
    EXPECT_FLOAT_EQ(guide.vertices[3].y(), 0.18f);
    EXPECT_FLOAT_EQ(guide.vertices[0].z(), 0.0f);
    EXPECT_FLOAT_EQ(guide.vertices[4].z(), -0.28f);
}

TEST(CameraGuideStandaloneTest, NearAndFarFrustumPlanesUseConfiguredExtents)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    EXPECT_FLOAT_EQ(guide.vertices[8].x(), -0.21f);
    EXPECT_FLOAT_EQ(guide.vertices[9].x(), 0.21f);
    EXPECT_FLOAT_EQ(guide.vertices[8].y(), -0.14f);
    EXPECT_FLOAT_EQ(guide.vertices[11].y(), 0.14f);
    EXPECT_FLOAT_EQ(guide.vertices[8].z(), -0.36f);
    EXPECT_FLOAT_EQ(guide.vertices[12].x(), -0.475f);
    EXPECT_FLOAT_EQ(guide.vertices[14].y(), 0.32f);
    EXPECT_FLOAT_EQ(guide.vertices[12].z(), -1.45f);
}

TEST(CameraGuideStandaloneTest, BodyAndFrustumVerticesAreSymmetricAcrossCenterAxes)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    for (int first = 0; first < 4; ++first) {
        const int opposite = first ^ 1;
        EXPECT_FLOAT_EQ(guide.vertices[first].x(), -guide.vertices[opposite].x());
        EXPECT_FLOAT_EQ(guide.vertices[first].y(), guide.vertices[opposite].y());
    }
    for (int first = 8; first < 12; ++first) {
        const int opposite = first ^ 1;
        EXPECT_FLOAT_EQ(guide.vertices[first].x(), -guide.vertices[opposite].x());
        EXPECT_FLOAT_EQ(guide.vertices[first].y(), guide.vertices[opposite].y());
    }
}

TEST(CameraGuideStandaloneTest, EveryLineIndexReferencesAnExistingVertex)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    ASSERT_FALSE(guide.lineIndices.empty());
    EXPECT_EQ(guide.lineIndices.size() % 2u, 0u);
    for (const std::uint32_t index : guide.lineIndices) {
        EXPECT_LT(index, guide.vertices.size());
    }
}

TEST(CameraGuideStandaloneTest, EveryTriangleIndexReferencesAnExistingVertex)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    ASSERT_FALSE(guide.triangleIndices.empty());
    EXPECT_EQ(guide.triangleIndices.size() % 3u, 0u);
    for (const std::uint32_t index : guide.triangleIndices) {
        EXPECT_LT(index, guide.vertices.size());
    }
}

TEST(CameraGuideStandaloneTest, BodyDepthHasMinimumExtent)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive(
        0.55f, 0.36f, 0.0f);
    EXPECT_FLOAT_EQ(guide.vertices[4].z(), -0.01f);
    EXPECT_FLOAT_EQ(guide.vertices[7].z(), -0.01f);
}

TEST(CameraGuideStandaloneTest, CustomDimensionsChangeOnlyTheirCorrespondingExtents)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive(
        2.0f, 4.0f, 6.0f,
        8.0f, 10.0f,
        12.0f, 14.0f,
        -3.0f, -9.0f);
    EXPECT_FLOAT_EQ(guide.vertices[0].x(), -1.0f);
    EXPECT_FLOAT_EQ(guide.vertices[0].y(), -2.0f);
    EXPECT_FLOAT_EQ(guide.vertices[4].z(), -6.0f);
    EXPECT_FLOAT_EQ(guide.vertices[8].x(), -4.0f);
    EXPECT_FLOAT_EQ(guide.vertices[8].y(), -5.0f);
    EXPECT_FLOAT_EQ(guide.vertices[8].z(), -3.0f);
    EXPECT_FLOAT_EQ(guide.vertices[12].x(), -6.0f);
    EXPECT_FLOAT_EQ(guide.vertices[12].y(), -7.0f);
    EXPECT_FLOAT_EQ(guide.vertices[12].z(), -9.0f);
}

TEST(CameraGuideStandaloneTest, LineTopologyIncludesBodyToFrustumAndFrustumRails)
{
    const auto guide = makeNukeStyleCameraGuidePrimitive();
    const std::vector<std::uint32_t> expected{
        1, 9, 2, 10, 3, 11,
        8, 12, 9, 13, 10, 14, 11, 15};
    ASSERT_GE(guide.lineIndices.size(), expected.size());
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), guide.lineIndices.end() - expected.size()));
}
